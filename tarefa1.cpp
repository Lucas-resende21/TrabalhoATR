#include <windows.h>
#include <iostream>
#include <string>
#include <iomanip>  // Para std::setfill, std::setw
#include <sstream>  // Para std::stringstream
#include <ctime>    // Para time()
#include <cstdlib>  // Para rand()
#include <conio.h>  // Para _getch() e _kbhit()

// --- Constantes ---
#define BUFFER_SIZE 200
#define MESSAGE_SIZE 48 // 47 caracteres + 1 terminador nulo

// --- Estruturas de Dados ---

/**
 * @brief Define o buffer circular e seus objetos de sincronização.
 */
struct CircularBuffer {
    char buffer[BUFFER_SIZE][MESSAGE_SIZE];
    int write_index;
    int read_index;
    CRITICAL_SECTION csBuffer; // Protege o acesso aos índices e ao array
    HANDLE hEmptySlots;      // Semáforo: conta posições vazias (bloqueia produtor)
    HANDLE hFullSlots;       // Semáforo: conta posições cheias (bloqueia consumidor)
};

/**
 * @brief Parâmetros passados para a thread de medição.
 */
struct TaskParams {
    CircularBuffer* pBuffer;
    HANDLE hStopEvent;       // Evento para parar a thread
    HANDLE hPauseEvent;      // Evento para pausar/retomar a thread (toggle)
    CRITICAL_SECTION* pConsoleCS; // Protege o std::cout
};

// --- Funções Auxiliares ---

/**
 * @brief Imprime uma mensagem no console de forma thread-safe.
 * @param pCS Ponteiro para a Critical Section do console.
 * @param msg A mensagem a ser impressa.
 */
void PrintToConsole(CRITICAL_SECTION* pCS, const std::string& msg) {
    EnterCriticalSection(pCS);
    std::cout << msg << std::endl;
    LeaveCriticalSection(pCS);
}

/**
 * @brief Gera uma mensagem de granulometria formatada.
 * @param msgBuffer Buffer de destino (deve ter MESSAGE_SIZE).
 * @param nseq O número sequencial atual.
 */
void GenerateMessage(char* msgBuffer, int nseq) {
    std::stringstream ss;
    SYSTEMTIME st;
    GetLocalTime(&st);

    // Gera valores aleatórios nas faixas especificadas
    double gr_med = (double)rand() / RAND_MAX * 100.0;
    double gr_max = (double)rand() / RAND_MAX * 100.0;
    double gr_min = (double)rand() / RAND_MAX * 100.0;
    double sigma = (double)rand() / RAND_MAX * 100.0;
    int id_disco = (rand() % 2) + 1; // [1...2]

    // Formata a string de acordo com a especificação
    ss << "11/" // TIPO
        << std::setfill('0') << std::setw(4) << nseq << "/" // NSEQ
        << std::setfill('0') << std::setw(2) << id_disco << "/" // ID DISCO
        << std::setfill('0') << std::setw(2) << st.wHour << ":"   // TIMESTAMP
        << std::setfill('0') << std::setw(2) << st.wMinute << ":"
        << std::setfill('0') << std::setw(2) << st.wSecond << "/"
        << std::fixed << std::setprecision(2) << std::setfill('0') << std::setw(6) << gr_med << "/" // GR MED
        << std::fixed << std::setprecision(2) << std::setfill('0') << std::setw(6) << gr_max << "/" // GR MAX
        << std::fixed << std::setprecision(2) << std::setfill('0') << std::setw(6) << gr_min << "/" // GR MIN
        << std::fixed << std::setprecision(2) << std::setfill('0') << std::setw(6) << sigma;     // SIGMA

    // Copia a string formatada para o buffer de saída
    strcpy_s(msgBuffer, MESSAGE_SIZE, ss.str().c_str());
}

// --- Função da Thread ---

/**
 * @brief Tarefa de Leitura do Sistema de Medição.
 */
DWORD WINAPI MeasurementTask(LPVOID lpParam) {
    TaskParams* params = (TaskParams*)lpParam;

    // Estado da thread
    static bool bPaused = false; // Controla o estado de pausa
    static int nseq = 1;         // Número sequencial (começa em 1)

    // Cria um timer para a periodicidade (alternativa ao Sleep)
    HANDLE hWaitTimer = CreateWaitableTimer(NULL, TRUE, NULL); // Timer de reset manual
    if (hWaitTimer == NULL) {
        PrintToConsole(params->pConsoleCS, "Tarefa de Medição: Erro ao criar WaitableTimer.");
        return 1;
    }

    // Inicializa o gerador de números aleatórios para esta thread
    srand((unsigned int)time(NULL) ^ GetCurrentThreadId());

    PrintToConsole(params->pConsoleCS, "Tarefa de Medição: Iniciada.");

    while (true) {
        // --- 1. MANIPULAÇÃO DO ESTADO DE PAUSA ---
        if (bPaused) {
            PrintToConsole(params->pConsoleCS, "Tarefa de Medição: Pausada. Aguardando retomada...");
            // Bloqueia até receber um sinal de STOP ou PAUSE (retomada)
            HANDLE hWaitResume[] = { params->hStopEvent, params->hPauseEvent };
            DWORD waitResult = WaitForMultipleObjects(2, hWaitResume, FALSE, INFINITE);

            if (waitResult == WAIT_OBJECT_0) {
                break; // STOP
            }
            if (waitResult == WAIT_OBJECT_0 + 1) { // RESUME (toggle)
                bPaused = false;
                PrintToConsole(params->pConsoleCS, "Tarefa de Medição: Retomada.");
            }
        }

        // --- 2. ESPERA DA PERIODICIDADE (1-5 segundos) ---
        int waitTimeMs = (rand() % 4001) + 1000; // 1000 a 5000 ms
        LARGE_INTEGER liDueTime;
        liDueTime.QuadPart = -((LONGLONG)waitTimeMs * 10000); // Tempo relativo, em unidades de 100ns

        SetWaitableTimer(hWaitTimer, &liDueTime, 0, NULL, NULL, 0);

        // Aguarda pelo timer, ou por um evento de STOP ou PAUSE
        HANDLE hWaitTime[] = { params->hStopEvent, params->hPauseEvent, hWaitTimer };
        DWORD waitResult = WaitForMultipleObjects(3, hWaitTime, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0) break; // STOP
        if (waitResult == WAIT_OBJECT_0 + 1) { // PAUSE (toggle)
            bPaused = true;
            continue; // Volta ao início do loop para entrar no estado de pausa
        }
        // Se for WAIT_OBJECT_0 + 2, o timer expirou. Continua para gerar a msg.


        // --- 3. GERAR MENSAGEM ---
        char message[MESSAGE_SIZE];
        GenerateMessage(message, nseq); // Usa o nseq atual

        // --- 4. AGUARDAR POR ESPAÇO NO BUFFER ---
        HANDLE hWaitBuffer[] = { params->hStopEvent, params->hPauseEvent, params->pBuffer->hEmptySlots };

        // Verifica se o buffer está cheio ANTES de bloquear, para poder alertar
        if (WaitForSingleObject(params->pBuffer->hEmptySlots, 0) == WAIT_TIMEOUT) {
            PrintToConsole(params->pConsoleCS, "Tarefa de Medição: Buffer cheio. Aguardando liberação...");
        }

        // Bloqueia até que haja um slot vazio (ou receba STOP ou PAUSE)
        waitResult = WaitForMultipleObjects(3, hWaitBuffer, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0) break; // STOP
        if (waitResult == WAIT_OBJECT_0 + 1) { // PAUSE (toggle)
            bPaused = true;
            // A mensagem foi gerada mas não escrita.
            // Ela será descartada e gerada novamente (com o *mesmo* nseq)
            // após a tarefa ser retomada.
            continue;
        }
        // Se for WAIT_OBJECT_0 + 2, o semáforo hEmptySlots foi sinalizado (tem espaço).


        // --- 5. ESCREVER NO BUFFER ---
        // Temos um slot garantido pelo semáforo hEmptySlots
        EnterCriticalSection(&params->pBuffer->csBuffer);
        {
            strcpy_s(params->pBuffer->buffer[params->pBuffer->write_index], MESSAGE_SIZE, message);
            params->pBuffer->write_index = (params->pBuffer->write_index + 1) % BUFFER_SIZE;
        }
        LeaveCriticalSection(&params->pBuffer->csBuffer);

        // Sinaliza que uma posição foi preenchida (para o consumidor)
        ReleaseSemaphore(params->pBuffer->hFullSlots, 1, NULL);

        // Log da mensagem gerada
        std::stringstream ss_log;
        ss_log << "Tarefa de Medição: Gerou msg [" << message << "]";
        PrintToConsole(params->pConsoleCS, ss_log.str());


        // --- 6. INCREMENTAR NÚMERO SEQUENCIAL (APÓS SUCESSO) ---
        nseq++;
        if (nseq > 9999) {
            nseq = 0; // "voltando ao valor zero após alcançar a contagem máxima"
        }
    }

    CloseHandle(hWaitTimer);
    PrintToConsole(params->pConsoleCS, "Tarefa de Medição: Encerrando.");
    return 0;
}


// --- Função Principal (Tarefa de Leitura do Teclado) ---
int main() {
    CircularBuffer buffer = { 0 };
    TaskParams params = { 0 };
    CRITICAL_SECTION csConsole;

    // Inicializa a Critical Section do Console
    InitializeCriticalSection(&csConsole);

    // Inicializa o Buffer Circular
    buffer.write_index = 0;
    buffer.read_index = 0;
    InitializeCriticalSection(&buffer.csBuffer);

    // Cria semáforos:
    // hEmptySlots: Começa "cheio" (BUFFER_SIZE posições livres)
    // hFullSlots:  Começa "vazio" (0 posições ocupadas)
    buffer.hEmptySlots = CreateSemaphore(NULL, BUFFER_SIZE, BUFFER_SIZE, NULL);
    buffer.hFullSlots = CreateSemaphore(NULL, 0, BUFFER_SIZE, NULL);

    if (buffer.hEmptySlots == NULL || buffer.hFullSlots == NULL) {
        PrintToConsole(&csConsole, "Erro ao criar semáforos.");
        return 1;
    }

    // Inicializa Eventos de Sincronização
    HANDLE hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);  // Reset Manual, não sinalizado
    HANDLE hPauseEvent = CreateEvent(NULL, FALSE, FALSE, NULL); // Reset Automático, não sinalizado

    if (hStopEvent == NULL || hPauseEvent == NULL) {
        PrintToConsole(&csConsole, "Erro ao criar eventos.");
        return 1;
    }

    // Prepara os parâmetros para a thread
    params.pBuffer = &buffer;
    params.hStopEvent = hStopEvent;
    params.hPauseEvent = hPauseEvent;
    params.pConsoleCS = &csConsole;

    // Inicia a Tarefa de Medição
    HANDLE hThread = CreateThread(NULL, 0, MeasurementTask, &params, 0, NULL);
    if (hThread == NULL) {
        PrintToConsole(&csConsole, "Erro ao criar a thread de medição.");
        return 1;
    }

    // Loop da thread principal (Tarefa de Leitura do Teclado)
    PrintToConsole(&csConsole, "================================================");
    PrintToConsole(&csConsole, "Tarefa Principal (Leitura de Teclado) Iniciada.");
    PrintToConsole(&csConsole, "Pressione 'P' para Pausar/Retomar a tarefa.");
    PrintToConsole(&csConsole, "Pressione 'S' para Parar a aplicação.");
    PrintToConsole(&csConsole, "================================================");

    char c;
    while (true) {
        if (_kbhit()) { // Verifica se uma tecla foi pressionada (não bloqueante)
            c = _getch(); // Lê a tecla

            if (c == 'p' || c == 'P') {
                PrintToConsole(&csConsole, "[Comando Principal]: Pausar/Retomar recebido.");
                SetEvent(hPauseEvent); // Sinaliza o evento de toggle
            }
            else if (c == 's' || c == 'S') {
                PrintToConsole(&csConsole, "[Comando Principal]: Parar recebido.");
                SetEvent(hStopEvent); // Sinaliza o evento de parada
                break; // Sai do loop principal
            }
        }

        // Evita 100% de uso da CPU no loop principal (o _kbhit() é polling)
        // Este Sleep() está na tarefa principal, não na de medição,
        // então não viola a restrição.
        Sleep(100);
    }

    // --- Limpeza (Cleanup) ---
    PrintToConsole(&csConsole, "Aguardando encerramento da tarefa de medição...");
    WaitForSingleObject(hThread, INFINITE); // Espera a thread terminar
    PrintToConsole(&csConsole, "Tarefa encerrada. Limpando recursos.");

    // Fecha todos os handles e deleta as critical sections
    CloseHandle(hThread);
    CloseHandle(hStopEvent);
    CloseHandle(hPauseEvent);
    CloseHandle(buffer.hEmptySlots);
    CloseHandle(buffer.hFullSlots);
    DeleteCriticalSection(&buffer.csBuffer);
    DeleteCriticalSection(&csConsole);

    return 0;
}