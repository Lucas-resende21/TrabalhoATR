#include "Tasks.h"

/**
 * @brief Gera uma mensagem de granulometria (Tipo 11).
 */
void GenerateMeasurementMessage(char* msgBuffer, int nseq) {
    std::stringstream ss;
    SYSTEMTIME st;
    GetLocalTime(&st);

    double gr_med = (double)rand() / RAND_MAX * 100.0;
    double gr_max = (double)rand() / RAND_MAX * 100.0;
    double gr_min = (double)rand() / RAND_MAX * 100.0;
    double sigma = (double)rand() / RAND_MAX * 100.0;
    int id_disco = (rand() % 2) + 1; // [1...2]

    // Formato: "11/NNNN/NN/HH:MM:SS/NNN.NN/NNN.NN/NNN.NN/NNN.NN" (47 chars)
    ss << "11/"
        << std::setfill('0') << std::setw(4) << nseq << "/"
        << std::setfill('0') << std::setw(2) << id_disco << "/"
        << std::setfill('0') << std::setw(2) << st.wHour << ":"
        << std::setfill('0') << std::setw(2) << st.wMinute << ":"
        << std::setfill('0') << std::setw(2) << st.wSecond << "/"
        << std::fixed << std::setprecision(2) << std::setfill('0') << std::setw(6) << gr_med << "/"
        << std::fixed << std::setprecision(2) << std::setfill('0') << std::setw(6) << gr_max << "/"
        << std::fixed << std::setprecision(2) << std::setfill('0') << std::setw(6) << gr_min << "/"
        << std::fixed << std::setprecision(2) << std::setfill('0') << std::setw(6) << sigma;

    strcpy_s(msgBuffer, LC2_MSG_SIZE, ss.str().c_str()); // LC2_MSG_SIZE = 48
}

// --- TAREFA 1: Leitura do Sistema de Medição ---
DWORD WINAPI MeasurementTask(LPVOID lpParam) {
    ProducerTaskParams* params = (ProducerTaskParams*)lpParam;
    static bool bPaused = false;
    static int nseq = 1;

    HANDLE hWaitTimer = CreateWaitableTimer(NULL, TRUE, NULL); // Reset manual
    if (hWaitTimer == NULL) {
        PrintToMainConsole(params->pConsoleCS, "Tarefa 1 (Medição): Erro ao criar WaitableTimer.");
        return 1;
    }
    srand((unsigned int)time(NULL) ^ GetCurrentThreadId());
    PrintToMainConsole(params->pConsoleCS, "Tarefa 1 (Medição): Iniciada.");

    while (true) {
        // --- 1. MANIPULAÇÃO DO ESTADO DE PAUSA ---
        if (bPaused) {
            PrintToMainConsole(params->pConsoleCS, "Tarefa 1 (Medição): Pausada.");
            HANDLE hWaitResume[] = { params->hStopEvent, params->hPauseEvent };
            DWORD waitResult = WaitForMultipleObjects(2, hWaitResume, FALSE, INFINITE);

            if (waitResult == WAIT_OBJECT_0) break; // STOP
            if (waitResult == WAIT_OBJECT_0 + 1) { // RESUME
                bPaused = false;
                PrintToMainConsole(params->pConsoleCS, "Tarefa 1 (Medição): Retomada.");
            }
        }

        // --- 2. ESPERA DA PERIODICIDADE (1-5 segundos) ---
        int waitTimeMs = (rand() % 4001) + 1000; // 1000 a 5000 ms
        LARGE_INTEGER liDueTime;
        liDueTime.QuadPart = -((LONGLONG)waitTimeMs * 10000); // relativo, 100ns

        SetWaitableTimer(hWaitTimer, &liDueTime, 0, NULL, NULL, 0);

        HANDLE hWaitTime[] = { params->hStopEvent, params->hPauseEvent, hWaitTimer };
        DWORD waitResult = WaitForMultipleObjects(3, hWaitTime, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0) break; // STOP
        if (waitResult == WAIT_OBJECT_0 + 1) { // PAUSE
            bPaused = true;
            CancelWaitableTimer(hWaitTimer); // Cancela o timer atual
            continue;
        }
        // Se for WAIT_OBJECT_0 + 2, o timer expirou.

        // --- 3. GERAR MENSAGEM ---
        char message[LC2_MSG_SIZE]; // 48
        GenerateMeasurementMessage(message, nseq);

        // --- 4. AGUARDAR POR ESPAÇO NA LISTA 1 ---
        HANDLE hWaitBuffer[] = { params->hStopEvent, params->hPauseEvent, params->pLC1->hEmptySlots };

        if (WaitForSingleObject(params->pLC1->hEmptySlots, 0) == WAIT_TIMEOUT) {
            PrintToMainConsole(params->pConsoleCS, "Tarefa 1 (Medição): LC1 cheia. Aguardando liberação...");
        }

        waitResult = WaitForMultipleObjects(3, hWaitBuffer, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0) break; // STOP
        if (waitResult == WAIT_OBJECT_0 + 1) { // PAUSE
            bPaused = true;
            continue;
        }
        // Se for WAIT_OBJECT_0 + 2, há espaço.

        // --- 5. ESCREVER NA LISTA 1 ---
        EnterCriticalSection(&params->pLC1->cs);
        {
            strcpy_s(params->pLC1->buffer[params->pLC1->write_index], LC1_MSG_SIZE, message);
            params->pLC1->write_index = (params->pLC1->write_index + 1) % LC1_CAPACITY;
        }
        LeaveCriticalSection(&params->pLC1->cs);
        ReleaseSemaphore(params->pLC1->hFullSlots, 1, NULL); // Sinaliza item cheio

        // Log
        std::stringstream ss_log;
        ss_log << "Tarefa 1 (Medição): Depositou msg nseq " << nseq << " na LC1.";
        PrintToMainConsole(params->pConsoleCS, ss_log.str());

        // --- 6. INCREMENTAR NÚMERO SEQUENCIAL ---
        nseq = (nseq % 9999) + 1; // [1...9999]
    }

    CloseHandle(hWaitTimer);
    PrintToMainConsole(params->pConsoleCS, "Tarefa 1 (Medição): Encerrando.");
    return 0;
}