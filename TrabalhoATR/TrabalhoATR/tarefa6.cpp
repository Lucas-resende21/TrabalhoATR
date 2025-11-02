#include "Tasks.h"

// --- TAREFA 6: Leitura do Teclado (Thread Principal) ---
/**
 * @brief Tarefa 6: Main() - Setup e Loop do Teclado.
 */
int main() {
    // --- 1. INICIALIZAÇÃO ---
    CircularBuffer1 g_LC1 = { 0 };
    CircularBuffer2 g_LC2 = { 0 };
    CRITICAL_SECTION g_csConsole; // Proteção do console principal

    HANDLE hPipeRead, hPipeWrite;

    // Handles das Threads
    HANDLE hThreads[5];

    // Handles de Eventos (5 pares de Stop/Pause + 1 Clear)
    HANDLE hStopEvents[5];   // [0]=T1, [1]=T2, [2]=T3, [3]=T4, [4]=T5
    HANDLE hPauseEvents[5];  // [0]=T1, [1]=T2, [2]=T3, [3]=T4, [4]=T5
    HANDLE hClearEvent;      // Para Task 4

    // Parâmetros das Threads
    ProducerTaskParams paramsT1, paramsT2;
    CaptureTaskParams paramsT3;
    DisplayProcessParams paramsT4;
    DisplayGranParams paramsT5;

    InitializeCriticalSection(&g_csConsole);
    PrintToMainConsole(&g_csConsole, "--- Sistema de Controle de Pelotização Iniciando ---");

    // --- 2. CRIAR OBJETOS DE SINCRONIZAÇÃO ---

    // Lista 1 (LC1)
    InitializeCriticalSection(&g_LC1.cs);
    g_LC1.hEmptySlots = CreateSemaphore(NULL, LC1_CAPACITY, LC1_CAPACITY, NULL);
    g_LC1.hFullSlots = CreateSemaphore(NULL, 0, LC1_CAPACITY, NULL);

    // Lista 2 (LC2)
    InitializeCriticalSection(&g_LC2.cs);
    g_LC2.hEmptySlots = CreateSemaphore(NULL, LC2_CAPACITY, LC2_CAPACITY, NULL);
    g_LC2.hFullSlots = CreateSemaphore(NULL, 0, LC2_CAPACITY, NULL);

    // IPC Pipe (Task 3 -> Task 4)
    if (!CreatePipe(&hPipeRead, &hPipeWrite, NULL, 0)) {
        PrintToMainConsole(&g_csConsole, "ERRO: Falha ao criar Pipe.");
        return 1;
    }

    // Eventos
    for (int i = 0; i < 5; i++) {
        hStopEvents[i] = CreateEvent(NULL, TRUE, FALSE, NULL);  // Manual-reset
        hPauseEvents[i] = CreateEvent(NULL, FALSE, FALSE, NULL); // Auto-reset (como na Task 1)
    }
    hClearEvent = CreateEvent(NULL, FALSE, FALSE, NULL); // Auto-reset

    // --- 3. PREPARAR PARÂMETROS E INICIAR THREADS ---

    // T1 (Medição)
    paramsT1.pLC1 = &g_LC1;
    paramsT1.hStopEvent = hStopEvents[0];
    paramsT1.hPauseEvent = hPauseEvents[0];
    paramsT1.pConsoleCS = &g_csConsole;
    hThreads[0] = CreateThread(NULL, 0, MeasurementTask, &paramsT1, 0, NULL);

    // T2 (CLP)
    paramsT2.pLC1 = &g_LC1;
    paramsT2.hStopEvent = hStopEvents[1];
    paramsT2.hPauseEvent = hPauseEvents[1];
    paramsT2.pConsoleCS = &g_csConsole;
    hThreads[1] = CreateThread(NULL, 0, CLPTask, &paramsT2, 0, NULL);

    // T3 (Captura)
    paramsT3.pLC1 = &g_LC1;
    paramsT3.pLC2 = &g_LC2;
    paramsT3.hPipeWrite = hPipeWrite;
    paramsT3.hStopEvent = hStopEvents[2];
    paramsT3.hPauseEvent = hPauseEvents[2];
    paramsT3.pConsoleCS = &g_csConsole;
    hThreads[2] = CreateThread(NULL, 0, CaptureTask, &paramsT3, 0, NULL);

    // T4 (Display Processo)
    paramsT4.hPipeRead = hPipeRead;
    paramsT4.hStopEvent = hStopEvents[3];
    paramsT4.hPauseEvent = hPauseEvents[3];
    paramsT4.hClearEvent = hClearEvent;
    hThreads[3] = CreateThread(NULL, 0, DisplayProcessTask, &paramsT4, 0, NULL);

    // T5 (Display Granulometria)
    paramsT5.pLC2 = &g_LC2;
    paramsT5.hStopEvent = hStopEvents[4];
    paramsT5.hPauseEvent = hPauseEvents[4];
    hThreads[4] = CreateThread(NULL, 0, DisplayGranTask, &paramsT5, 0, NULL);

    // --- 4. LOOP DA TAREFA 6 (TECLADO) ---
    PrintToMainConsole(&g_csConsole, "================================================");
    PrintToMainConsole(&g_csConsole, "Console Principal (Tarefa 6) Ativo.");
    PrintToMainConsole(&g_csConsole, " 'm': Pausar/Retomar Tarefa 1 (Medição)");
    PrintToMainConsole(&g_csConsole, " 'p': Pausar/Retomar Tarefa 2 (CLP)");
    PrintToMainConsole(&g_csConsole, " 'r': Pausar/Retomar Tarefa 3 (Captura)");
    PrintToMainConsole(&g_csConsole, " 'e': Pausar/Retomar Tarefa 4 (Exib. Processo)");
    PrintToMainConsole(&g_csConsole, " 'a': Pausar/Retomar Tarefa 5 (Exib. Análise)");
    PrintToMainConsole(&g_csConsole, " 'c': Limpar Tela da Tarefa 4 (Exib. Processo)");
    PrintToMainConsole(&g_csConsole, " 'ESC': Encerrar todas as tarefas");
    PrintToMainConsole(&g_csConsole, "================================================");

    while (true) {
        if (_kbhit()) {
            char c = _getch();

            switch (c) {
            case 'm': // Pausa/Retoma Medição
                PrintToMainConsole(&g_csConsole, "[Comando] Acionando Pausa/Retomada Tarefa 1.");
                SetEvent(hPauseEvents[0]);
                break;
            case 'p': // Pausa/Retoma CLP
                PrintToMainConsole(&g_csConsole, "[Comando] Acionando Pausa/Retomada Tarefa 2.");
                SetEvent(hPauseEvents[1]);
                break;
            case 'r': // Pausa/Retoma Captura
                PrintToMainConsole(&g_csConsole, "[Comando] Acionando Pausa/Retomada Tarefa 3.");
                SetEvent(hPauseEvents[2]);
                break;
            case 'e': // Pausa/Retoma Exibição Processo
                PrintToMainConsole(&g_csConsole, "[Comando] Acionando Pausa/Retomada Tarefa 4.");
                SetEvent(hPauseEvents[3]);
                break;
            case 'a': // Pausa/Retoma Exibição Análise
                PrintToMainConsole(&g_csConsole, "[Comando] Acionando Pausa/Retomada Tarefa 5.");
                SetEvent(hPauseEvents[4]);
                break;
            case 'c': // Limpa Tela Processo
                PrintToMainConsole(&g_csConsole, "[Comando] Acionando Limpeza Tarefa 4.");
                SetEvent(hClearEvent);
                break;
            case 27: // ESC - Sair
                PrintToMainConsole(&g_csConsole, "[Comando] ENCERRANDO SISTEMA...");
                for (int i = 0; i < 5; i++) {
                    SetEvent(hStopEvents[i]); // Sinaliza parada para todas
                }
                goto cleanup; // Pula para a seção de limpeza
            }
        }
        Sleep(100); // Evita 100% CPU no loop do teclado
    }

cleanup:
    // --- 5. LIMPEZA ---
    PrintToMainConsole(&g_csConsole, "Aguardando encerramento de todas as threads...");

    // Espera todas as 5 threads terminarem
    WaitForMultipleObjects(5, hThreads, TRUE, INFINITE);

    PrintToMainConsole(&g_csConsole, "Threads encerradas. Limpando recursos.");

    // Fecha handles
    for (int i = 0; i < 5; i++) {
        CloseHandle(hThreads[i]);
        CloseHandle(hStopEvents[i]);
        CloseHandle(hPauseEvents[i]);
    }
    CloseHandle(hClearEvent);
    CloseHandle(hPipeRead);
    CloseHandle(hPipeWrite);

    CloseHandle(g_LC1.hEmptySlots);
    CloseHandle(g_LC1.hFullSlots);
    DeleteCriticalSection(&g_LC1.cs);

    CloseHandle(g_LC2.hEmptySlots);
    CloseHandle(g_LC2.hFullSlots);
    DeleteCriticalSection(&g_LC2.cs);

    DeleteCriticalSection(&g_csConsole);

    std::cout << "Sistema encerrado. Pressione qualquer tecla para sair." << std::endl;
    _getch();
    return 0;
}