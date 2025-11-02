#include "Tasks.h"

// --- TAREFA 4: Exibição de Dados do Processo (Projetor 1) ---
DWORD WINAPI DisplayProcessTask(LPVOID lpParam) {
    DisplayProcessParams* params = (DisplayProcessParams*)lpParam;

    // 1. Criar o console exclusivo ("Projetor")
    AllocConsole();
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTitleA("PROJETOR 1: DADOS DE PROCESSO");

    DWORD dwWritten;
    WriteConsoleA(hConsole, "--- PROJETOR 1 (PROCESSO) ATIVO ---\n", 37, &dwWritten, NULL);

    static bool bPaused = false;
    char message[LC1_MSG_SIZE]; // 56 bytes

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

        // --- 2. VERIFICAR EVENTOS DE CONTROLE (Stop, Pause, Clear) ---
        HANDLE hWaitEvents[] = { params->hStopEvent, params->hPauseEvent, params->hClearEvent };
        DWORD waitResult = WaitForMultipleObjects(3, hWaitEvents, FALSE, 0); // Timeout 0

        if (waitResult == WAIT_OBJECT_0) break; // STOP
        if (waitResult == WAIT_OBJECT_0 + 1) { // PAUSE
            bPaused = true;
            continue;
        }
        if (waitResult == WAIT_OBJECT_0 + 2) { // CLEAR
            COORD coordScreen = { 0, 0 };
            DWORD cCharsWritten;
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            GetConsoleScreenBufferInfo(hConsole, &csbi);
            DWORD dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
            FillConsoleOutputCharacter(hConsole, (TCHAR)' ', dwConSize, coordScreen, &cCharsWritten);
            SetConsoleCursorPosition(hConsole, coordScreen);
            WriteConsoleA(hConsole, "--- PROJETOR 1 (PROCESSO) LIMPO ---\n", 37, &dwWritten, NULL);
        }

        // --- 3. LER DO PIPE (Não bloqueante) ---
        DWORD bytesAvailable = 0;
        if (!PeekNamedPipe(params->hPipeRead, NULL, 0, NULL, &bytesAvailable, NULL)) {
            break; // Erro no pipe
        }

        if (bytesAvailable > 0) {
            DWORD bytesRead;
            BOOL bSuccess = ReadFile(
                params->hPipeRead,
                message,
                LC1_MSG_SIZE,
                &bytesRead,
                NULL
            );

            if (bSuccess && bytesRead == LC1_MSG_SIZE) {
                // --- 4. FORMATAR E EXIBIR A MENSAGEM ---
                // "MSS NSEQ: #### ID: ## VZ E: ###### VZ S: ###### V: ###### ANG: #### P: ##### HH:MM:SS"
                // Formato Original: "44/NSEQ(4)/ID(2)/TIME(12)/POT(5)/VEL(6)/INCL(4)/VZE(6)/VZS(6)"

                std::string s_nseq(message + 3, 4);
                std::string s_id(message + 8, 2);
                std::string s_time(message + 11, 8); // Apenas HH:MM:SS
                std::string s_pot(message + 24, 5);
                std::string s_vel(message + 30, 6);
                std::string s_incl(message + 37, 4);
                std::string s_vze(message + 42, 6);
                std::string s_vzs(message + 49, 6);

                std::stringstream ss;
                ss << "MSS NSEQ: " << s_nseq << " ID: " << s_id << " VZ E: " << s_vze
                    << " VZ S: " << s_vzs << " V: " << s_vel << " ANG: " << s_incl
                    << " P: " << s_pot << " " << s_time << "\n";

                WriteConsoleA(hConsole, ss.str().c_str(), (DWORD)ss.str().length(), &dwWritten, NULL);
            }
        }
        else {
            Sleep(50); // Dorme 50ms para não usar 100% CPU
        }
    }

    WriteConsoleA(hConsole, "--- PROJETOR 1 ENCERRANDO ---\n", 31, &dwWritten, NULL);
    FreeConsole();
    return 0;
}