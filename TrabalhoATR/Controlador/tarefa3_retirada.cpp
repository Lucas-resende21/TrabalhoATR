#include "Controlador.h"

// --- Função da Thread ---
DWORD WINAPI CaptureTask(LPVOID lpParam) {
    TaskParams* params = (TaskParams*)lpParam;
    bool bPaused = false; // "Chave" de pausa
    char message[LC1_MSG_SIZE]; // Buffer local

    PrintToMainConsole(params->pConsoleCS, "Tarefa 3 (Retirada): Iniciada.");

    while (true) {
        // --- 1. AGUARDAR POR ITEM OU EVENTO ---
        HANDLE hWaitEvents[] = { params->hStopEvent, params->hPauseEvent, params->pLC1->hFullSlots };
        DWORD waitResult = WaitForMultipleObjects(3, hWaitEvents, FALSE, INFINITE);

        // --- 2. TRATAR EVENTOS DE CONTROLE PRIMEIRO ---
        if (waitResult == WAIT_OBJECT_0) {
            // --- STOP ---
            break;
        }
        else if (waitResult == WAIT_OBJECT_0 + 1) {
            // --- PAUSE / RESUME ---
            bPaused = !bPaused;
            if (bPaused) {
                PrintToMainConsole(params->pConsoleCS, "Tarefa 3 (Retirada): Pausada.");
            }
            else {
                PrintToMainConsole(params->pConsoleCS, "Tarefa 3 (Retirada): Retomada.");
            }

            // ***** ESTA É A CORREÇÃO *****
            // Nós NÃO usamos 'continue' aqui. 
            // Se um item estiver esperando (hFullSlots já estava sinalizado),
            // o loop while(true) irá rodar de novo e o próximo 
            // WaitForMultipleObjects retornará IMEDIATAMENTE com o item.
            // O 'continue' anterior causava o deadlock.
        }
        else if (waitResult == WAIT_OBJECT_0 + 2)
        {
            // --- 3. FOI UM ITEM (hFullSlots) ---

            // Agora, verificamos se estamos pausados ANTES de consumir
            if (bPaused) {
                // Estamos pausados, mas fomos acordados por um item.
                // Devemos devolver o item para a lista.
                ReleaseSemaphore(params->pLC1->hFullSlots, 1, NULL);
                // Volta a esperar. (Este continue é correto)
                continue;
            }

            // --- 4. NÃO ESTAMOS PAUSADOS: CONSUMIR O ITEM ---
            EnterCriticalSection(&params->pLC1->cs);
            {
                strcpy_s(message, LC1_MSG_SIZE, params->pLC1->buffer[params->pLC1->read_index]);
                params->pLC1->read_index = (params->pLC1->read_index + 1) % LC1_CAPACITY;
            }
            LeaveCriticalSection(&params->pLC1->cs);
            ReleaseSemaphore(params->pLC1->hEmptySlots, 1, NULL); // Sinaliza espaço vazio na LC1

            // Log
            std::stringstream ss_log;
            ss_log << "Tarefa 3 (Retirada): Consumiu msg [" << message << "]";
            PrintToMainConsole(params->pConsoleCS, ss_log.str());
        }
    } // Fim do while(true)

    PrintToMainConsole(params->pConsoleCS, "Tarefa 3 (Retirada): Encerrando.");
    return 0;
}