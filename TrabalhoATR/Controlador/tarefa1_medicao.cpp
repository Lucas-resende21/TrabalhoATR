#include "Controlador.h"

// Função auxiliar de log
void PrintToMainConsole(CRITICAL_SECTION* pCS, const std::string& msg) {
    EnterCriticalSection(pCS);
    std::cout << msg << std::endl;
    LeaveCriticalSection(pCS);
}

// --- Função da Thread ---
DWORD WINAPI MeasurementTask(LPVOID lpParam) {
    TaskParams* params = (TaskParams*)lpParam;
    bool bPaused = false; // "Chave" de pausa
    int nseq = 1;

    srand((unsigned int)time(NULL) ^ GetCurrentThreadId());
    PrintToMainConsole(params->pConsoleCS, "Tarefa 1 (Medicao): Iniciada.");

    while (true) {
        // --- 1. ESPERA DA PERIODICIDADE ---
        HANDLE hWaitTime[] = { params->hStopEvent, params->hPauseEvent };
        DWORD waitResult = WaitForMultipleObjects(2, hWaitTime, FALSE, (rand() % 4001) + 1000);

        if (waitResult == WAIT_OBJECT_0) {
            // --- STOP ---
            break;
        }
        else if (waitResult == WAIT_OBJECT_0 + 1) {
            // --- PAUSE / RESUME ---
            bPaused = !bPaused;
            if (bPaused) PrintToMainConsole(params->pConsoleCS, "Tarefa 1 (Medicao): Pausada.");
            else PrintToMainConsole(params->pConsoleCS, "Tarefa 1 (Medicao): Retomada.");
            continue; // Volta a esperar pelo timer
        }

        // --- 2. SE CHEGAMOS AQUI, O TIMER EXPIROU (WAIT_TIMEOUT) ---

        // Se estamos pausados (pelo evento anterior), não produzimos.
        if (bPaused) {
            continue;
        }

        // --- 3. GERAR MENSAGEM ---
        char message[LC1_MSG_SIZE];
        sprintf_s(message, LC1_MSG_SIZE, "MSG-T1 (Medicao) NSeq: %d", nseq);

        // --- 4. AGUARDAR POR ESPAÇO NO BUFFER (LC1) ---
        HANDLE hWaitBuffer[] = { params->hStopEvent, params->hPauseEvent, params->pLC1->hEmptySlots };

        if (WaitForSingleObject(params->pLC1->hEmptySlots, 0) == WAIT_TIMEOUT) {
            PrintToMainConsole(params->pConsoleCS, "Tarefa 1 (Medicao): LC1 cheia. Aguardando...");
        }

        waitResult = WaitForMultipleObjects(3, hWaitBuffer, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0) {
            // --- STOP ---
            break;
        }
        else if (waitResult == WAIT_OBJECT_0 + 1) {
            // --- PAUSE / RESUME ---
            bPaused = !bPaused;
            if (bPaused) PrintToMainConsole(params->pConsoleCS, "Tarefa 1 (Medicao): Pausada.");
            else PrintToMainConsole(params->pConsoleCS, "Tarefa 1 (Medicao): Retomada.");
            continue; // Volta a esperar pelo buffer (sem ter pego um slot)
        }

        // --- 5. SE CHEGAMOS AQUI, FOI UM SLOT (hEmptySlots) ---
        // (waitResult == WAIT_OBJECT_0 + 2)

        // Verificamos DE NOVO se fomos pausados ENQUANTO esperávamos
        if (bPaused) {
            // Devolvemos o slot que não vamos usar
            ReleaseSemaphore(params->pLC1->hEmptySlots, 1, NULL);
            continue; // Volta ao início do loop
        }

        // --- 6. ESCREVER NO BUFFER ---
        EnterCriticalSection(&params->pLC1->cs);
        {
            strcpy_s(params->pLC1->buffer[params->pLC1->write_index], LC1_MSG_SIZE, message);
            params->pLC1->write_index = (params->pLC1->write_index + 1) % LC1_CAPACITY;
        }
        LeaveCriticalSection(&params->pLC1->cs);
        ReleaseSemaphore(params->pLC1->hFullSlots, 1, NULL);

        std::stringstream ss_log;
        ss_log << "Tarefa 1 (Medicao): Depositou msg nseq " << nseq;
        PrintToMainConsole(params->pConsoleCS, ss_log.str());

        nseq = (nseq % 9999) + 1;
    }

    PrintToMainConsole(params->pConsoleCS, "Tarefa 1 (Medicao): Encerrando.");
    return 0;
}