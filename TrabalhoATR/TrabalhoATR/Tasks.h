#pragma once
#pragma once

#include <windows.h>
#include <iostream>
#include <string>
#include <iomanip>      // Para std::setfill, std::setw
#include <sstream>      // Para std::stringstream
#include <ctime>        // Para time()
#include <cstdlib>      // Para rand()
#include <conio.h>      // Para _getch() e _kbhit()

// --- 1. CONSTANTES E ESTRUTURAS GLOBAIS ---

// --- Listas Circulares ---
#define LC1_CAPACITY 200
#define LC1_MSG_SIZE 56 // Tamanho da msg do CLP (55) + 1 nulo
#define LC2_CAPACITY 100
#define LC2_MSG_SIZE 48 // Tamanho da msg de Medição (47) + 1 nulo

/**
 * @brief Define a Lista Circular 1 (CLP + Medição)
 */
struct CircularBuffer1 {
    char buffer[LC1_CAPACITY][LC1_MSG_SIZE];
    int write_index;
    int read_index;
    CRITICAL_SECTION cs;    // Protege o acesso aos índices
    HANDLE hEmptySlots;     // Semáforo: conta posições vazias (bloqueia produtores)
    HANDLE hFullSlots;      // Semáforo: conta posições cheias (bloqueia consumidor)
};

/**
 * @brief Define a Lista Circular 2 (Apenas Medição)
 */
struct CircularBuffer2 {
    char buffer[LC2_CAPACITY][LC2_MSG_SIZE];
    int write_index;
    int read_index;
    CRITICAL_SECTION cs;
    HANDLE hEmptySlots;     // Semáforo (bloqueia produtor: Task 3)
    HANDLE hFullSlots;      // Semáforo (bloqueia consumidor: Task 5)
};

// --- Estruturas de Parâmetros das Threads ---

/**
 * @brief Parâmetros para Tarefas 1 (Medição) e 2 (CLP)
 */
struct ProducerTaskParams {
    CircularBuffer1* pLC1;
    HANDLE hStopEvent;
    HANDLE hPauseEvent;
    CRITICAL_SECTION* pConsoleCS; // Protege o std::cout principal
};

/**
 * @brief Parâmetros para Tarefa 3 (Captura)
 */
struct CaptureTaskParams {
    CircularBuffer1* pLC1;
    CircularBuffer2* pLC2;
    HANDLE hPipeWrite;      // Handle de escrita do Pipe (para Task 4)
    HANDLE hStopEvent;
    HANDLE hPauseEvent;
    CRITICAL_SECTION* pConsoleCS;
};

/**
 * @brief Parâmetros para Tarefa 4 (Exibição Processo)
 */
struct DisplayProcessParams {
    HANDLE hPipeRead;       // Handle de leitura do Pipe (de Task 3)
    HANDLE hStopEvent;
    HANDLE hPauseEvent;
    HANDLE hClearEvent;     // Evento para limpar o console
};

/**
 * @brief Parâmetros para Tarefa 5 (Exibição Granulometria)
 */
struct DisplayGranParams {
    CircularBuffer2* pLC2;
    HANDLE hStopEvent;
    HANDLE hPauseEvent;
};

// --- 2. FUNÇÕES AUXILIARES ---

/**
 * @brief Imprime uma mensagem no console principal de forma thread-safe.
 * (Definida como 'inline' para evitar erros de linker)
 */
inline void PrintToMainConsole(CRITICAL_SECTION* pCS, const std::string& msg) {
    EnterCriticalSection(pCS);
    std::cout << msg << std::endl;
    LeaveCriticalSection(pCS);
}

// --- 3. DECLARAÇÕES DAS FUNÇÕES DAS TAREFAS ---

// Tarefa 1
DWORD WINAPI MeasurementTask(LPVOID lpParam);

// Tarefa 2
DWORD WINAPI CLPTask(LPVOID lpParam);

// Tarefa 3
DWORD WINAPI CaptureTask(LPVOID lpParam);

// Tarefa 4
DWORD WINAPI DisplayProcessTask(LPVOID lpParam);

// Tarefa 5
DWORD WINAPI DisplayGranTask(LPVOID lpParam);