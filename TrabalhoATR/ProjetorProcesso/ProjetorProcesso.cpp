#include <windows.h>
#include <iostream>

// Nomes dos eventos (devem ser idênticos aos do Controlador.h)
#define EVENT_STOP_T4   "Global\\StopEvent_Task4"
#define EVENT_PAUSE_T4  "Global\\PauseEvent_Task4"

int main()
{
    // Esta é a Tarefa 4 (Etapa 1)
    SetConsoleTitleA("PROJETOR 1 (PROCESSO) - ETAPA 1");
    std::cout << "Tarefa 4 (Exib. Processo) iniciada." << std::endl;

    // Abrir os eventos nomeados criados pelo Controlador
    HANDLE hStopEvent = OpenEventA(SYNCHRONIZE, FALSE, EVENT_STOP_T4);
    HANDLE hPauseEvent = OpenEventA(SYNCHRONIZE, FALSE, EVENT_PAUSE_T4);

    if (hStopEvent == NULL || hPauseEvent == NULL) {
        std::cout << "ERRO: Nao foi possivel abrir os eventos de controle." << std::endl;
        Sleep(5000);
        return 1;
    }

    HANDLE hEvents[] = { hStopEvent, hPauseEvent };
    bool bPaused = false;

    while (true) {
        // Espera por um evento (Stop ou Pause)
        // Usamos um timeout de 250ms para que o loop possa
        // imprimir o estado "ATIVO" repetidamente.
        DWORD waitResult = WaitForMultipleObjects(2, hEvents, FALSE, 250);

        switch (waitResult) {
        case WAIT_OBJECT_0: // --- STOP ---
            std::cout << "Recebido evento de TERMINO." << std::endl;
            goto cleanup;

        case WAIT_OBJECT_0 + 1: // --- PAUSE / RESUME ---
            bPaused = !bPaused; // Inverte o estado
            break;

        case WAIT_TIMEOUT: // --- NENHUM EVENTO ---
            // Nenhuma tecla foi pressionada, continue fazendo o que estava fazendo.
            break;
        }

        // Imprime o estado atual (requisito da Etapa 1)
        if (bPaused) {
            std::cout << "Estado: BLOQUEADO (Pausado)" << std::endl;
        }
        else {
            std::cout << "Estado: ATIVO" << std::endl;
        }

        // Limpa o console para atualizar o status (opcional, mas limpo)
        Sleep(1000); // Espera 1s
        system("cls");
    }

cleanup:
    std::cout << "Tarefa 4 Encerrando..." << std::endl;
    CloseHandle(hStopEvent);
    CloseHandle(hPauseEvent);
    Sleep(2000);
    return 0;
}