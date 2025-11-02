#include "Tasks.h"

/**
 * @brief Gera uma mensagem de dados de processo (Tipo 44).
 */
void GenerateCLPMessage(char* msgBuffer, int nseq) {
    std::stringstream ss;
    SYSTEMTIME st;
    GetLocalTime(&st);

    // Gera valores aleatórios nas faixas especificadas
    int id_disco = (rand() % 6) + 1; // [1...6]
    double potencia = (double)rand() / RAND_MAX * 2.0; // [0...2]
    double vel = (double)rand() / RAND_MAX * 1000.0; // [0...1000]
    double incl = (double)rand() / RAND_MAX * 45.0; // [0...45]
    double vz_ent = (double)rand() / RAND_MAX * 1000.0; // [0...1000]
    double vz_saida = (double)rand() / RAND_MAX * 1000.0; // [0...1000]

    // Formato: "44/NNNN/NN/HH:MM:SS:mss/POT(5)/VEL(6)/INCL(4)/VZE(6)/VZS(6)" (55 chars)
    ss << "44/"
        << std::setfill('0') << std::setw(4) << nseq << "/"
        << std::setfill('0') << std::setw(2) << id_disco << "/"
        << std::setfill('0') << std::setw(2) << st.wHour << ":"
        << std::setfill('0') << std::setw(2) << st.wMinute << ":"
        << std::setfill('0') << std::setw(2) << st.wSecond << ":"
        << std::setfill('0') << std::setw(3) << st.wMilliseconds << "/" // Timestamp 12 chars (HH:MM:SS:mss)
        << std::fixed << std::setprecision(1) << std::setfill('0') << std::setw(5) << potencia << "/" // POTENCIA (5)
        << std::fixed << std::setprecision(1) << std::setfill('0') << std::setw(6) << vel << "/"      // VEL (6)
        << std::fixed << std::setprecision(1) << std::setfill('0') << std::setw(4) << incl << "/"       // INCL (4)
        << std::fixed << std::setprecision(1) << std::setfill('0') << std::setw(6) << vz_ent << "/"    // VZ ENT (6)
        << std::fixed << std::setprecision(1) << std::setfill('0') << std::setw(6) << vz_saida; // VZ SAIDA (6)

    std::string result = ss.str();
    result.resize(55, ' '); // Garante 55 chars

    strcpy_s(msgBuffer, LC1_MSG_SIZE, result.c_str()); // LC1_MSG_SIZE = 56
}

// --- TAREFA 2: Leitura de Dados do Processo (CLP) ---
DWORD WINAPI CLPTask(LPVOID lpParam) {
    ProducerTaskParams* params = (ProducerTaskParams*)lpParam;
    static bool bPaused = false;
    static int nseq = 1;

    // Timer periódico de 500ms
    HANDLE hWaitTimer = CreateWaitableTimer(NULL, FALSE, NULL); // Auto-reset
    if (hWaitTimer == NULL) {
        PrintToMainConsole(params->pConsoleCS, "Tarefa 2 (CLP): Erro ao criar WaitableTimer.");
        return 1;
    }

    LARGE_INTEGER liDueTime;
    liDueTime.QuadPart = -1; // Inicia quase imediatamente
    if (!SetWaitableTimer(hWaitTimer, &liDueTime, 500, NULL, NULL, 0)) { // 500ms
        PrintToMainConsole(params->pConsoleCS, "Tarefa 2 (CLP): Erro ao configurar timer periódico.");
        return 1;
    }

    srand((unsigned int)time(NULL) ^ GetCurrentThreadId());
    PrintToMainConsole(params->pConsoleCS, "Tarefa 2 (CLP): Iniciada.");

    while (true) {
        // --- 1. MANIPULAÇÃO DO ESTADO DE PAUSA ---
        if (bPaused) {
            PrintToMainConsole(params->pConsoleCS, "Tarefa 2 (CLP): Pausada.");
            CancelWaitableTimer(hWaitTimer); // Pausa o timer

            HANDLE hWaitResume[] = { params->hStopEvent, params->hPauseEvent };
            DWORD waitResult = WaitForMultipleObjects(2, hWaitResume, FALSE, INFINITE);

            if (waitResult == WAIT_OBJECT_0) break; // STOP
            if (waitResult == WAIT_OBJECT_0 + 1) { // RESUME
                bPaused = false;
                SetWaitableTimer(hWaitTimer, &liDueTime, 500, NULL, NULL, 0); // Retoma o timer
                PrintToMainConsole(params->pConsoleCS, "Tarefa 2 (CLP): Retomada.");
            }
        }

        // --- 2. ESPERA DA PERIODICIDADE (500 ms) ---
        HANDLE hWaitTime[] = { params->hStopEvent, params->hPauseEvent, hWaitTimer };
        DWORD waitResult = WaitForMultipleObjects(3, hWaitTime, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0) break; // STOP
        if (waitResult == WAIT_OBJECT_0 + 1) { // PAUSE
            bPaused = true;
            continue;
        }
        // Se for WAIT_OBJECT_0 + 2, o timer expirou.

        // --- 3. GERAR MENSAGEM ---
        char message[LC1_MSG_SIZE]; // 56
        GenerateCLPMessage(message, nseq);

        // --- 4. AGUARDAR POR ESPAÇO NA LISTA 1 ---
        HANDLE hWaitBuffer[] = { params->hStopEvent, params->hPauseEvent, params->pLC1->hEmptySlots };

        if (WaitForSingleObject(params->pLC1->hEmptySlots, 0) == WAIT_TIMEOUT) {
            PrintToMainConsole(params->pConsoleCS, "Tarefa 2 (CLP): LC1 cheia. Aguardando liberação...");
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
        ss_log << "Tarefa 2 (CLP): Depositou msg nseq " << nseq << " na LC1.";
        PrintToMainConsole(params->pConsoleCS, ss_log.str());

        // --- 6. INCREMENTAR NÚMERO SEQUENCIAL ---
        nseq = (nseq % 9999) + 1; // [1...9999]
    }

    CancelWaitableTimer(hWaitTimer);
    CloseHandle(hWaitTimer);
    PrintToMainConsole(params->pConsoleCS, "Tarefa 2 (CLP): Encerrando.");
    return 0;
}