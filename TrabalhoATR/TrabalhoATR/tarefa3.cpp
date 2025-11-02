#include "Tasks.h"

// --- TAREFA 3: Captura de Mensagens (Dispatcher) ---
DWORD WINAPI CaptureTask(LPVOID lpParam) {
    CaptureTaskParams* params = (CaptureTaskParams*)lpParam;
    static bool bPaused = false;
    char message[LC1_MSG_SIZE]; // Buffer local para a mensagem

    PrintToMainConsole(params->pConsoleCS, "Tarefa 3 (Captura): Iniciada.");

    while (true) {
        // --- 1. MANIPULAÇÃO DO ESTADO DE PAUSA ---
        if (bPaused) {
            PrintToMainConsole(params->pConsoleCS, "Tarefa 3 (Captura): Pausada.");
            HANDLE hWaitResume[] = { params->hStopEvent, params->hPauseEvent };
            DWORD waitResult = WaitForMultipleObjects(2, hWaitResume, FALSE, INFINITE);

            if (waitResult == WAIT_OBJECT_0) break; // STOP
            if (waitResult == WAIT_OBJECT_0 + 1) { // RESUME
                bPaused = false;
                PrintToMainConsole(params->pConsoleCS, "Tarefa 3 (Captura): Retomada.");
            }
        }

        // --- 2. AGUARDAR POR ITEM NA LISTA 1 ---
        HANDLE hWaitEvents[] = { params->hStopEvent, params->hPauseEvent, params->pLC1->hFullSlots };
        DWORD waitResult = WaitForMultipleObjects(3, hWaitEvents, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0) break; // STOP
        if (waitResult == WAIT_OBJECT_0 + 1) { // PAUSE
            bPaused = true;
            continue;
        }
        // Se for WAIT_OBJECT_0 + 2, há item na LC1.

        // --- 3. LER DA LISTA 1 (Consumidor) ---
        EnterCriticalSection(&params->pLC1->cs);
        {
            strcpy_s(message, LC1_MSG_SIZE, params->pLC1->buffer[params->pLC1->read_index]);
            params->pLC1->read_index = (params->pLC1->read_index + 1) % LC1_CAPACITY;
        }
        LeaveCriticalSection(&params->pLC1->cs);
        ReleaseSemaphore(params->pLC1->hEmptySlots, 1, NULL); // Sinaliza espaço vazio na LC1

        // --- 4. REDIRECIONAR MENSAGEM ---

        if (strncmp(message, "11", 2) == 0)
        {
            // TIPO 11: Enviar para Lista 2
            if (WaitForSingleObject(params->pLC2->hEmptySlots, 0) == WAIT_TIMEOUT) {
                PrintToMainConsole(params->pConsoleCS, "Tarefa 3 (Captura): LC2 cheia. Aguardando liberação...");
            }

            HANDLE hWaitLC2[] = { params->hStopEvent, params->hPauseEvent, params->pLC2->hEmptySlots };
            waitResult = WaitForMultipleObjects(3, hWaitLC2, FALSE, INFINITE);

            if (waitResult == WAIT_OBJECT_0) break; // STOP
            if (waitResult == WAIT_OBJECT_0 + 1) { // PAUSE
                bPaused = true;
                PrintToMainConsole(params->pConsoleCS, "Tarefa 3 (Captura): Mensagem '11' descartada devido a pausa.");
                continue;
            }
            // Se for WAIT_OBJECT_0 + 2, há espaço na LC2.

            EnterCriticalSection(&params->pLC2->cs);
            {
                strcpy_s(params->pLC2->buffer[params->pLC2->write_index], LC2_MSG_SIZE, message);
                params->pLC2->write_index = (params->pLC2->write_index + 1) % LC2_CAPACITY;
            }
            LeaveCriticalSection(&params->pLC2->cs);
            ReleaseSemaphore(params->pLC2->hFullSlots, 1, NULL); // Sinaliza item cheio na LC2

            PrintToMainConsole(params->pConsoleCS, "Tarefa 3 (Captura): Leu '11' da LC1 -> Enviou para LC2.");
        }
        else if (strncmp(message, "44", 2) == 0)
        {
            // TIPO 44: Enviar para Tarefa 4 via Pipe
            DWORD bytesWritten;
            BOOL bSuccess = WriteFile(
                params->hPipeWrite, // Handle do pipe
                message,            // Dados
                LC1_MSG_SIZE,       // Tamanho (enviar o buffer todo)
                &bytesWritten,      // Bytes escritos
                NULL                // Não sobreposto
            );

            if (!bSuccess) {
                PrintToMainConsole(params->pConsoleCS, "Tarefa 3 (Captura): ERRO ao escrever no Pipe.");
            }
            else {
                PrintToMainConsole(params->pConsoleCS, "Tarefa 3 (Captura): Leu '44' da LC1 -> Enviou para Pipe.");
            }
        }
    }

    PrintToMainConsole(params->pConsoleCS, "Tarefa 3 (Captura): Encerrando.");
    return 0;
}