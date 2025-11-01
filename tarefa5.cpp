#include <windows.h>
#include <stdio.h>
#include <conio.h>  // Para _getch()
#include <time.h>   // Para time()

// --- 1. Definição da Estrutura de Mensagem e Buffer Circular ---

struct MensagemGranulometria {
    int nseq;
    int id;
    int gmed;
    int gmax;
    int gmin;
    int sig;
    // O tempo será capturado no momento da exibição
};

#define TAM_BUFFER_GR 10
MensagemGranulometria g_BufferGranulometria[TAM_BUFFER_GR];
int g_idxLeituraGR = 0;
int g_idxEscritaGR = 0;

// --- 2. Objetos de Sincronização Globais ---

// Para a Lista Circular (Produtor-Consumidor)
CRITICAL_SECTION g_csBufferGR;  // Protege os índices g_idxLeituraGR e g_idxEscritaGR
HANDLE           g_hSemItensGR;   // Contagem de itens NO buffer (para o consumidor)
HANDLE           g_hSemEspacoGR;  // Contagem de espaços VAZIOS no buffer (para o produtor)

// Para Sincronização com a Tarefa de Teclado (Controle)
HANDLE g_hEventoAcionarDisplay;   // Teclado define: "Exiba o próximo item"
HANDLE g_hEventoDisplayConcluido; // Tarefa de display define: "Item exibido"

// Handle para o console do projetor
HANDLE g_hConsoleProjetor = NULL;


// --- 3. A Tarefa de Análise de Granulometria (Consumidor) ---

/**
 * @brief Esta é a thread principal da tarefa de análise de granulometria.
 * Ela simula o projetor criando seu próprio console.
 */
DWORD WINAPI TarefaAnaliseGranulometria(LPVOID lpParam) {
    // 3.1. Cria o console exclusivo ("Projetor")
    AllocConsole();
    SetConsoleTitleA("Projetor - Sala de Controle (Granulometria)");
    g_hConsoleProjetor = GetStdHandle(STD_OUTPUT_HANDLE);
    if (g_hConsoleProjetor == INVALID_HANDLE_VALUE) {
        printf("Falha ao obter handle do novo console.\n");
        return 1;
    }

    char buffer[256];
    DWORD dwWritten;
    sprintf_s(buffer, "--- PROJETOR DE GRANULOMETRIA ATIVO ---\n");
    WriteConsoleA(g_hConsoleProjetor, buffer, (DWORD)strlen(buffer), &dwWritten, NULL);

    // Loop principal da tarefa
    while (true) {
        // 3.2. Sincroniza com a Tarefa de Teclado (Req 2)
        // Aguarda o "comando" da tarefa de teclado para exibir o próximo item
        WaitForSingleObject(g_hEventoAcionarDisplay, INFINITE);
        ResetEvent(g_hEventoAcionarDisplay); // Prepara para a próxima espera

        // 3.3. Sincroniza com o Buffer Circular (Req 1)
        // Aguarda até que um item esteja disponível no buffer
        WaitForSingleObject(g_hSemItensGR, INFINITE);

        // 3.4. Acessa a Lista Circular (Zona Crítica)
        EnterCriticalSection(&g_csBufferGR);
        MensagemGranulometria msg = g_BufferGranulometria[g_idxLeituraGR];
        g_idxLeituraGR = (g_idxLeituraGR + 1) % TAM_BUFFER_GR;
        LeaveCriticalSection(&g_csBufferGR);

        // 3.5. Sinaliza ao Produtor que um espaço foi liberado
        ReleaseSemaphore(g_hSemEspacoGR, 1, NULL);

        // 3.6. Obtém o tempo atual
        SYSTEMTIME st;
        GetLocalTime(&st);
        char timeStr[10];
        sprintf_s(timeStr, "%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);

        // 3.7. Formata e Exibe a Mensagem no console do projetor
        sprintf_s(buffer, 256,
            "NSEQ: %04d ID: %02d GMED: %06d GMAX: %06d GMIN: %06d SIG: %06d %s\n",
            msg.nseq, msg.id, msg.gmed, msg.gmax, msg.gmin, msg.sig, timeStr
        );

        WriteConsoleA(g_hConsoleProjetor, buffer, (DWORD)strlen(buffer), &dwWritten, NULL);

        // 3.8. Sinaliza à Tarefa de Teclado que a exibição foi concluída
        SetEvent(g_hEventoDisplayConcluido);
    }

    // Limpeza (embora o loop acima seja infinito)
    FreeConsole();
    return 0;
}


// --- 4. Simulação de um Produtor (adiciona dados ao buffer) ---

DWORD WINAPI TarefaProdutoraSimulada(LPVOID lpParam) {
    int nseq = 1;
    srand((unsigned)time(NULL)); // Inicializa gerador aleatório

    while (true) {
        // Aguarda por espaço no buffer
        WaitForSingleObject(g_hSemEspacoGR, INFINITE);

        // Prepara dados
        MensagemGranulometria novaMsg;
        novaMsg.nseq = nseq++;
        novaMsg.id = 1;
        novaMsg.gmed = 5000 + (rand() % 201 - 100); // 5000 +/- 100
        novaMsg.gmax = novaMsg.gmed + 1000;
        novaMsg.gmin = novaMsg.gmed - 1000;
        novaMsg.sig = 1500 + (rand() % 101 - 50);

        // Entra na zona crítica para escrever no buffer
        EnterCriticalSection(&g_csBufferGR);
        g_BufferGranulometria[g_idxEscritaGR] = novaMsg;
        g_idxEscritaGR = (g_idxEscritaGR + 1) % TAM_BUFFER_GR;
        LeaveCriticalSection(&g_csBufferGR);

        // Sinaliza ao consumidor que um novo item está disponível
        ReleaseSemaphore(g_hSemItensGR, 1, NULL);

        Sleep(2000); // Produz um novo dado a cada 2 segundos
    }
    return 0;
}


// --- 5. Main (Simula a Tarefa de Teclado) ---

int main() {
    // 5.1. Inicializa objetos de sincronização do buffer
    InitializeCriticalSection(&g_csBufferGR);
    // (Handle, ContagemInicial, ContagemMáxima, Nome)
    g_hSemItensGR = CreateSemaphore(NULL, 0, TAM_BUFFER_GR, NULL);
    g_hSemEspacoGR = CreateSemaphore(NULL, TAM_BUFFER_GR, TAM_BUFFER_GR, NULL);

    // 5.2. Inicializa objetos de sincronização do teclado
    // (Segurança, ResetManual, EstadoInicial, Nome)
    g_hEventoAcionarDisplay = CreateEvent(NULL, FALSE, FALSE, NULL); // Auto-reset
    g_hEventoDisplayConcluido = CreateEvent(NULL, TRUE, FALSE, NULL); // Manual-reset

    // 5.3. Cria as threads
    DWORD threadId;
    HANDLE hThreadDisplay = CreateThread(NULL, 0, TarefaAnaliseGranulometria, NULL, 0, &threadId);
    HANDLE hThreadProdutor = CreateThread(NULL, 0, TarefaProdutoraSimulada, NULL, 0, &threadId);

    // 5.4. Loop da "Tarefa de Teclado" (Thread Principal)
    printf("Simulador de Tarefa de Teclado / Controle Principal\n");
    printf("Pressione 'G' para exibir a proxima medicao no projetor.\n");
    printf("Pressione 'Q' para sair.\n");

    while (true) {
        char c = _getch();
        if (c == 'g' || c == 'G') {
            printf("[MAIN] Comando 'G' recebido. Acionando display...\n");

            // 1. Reseta o evento de "concluído"
            ResetEvent(g_hEventoDisplayConcluido);

            // 2. Dispara o evento para a tarefa de display
            SetEvent(g_hEventoAcionarDisplay);

            // 3. Aguarda a tarefa de display sinalizar que terminou
            WaitForSingleObject(g_hEventoDisplayConcluido, INFINITE);

            printf("[MAIN] Display concluido. Aguardando proximo comando...\n");

        }
        else if (c == 'q' || c == 'Q') {
            printf("[MAIN] Saindo...\n");
            break;
        }
    }

    // 5.5. Limpeza
    CloseHandle(g_hSemItensGR);
    CloseHandle(g_hSemEspacoGR);
    CloseHandle(g_hEventoAcionarDisplay);
    CloseHandle(g_hEventoDisplayConcluido);
    DeleteCriticalSection(&g_csBufferGR);

    // Encerra as threads de forma abrupta para o exemplo
    TerminateThread(hThreadDisplay, 0);
    TerminateThread(hThreadProdutor, 0);
    CloseHandle(hThreadDisplay);
    CloseHandle(hThreadProdutor);

    return 0;
}