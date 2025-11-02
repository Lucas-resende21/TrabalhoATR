#include "Tasks.h"

// --- TAREFA 5: Análise de Granulometria (Projetor 2) ---
DWORD WINAPI DisplayGranTask(LPVOID lpParam) {
    DisplayGranParams* params = (DisplayGranParams*)lpParam;

    // 1. Criar o console exclusivo ("Projetor")
    AllocConsole();
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTitleA("PROJETOR 2: ANALISE DE GRANULOMETRIA");

    DWORD dwWritten;
    WriteConsoleA(hConsole, "--- PROJETOR 2 (GRANULOMETRIA) ATIVO ---\n", 42, &dwWritten, NULL);

    static bool bPaused = false;
    char message[LC2_MSG_SIZE]; // 48 bytes

    while (true) {
        // --- 1. MANIPULAÇÃO DO ESTADO DE PAUSA ---
        if (bPaused) {
            WriteConsoleA(hConsole, "PAUSADO\n", 8, &dwWritten, NULL);
            HANDLE hWaitResume[] = { params->hStopEvent, params->hPauseEvent };
            DWORD waitResult = WaitForMultipleObjects(2, hWaitResume, FALSE, INFINITE);

            if (waitResult == WAIT_OBJECT_0) break; // STOP
            if (waitResult == WAIT_OBJECT_0 + 1) { // RESUME
                bPaused = false;
                WriteConsoleA(hConsole, "RETOMADO\n", 9, &dwWritten, NULL);
            }
        }

        // --- 2. AGUARDAR POR ITEM NA LISTA 2 ---
        HANDLE hWaitEvents[] = { params->hStopEvent, params->hPauseEvent, params->pLC2->hFullSlots };
        DWORD waitResult = WaitForMultipleObjects(3, hWaitEvents, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0) break; // STOP
        if (waitResult == WAIT_OBJECT_0 + 1) { // PAUSE
            bPaused = true;
            continue;
        }
        // Se for WAIT_OBJECT_0 + 2, há item na LC2.

        // --- 3. LER DA LISTA 2 (Consumidor) ---
        EnterCriticalSection(&params->pLC2->cs);
        {
            strcpy_s(message, LC2_MSG_SIZE, params->pLC2->buffer[params->pLC2->read_index]);
            params->pLC2->read_index = (params->pLC2->read_index + 1) % LC2_CAPACITY;
        }
        LeaveCriticalSection(&params->pLC2->cs);
        ReleaseSemaphore(params->pLC2->hEmptySlots, 1, NULL); // Sinaliza espaço vazio na LC2

        // --- 4. FORMATAR E EXIBIR A MENSAGEM ---
        // "NSEQ: #### ID: ## GMED: ###### GMAX: ###### GMIN: ###### SIG: ###### HH:MM:SS"
        // Formato Original: "11/NSEQ(4)/ID(2)/TIME(8)/GMED(6)/GMAX(6)/GMIN(6)/SIG(6)"

        std::string s_nseq(message + 3, 4);
        std::string s_id(message + 8, 2);
        std::string s_time(message + 11, 8);
        std::string s_gmed(message + 20, 6);
        std::string s_gmax(message + 27, 6);
        std::string s_gmin(message + 34, 6);
        std::string s_sig(message + 41, 6);

        std::stringstream ss;
        ss << "NSEQ: " << s_nseq << " ID: " << s_id << " GMED: " << s_gmed
            << " GMAX: " << s_gmax << " GMIN: " << s_gmin << " SIG: " << s_sig
            << " " << s_time << "\n";

        WriteConsoleA(hConsole, ss.str().c_str(), (DWORD)ss.str().length(), &dwWritten, NULL);
    }

    WriteConsoleA(hConsole, "--- PROJETOR 2 ENCERRANDO ---\n", 31, &dwWritten, NULL);
    FreeConsole();
    return 0;
}