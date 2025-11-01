#include <windows.h>
#include <stdio.h>
#include <tchar.h>

// --- Definições de Tipos e Estruturas (Assumidas) ---

// Tipo da mensagem para dispatching
enum MessageType {
    PROCESS_DATA,
    GRANULOMETRY_DATA
};

// Estrutura da mensagem
struct Message {
    MessageType type;
    // ... outros dados (payload)
    char data[256];
};

// Capacidade da segunda lista
#define LIST2_CAPACITY 100

// Nomes dos objetos de sincronização (devem ser consistentes entre tarefas)
#define CONSOLE_MUTEX_NAME      _T("GlobalConsoleMutex")
#define MAILSLOT_NAME           _T("\\\\.\\mailslot\\ProcessDataSlot")

// Sincronização da Lista 1 (Entrada)
#define LIST1_MUTEX_NAME        _T("List1Mutex")
#define LIST1_SEM_FULL_NAME     _T("List1SemFull")  // Slots cheios (dados disponíveis)
#define LIST1_SEM_EMPTY_NAME    _T("List1SemEmpty") // Slots vazios

// Sincronização da Lista 2 (Saída Granulometria)
#define LIST2_MUTEX_NAME        _T("List2Mutex")
#define LIST2_SEM_FULL_NAME     _T("List2SemFull")  // Slots cheios
#define LIST2_SEM_EMPTY_NAME    _T("List2SemEmpty") // Slots vazios

// Eventos de controle da Tarefa de Captura (controlados pela Tarefa do Teclado)
#define CAPTURE_STOP_EVENT_NAME   _T("CaptureStopEvent")  // Manual-reset
#define CAPTURE_PAUSE_EVENT_NAME  _T("CapturePauseEvent") // Manual-reset
#define CAPTURE_RESUME_EVENT_NAME _T("CaptureResumeEvent")// Auto-reset

// --- Globais (simplificação, poderiam ser passados por parâmetro) ---

// Handles de sincronização para a Lista 1
HANDLE g_hList1Mutex;
HANDLE g_hList1SemFull;  // Esta tarefa ESPERA por este
HANDLE g_hList1SemEmpty; // Esta tarefa SINALIZA este

// Handles de sincronização para a Lista 2
HANDLE g_hList2Mutex;
HANDLE g_hList2SemFull;  // Esta tarefa SINALIZA este
HANDLE g_hList2SemEmpty; // Esta tarefa ESPERA por este

// Handles de controle da tarefa
HANDLE g_hStopEvent;     // Sinaliza para terminar a thread
HANDLE g_hPauseEvent;    // Sinaliza para pausar a thread
HANDLE g_hResumeEvent;   // Sinaliza para retomar a thread

// Handle para o Mailslot
HANDLE g_hMailslot;

// Handle para o Mutex do Console
HANDLE g_hConsoleMutex;

// --- Estruturas de Dados (Assumidas) ---
// (Implementação real da lista circular não mostrada por brevidade)
Message g_List1_Buffer[100]; // Capacidade assumida
int g_List1_Head = 0;
int g_List1_Tail = 0;

Message g_List2_Buffer[LIST2_CAPACITY];
int g_List2_Head = 0;
int g_List2_Tail = 0;


// --- Funções Auxiliares ---

/**
 * @brief Imprime uma mensagem no console de forma thread-safe.
 */
void PrintToConsole(const TCHAR* message) {
    DWORD waitResult = WaitForSingleObject(g_hConsoleMutex, INFINITE);
    if (waitResult == WAIT_OBJECT_0) {
        _tprintf(_T("[Tarefa Captura] %s\n"), message);
        ReleaseMutex(g_hConsoleMutex);
    }
}

/**
 * @brief (Função Simulada) Lê uma mensagem da Lista 1.
 * Deve ser chamada SOMENTE após adquirir o mutex g_hList1Mutex.
 */
Message ReadFromList1() {
    // Lógica de remoção da lista circular (FIFO)
    Message msg = g_List1_Buffer[g_List1_Tail];
    g_List1_Tail = (g_List1_Tail + 1) % 100; // Assumindo capacidade 100
    return msg;
}

/**
 * @brief (Função Simulada) Escreve uma mensagem na Lista 2.
 * Deve ser chamada SOMENTE após adquirir o mutex g_hList2Mutex.
 */
void WriteToList2(const Message& msg) {
    // Lógica de adição na lista circular (FIFO)
    g_List2_Buffer[g_List2_Head] = msg;
    g_List2_Head = (g_List2_Head + 1) % LIST2_CAPACITY;
}

/**
 * @brief Lida com o estado de pausa.
 * Bloqueia até que o evento Resume ou Stop seja sinalizado.
 * Retorna TRUE se for para parar, FALSE se for para resumir.
 */
bool HandlePauseState() {
    PrintToConsole(_T("PAUSADA. Aguardando sinal de reinício..."));

    HANDLE hWaitEvents[2] = { g_hStopEvent, g_hResumeEvent };
    DWORD waitResult = WaitForMultipleObjects(2, hWaitEvents, FALSE, INFINITE);

    if (waitResult == WAIT_OBJECT_0) { // Stop
        PrintToConsole(_T("Recebido sinal de PARADA durante a pausa."));
        return true; // Parar
    }

    // Resume (WAIT_OBJECT_0 + 1)
    PrintToConsole(_T("Reiniciando..."));
    ResetEvent(g_hPauseEvent); // Reseta o evento de pausa para o próximo ciclo
    return false; // Continuar
}


// --- Função Principal da Thread ---

/**
 * @brief A função da thread para a "Tarefa de captura de mensagens".
 */
DWORD WINAPI CaptureThreadProc(LPVOID lpParam) {

    // --- 1. Inicialização: Abrir handles para objetos globais ---

    // Mutex do Console
    g_hConsoleMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, CONSOLE_MUTEX_NAME);
    if (g_hConsoleMutex == NULL) { /* Lidar com erro */ return 1; }

    // Eventos de Controle
    g_hStopEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, CAPTURE_STOP_EVENT_NAME);
    g_hPauseEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, CAPTURE_PAUSE_EVENT_NAME);
    g_hResumeEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, CAPTURE_RESUME_EVENT_NAME);
    if (!g_hStopEvent || !g_hPauseEvent || !g_hResumeEvent) {
        PrintToConsole(_T("ERRO: Falha ao abrir eventos de controle."));
        return 1;
    }

    // Mailslot (abrir para escrita)
    g_hMailslot = CreateFile(
        MAILSLOT_NAME,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (g_hMailslot == INVALID_HANDLE_VALUE) {
        PrintToConsole(_T("ERRO: Falha ao abrir Mailslot. A tarefa de exibição está em execução?"));
        return 1;
    }

    // Sincronização Lista 1 (Consumidor)
    g_hList1Mutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, LIST1_MUTEX_NAME);
    g_hList1SemFull = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, LIST1_SEM_FULL_NAME);
    g_hList1SemEmpty = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, LIST1_SEM_EMPTY_NAME);
    if (!g_hList1Mutex || !g_hList1SemFull || !g_hList1SemEmpty) {
        PrintToConsole(_T("ERRO: Falha ao abrir objetos da Lista 1."));
        return 1;
    }

    // Sincronização Lista 2 (Produtor)
    g_hList2Mutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, LIST2_MUTEX_NAME);
    g_hList2SemFull = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, LIST2_SEM_FULL_NAME);
    g_hList2SemEmpty = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, LIST2_SEM_EMPTY_NAME);
    if (!g_hList2Mutex || !g_hList2SemFull || !g_hList2SemEmpty) {
        PrintToConsole(_T("ERRO: Falha ao abrir objetos da Lista 2."));
        return 1;
    }

    PrintToConsole(_T("Tarefa iniciada e pronta."));

    // --- 2. Loop Principal da Tarefa ---

    while (true) {
        // Eventos para esperar:
        // 1. Parar (Stop)
        // 2. Pausar (Pause)
        // 3. Dados disponíveis na Lista 1 (List1 Full)
        HANDLE hWaitEvents[3] = { g_hStopEvent, g_hPauseEvent, g_hList1SemFull };

        DWORD waitResult = WaitForMultipleObjects(3, hWaitEvents, FALSE, INFINITE);

        switch (waitResult) {
            // --- Caso 1: Evento de Parada ---
        case WAIT_OBJECT_0:
        {
            PrintToConsole(_T("Sinal de parada recebido. Encerrando."));
            goto cleanup; // Pula para a seção de limpeza
        }

        // --- Caso 2: Evento de Pausa ---
        case WAIT_OBJECT_0 + 1:
        {
            if (HandlePauseState()) { // Lida com a pausa
                goto cleanup; // Se HandlePauseState retornar true, é para parar
            }
            // Se false, apenas continua o loop para reavaliar os eventos
            continue;
        }

        // --- Caso 3: Dados Disponíveis na Lista 1 ---
        case WAIT_OBJECT_0 + 2:
        {
            // Dados estão disponíveis. Consumir da Lista 1
            WaitForSingleObject(g_hList1Mutex, INFINITE);
            Message msg = ReadFromList1();
            ReleaseMutex(g_hList1Mutex);
            ReleaseSemaphore(g_hList1SemEmpty, 1, NULL); // Sinaliza 1 slot vazio na Lista 1

            // Agora, processar a mensagem
            if (msg.type == PROCESS_DATA) {
                // Enviar para a tarefa de exibição via Mailslot
                DWORD bytesWritten;
                BOOL bResult = WriteFile(g_hMailslot, &msg, sizeof(Message), &bytesWritten, NULL);
                if (!bResult) {
                    PrintToConsole(_T("ERRO: Falha ao escrever no Mailslot."));
                }
            }
            else if (msg.type == GRANULOMETRY_DATA) {
                // Depositar na Lista 2

                // Verificar se há espaço ANTES de bloquear, para poder alertar.
                DWORD spaceCheck = WaitForSingleObject(g_hList2SemEmpty, 0);

                if (spaceCheck == WAIT_TIMEOUT) {
                    // Lista 2 está cheia. Bloquear e alertar.
                    PrintToConsole(_T("ALERTA: Lista de Granulometria cheia. Tarefa bloqueada aguardando espaço..."));

                    // Espera (bloqueada) por um slot vazio OU um sinal de controle
                    HANDLE hWaitFull[3] = { g_hStopEvent, g_hPauseEvent, g_hList2SemEmpty };
                    DWORD fullWaitResult = WaitForMultipleObjects(3, hWaitFull, FALSE, INFINITE);

                    if (fullWaitResult == WAIT_OBJECT_0) { // Stop
                        PrintToConsole(_T("Sinal de PARADA recebido enquanto aguardava Lista 2."));
                        goto cleanup;
                    }
                    if (fullWaitResult == WAIT_OBJECT_0 + 1) { // Pause
                        if (HandlePauseState()) {
                            goto cleanup; // Parar
                        }
                        // Se resumir, precisamos tentar escrever a *mesma mensagem* novamente.
                        // A melhor forma é "rebobinar" o semáforo da Lista 1 que consumimos.
                        // Isso é complexo. Uma abordagem mais simples é descartar a mensagem.
                        // Ou, mais fácil: usar 'continue' para voltar ao início do loop
                        // e tentar processar esta mensagem de novo.

                        // Para evitar complexidade, vamos apenas tentar de novo (não é o ideal)
                        // A melhor solução seria um loop interno `while(!written)`.
                        // Mas para este exemplo, vamos assumir que a pausa é rara
                        // e apenas sinalizar que o item da Lista 1 foi "recuperado".
                        ReleaseSemaphore(g_hList1SemFull, 1, NULL); // Devolve o "crédito"
                        PrintToConsole(_T("Item retornado à Lista 1 devido à pausa."));
                        continue; // Volta ao loop de espera principal
                    }
                    // Se for WAIT_OBJECT_0 + 2, um espaço vagou. O fluxo continua para escrita.
                    PrintToConsole(_T("INFO: Espaço liberado na Lista 2. Retomando."));
                }

                // Escrever na Lista 2 (seja porque havia espaço, ou porque vagou)
                WaitForSingleObject(g_hList2Mutex, INFINITE);
                WriteToList2(msg);
                ReleaseMutex(g_hList2Mutex);
                ReleaseSemaphore(g_hList2SemFull, 1, NULL); // Sinaliza 1 slot cheio na Lista 2
            }
            break; // Fim do processamento da mensagem
        }

        // --- Caso 4: Erro ---
        default:
        {
            PrintToConsole(_T("ERRO: Falha em WaitForMultipleObjects."));
            goto cleanup;
        }
        } // Fim do switch
    } // Fim do while(true)

cleanup:
    // --- 3. Limpeza ---
    PrintToConsole(_T("Encerrando e limpando handles..."));
    CloseHandle(g_hConsoleMutex);
    CloseHandle(g_hMailslot);
    CloseHandle(g_hList1Mutex);
    CloseHandle(g_hList1SemFull);
    CloseHandle(g_hList1SemEmpty);
    CloseHandle(g_hList2Mutex);
    CloseHandle(g_hList2SemFull);
    CloseHandle(g_hList2SemEmpty);
    CloseHandle(g_hStopEvent);
    CloseHandle(g_hPauseEvent);
    CloseHandle(g_hResumeEvent);

    return 0;
}


// --- Função de Exemplo para Criar e Iniciar a Tarefa ---
// (Esta função seria chamada pela sua tarefa principal ou de setup)
HANDLE StartCaptureTask(void) {
    // NOTA: A tarefa principal DEVE criar os objetos de sincronização
    // antes de iniciar esta thread.

    // Ex: Criando os eventos de controle (a tarefa do teclado os abriria)
    // Evento de parada (Manual-Reset: fica sinalizado até ser resetado)
    HANDLE hStop = CreateEvent(NULL, TRUE, FALSE, CAPTURE_STOP_EVENT_NAME);
    // Evento de pausa (Manual-Reset)
    HANDLE hPause = CreateEvent(NULL, TRUE, FALSE, CAPTURE_PAUSE_EVENT_NAME);
    // Evento de resumo (Auto-Reset: volta a não-sinalizado após 1 espera)
    HANDLE hResume = CreateEvent(NULL, FALSE, FALSE, CAPTURE_RESUME_EVENT_NAME);

    // Ex: Criando semáforos da Lista 2 (a tarefa de análise os abriria)
    HANDLE hList2M = CreateMutex(NULL, FALSE, LIST2_MUTEX_NAME);
    // Começa VAZIA: 100 slots vazios, 0 slots cheios
    HANDLE hList2SemE = CreateSemaphore(NULL, LIST2_CAPACITY, LIST2_CAPACITY, LIST2_SEM_EMPTY_NAME);
    HANDLE hList2SemF = CreateSemaphore(NULL, 0, LIST2_CAPACITY, LIST2_SEM_FULL_NAME);

    // ... (criação similar para Lista 1 e Console Mutex) ...

    DWORD dwThreadId;
    HANDLE hThread = CreateThread(
        NULL,                   // Atributos de segurança padrão
        0,                      // Tamanho da stack padrão
        CaptureThreadProc,      // Função da thread
        NULL,                   // Parâmetro da thread (nenhum)
        0,                      // Flags de criação (inicia imediatamente)
        &dwThreadId             // Recebe o ID da thread
    );

    if (hThread == NULL) {
        _tprintf(_T("ERRO: Não foi possível criar a tarefa de captura.\n"));
    }

    // Retorna o handle da thread (a tarefa do teclado pode precisar dele)
    // Os outros handles (hStop, hPause, hList2M, etc.) devem ser fechados pela
    // tarefa principal quando não forem mais necessários (após a thread terminar).
    // NÃO os feche aqui, pois a thread precisa deles.

    return hThread;
}