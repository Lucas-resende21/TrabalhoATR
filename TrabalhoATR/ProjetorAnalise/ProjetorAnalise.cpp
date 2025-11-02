#include <windows.h>
#include <iostream>

// Nomes dos eventos (devem ser idênticos aos do Controlador.h)
#define EVENT_STOP_T5   "Global\\StopEvent_Task5"
#define EVENT_PAUSE_T5  "Global\\PauseEvent_Task5"

int main()
{
    // Esta é a Tarefa 5 (Etapa 1)
    SetConsoleTitleA("PROJETOR 2 (ANALISE) - ETAPA 1");
    std::cout << "Tarefa 5 (Analise Granulometria) iniciada." << std::endl;

    // Abrir os eventos nomeados criados pelo Controlador
    HANDLE hStopEvent = OpenEventA(SYNCHRONIZE, FALSE, EVENT_STOP_T5);
    HANDLE hPauseEvent = OpenEventA(SYNCHRONIZE, FALSE, EVENT_PAUSE_T5);

    if (hStopEvent == NULL || hPauseEvent == NULL) {
        std::cout << "ERRO: Nao foi possivel abrir os eventos de controle." << std::endl;
        Sleep(5000);
        return 1;
    }

    HANDLE hEvents[] = { hStopEvent, hPauseEvent };
    bool bPaused = false;

    while (true) {
        DWORD waitResult = WaitForMultipleObjects(2, hEvents, FALSE, 250);

        switch (waitResult) {
        case WAIT_OBJECT_0: // --- STOP ---
            std::cout << "Recebido evento de TERMINO." << std::endl;
            goto cleanup;

        case WAIT_OBJECT_0 + 1: // --- PAUSE / RESUME ---
            bPaused = !bPaused; // Inverte o estado
            break;

        case WAIT_TIMEOUT:
            break;
        }

        // Imprime o estado atual (requisito da Etapa 1)
        if (bPaused) {
            std::cout << "Estado: BLOQUEADO (Pausado)" << std::endl;
        }
        else {
            std::cout << "Estado: ATIVO" << std::endl;
        }

        Sleep(1000);
        system("cls");
    }

cleanup:
    std::cout << "Tarefa 5 Encerrando..." << std::endl;
    CloseHandle(hStopEvent);
    CloseHandle(hPauseEvent);
    Sleep(2000);
    return 0;
}