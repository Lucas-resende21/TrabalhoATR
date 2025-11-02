#include "Controlador.h"

// --- Globais do Processo Controlador ---
CircularBuffer1 g_LC1 = { 0 };
CRITICAL_SECTION g_csConsole; // Proteção do console principal

// --- Função para lançar os processos "Projetor" ---
void StartChildProcess(const char* processName) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // O CREATE_NEW_CONSOLE é a chave!
    if (!CreateProcessA(
        NULL,           // Nome do executável (vamos usar a linha de comando)
        (LPSTR)processName, // Linha de comando (ex: "ProjetorProcesso.exe")
        NULL,           // Segurança do Processo
        NULL,           // Segurança da Thread
        FALSE,          // Herança de Handles
        CREATE_NEW_CONSOLE, // Flags de Criação
        NULL,           // Variáveis de Ambiente
        NULL,           // Diretório Atual
        &si,            // STARTUPINFO
        &pi             // PROCESS_INFORMATION
    )) {
        printf("ERRO: CreateProcess falhou para %s. Codigo: %d\n", processName, GetLastError());
        return;
    }

    // Fechamos os handles do processo filho, pois não vamos esperar por ele
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

// --- TAREFA 6: Leitura do Teclado (Main) ---
int main() {
    SetConsoleTitleA("CONSOLE PRINCIPAL (TAREFAS 1, 2, 3, 6)");
    InitializeCriticalSection(&g_csConsole);
    PrintToMainConsole(&g_csConsole, "--- Sistema de Controle (Etapa 1) Iniciando ---");

    // --- 1. CRIAR OBJETOS DE SINCRONIZAÇÃO ---

    // Lista 1 (LC1) - Interna a este processo
    InitializeCriticalSection(&g_LC1.cs);
    g_LC1.hEmptySlots = CreateSemaphore(NULL, LC1_CAPACITY, LC1_CAPACITY, NULL);
    g_LC1.hFullSlots = CreateSemaphore(NULL, 0, LC1_CAPACITY, NULL);

    // Eventos para as threads internas (T1, T2, T3)
    HANDLE hStopEvents_Internal[3];
    HANDLE hPauseEvents_Internal[3];
    for (int i = 0; i < 3; i++) {
        hStopEvents_Internal[i] = CreateEvent(NULL, TRUE, FALSE, NULL);  // Manual
        hPauseEvents_Internal[i] = CreateEvent(NULL, FALSE, FALSE, NULL); // Auto
    }

    // Eventos para os processos externos (T4, T5) - DEVEM SER NOMEADOS
    HANDLE hStopEvent_T4 = CreateEventA(NULL, TRUE, FALSE, EVENT_STOP_T4);
    HANDLE hPauseEvent_T4 = CreateEventA(NULL, FALSE, FALSE, EVENT_PAUSE_T4);
    HANDLE hStopEvent_T5 = CreateEventA(NULL, TRUE, FALSE, EVENT_STOP_T5);
    HANDLE hPauseEvent_T5 = CreateEventA(NULL, FALSE, FALSE, EVENT_PAUSE_T5);

    if (!hStopEvent_T4 || !hPauseEvent_T4 || !hStopEvent_T5 || !hPauseEvent_T5) {
        PrintToMainConsole(&g_csConsole, "ERRO: Falha ao criar eventos nomeados.");
        return 1;
    }

    // --- 2. LANÇAR OS PROCESSOS FILHO (T4, T5) ---
    // NOTA: Isso assume que os .exe estão no mesmo diretório.
    // Configure as dependências de build na Solução do VS!
    // (Clique com o botão direito na Solução -> Propriedades -> Dependências do Projeto)
    // (Faça `Controlador` depender de `ProjetorProcesso` e `ProjetorAnalise`)
    PrintToMainConsole(&g_csConsole, "Iniciando processo ProjetorProcesso.exe (Tarefa 4)...");
    StartChildProcess("ProjetorProcesso.exe");

    PrintToMainConsole(&g_csConsole, "Iniciando processo ProjetorAnalise.exe (Tarefa 5)...");
    StartChildProcess("ProjetorAnalise.exe");

    Sleep(2000); // Dá um tempo para os processos filho abrirem

    // --- 3. PREPARAR PARÂMETROS E INICIAR THREADS (T1, T2, T3) ---
    TaskParams paramsT1, paramsT2, paramsT3;
    HANDLE hThreads[3];

    // T1 (Medição)
    paramsT1.pLC1 = &g_LC1;
    paramsT1.hStopEvent = hStopEvents_Internal[0];
    paramsT1.hPauseEvent = hPauseEvents_Internal[0];
    paramsT1.pConsoleCS = &g_csConsole;
    hThreads[0] = CreateThread(NULL, 0, MeasurementTask, &paramsT1, 0, NULL);

    // T2 (CLP)
    paramsT2.pLC1 = &g_LC1;
    paramsT2.hStopEvent = hStopEvents_Internal[1];
    paramsT2.hPauseEvent = hPauseEvents_Internal[1];
    paramsT2.pConsoleCS = &g_csConsole;
    hThreads[1] = CreateThread(NULL, 0, CLPTask, &paramsT2, 0, NULL);

    // T3 (Retirada)
    paramsT3.pLC1 = &g_LC1;
    paramsT3.hStopEvent = hStopEvents_Internal[2];
    paramsT3.hPauseEvent = hPauseEvents_Internal[2];
    paramsT3.pConsoleCS = &g_csConsole;
    hThreads[2] = CreateThread(NULL, 0, CaptureTask, &paramsT3, 0, NULL);

    // --- 4. LOOP DA TAREFA 6 (TECLADO) ---
    PrintToMainConsole(&g_csConsole, "================================================");
    PrintToMainConsole(&g_csConsole, "Console Principal (Tarefa 6) Ativo.");
    PrintToMainConsole(&g_csConsole, " 'm': Pausar/Retomar Tarefa 1 (Medição)");
    PrintToMainConsole(&g_csConsole, " 'p': Pausar/Retomar Tarefa 2 (CLP)");
    PrintToMainConsole(&g_csConsole, " 'r': Pausar/Retomar Tarefa 3 (Retirada)");
    PrintToMainConsole(&g_csConsole, " 'e': Pausar/Retomar Tarefa 4 (Exib. Processo)");
    PrintToMainConsole(&g_csConsole, " 'a': Pausar/Retomar Tarefa 5 (Exib. Análise)");
    PrintToMainConsole(&g_csConsole, " 'ESC': Encerrar todas as tarefas");
    PrintToMainConsole(&g_csConsole, "================================================");

    while (true) {
        if (_kbhit()) {
            char c = _getch();

            switch (c) {
                // Comandos para Threads Internas
            case 'm': SetEvent(hPauseEvents_Internal[0]); break;
            case 'p': SetEvent(hPauseEvents_Internal[1]); break;
            case 'r': SetEvent(hPauseEvents_Internal[2]); break;

                // Comandos para Processos Externos
            case 'e': SetEvent(hPauseEvent_T4); break;
            case 'a': SetEvent(hPauseEvent_T5); break;

                // Comando de Limpeza (Etapa 2)
            case 'c': PrintToMainConsole(&g_csConsole, "(Limpeza da T4 eh Etapa 2)"); break;

            case 27: // ESC - Sair
                PrintToMainConsole(&g_csConsole, "[Comando] ENCERRANDO SISTEMA...");
                // Sinaliza para threads internas
                for (int i = 0; i < 3; i++) {
                    SetEvent(hStopEvents_Internal[i]);
                }
                // Sinaliza para processos externos
                SetEvent(hStopEvent_T4);
                SetEvent(hStopEvent_T5);
                goto cleanup;
            }
        }
        Sleep(100);
    }

cleanup:
    PrintToMainConsole(&g_csConsole, "Aguardando encerramento das threads internas...");
    WaitForMultipleObjects(3, hThreads, TRUE, INFINITE);

    PrintToMainConsole(&g_csConsole, "Threads encerradas. Limpando recursos.");
    for (int i = 0; i < 3; i++) {
        CloseHandle(hThreads[i]);
        CloseHandle(hStopEvents_Internal[i]);
        CloseHandle(hPauseEvents_Internal[i]);
    }
    CloseHandle(hStopEvent_T4);
    CloseHandle(hPauseEvent_T4);
    CloseHandle(hStopEvent_T5);
    CloseHandle(hPauseEvent_T5);
    CloseHandle(g_LC1.hEmptySlots);
    CloseHandle(g_LC1.hFullSlots);
    DeleteCriticalSection(&g_LC1.cs);
    DeleteCriticalSection(&g_csConsole);

    std::cout << "Sistema encerrado." << std::endl;
    Sleep(2000);
    return 0;
}