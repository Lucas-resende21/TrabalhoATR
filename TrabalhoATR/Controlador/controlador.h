#pragma once
#pragma once

#include <windows.h>
#include <iostream>
#include <string>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <conio.h>

// --- Nomes dos Eventos para IPC (Etapa 1) ---
// (Usaremos nomes para controlar os processos externos)
#define EVENT_STOP_T4   "Global\\StopEvent_Task4"
#define EVENT_PAUSE_T4  "Global\\PauseEvent_Task4"
#define EVENT_STOP_T5   "Global\\StopEvent_Task5"
#define EVENT_PAUSE_T5  "Global\\PauseEvent_Task5"


// --- Constantes (Etapa 1) ---
#define LC1_CAPACITY 200
#define LC1_MSG_SIZE 60 // Um tamanho seguro para as msgs de Etapa 1

// --- Lista Circular 1 ---
// (Não precisa de memória compartilhada na Etapa 1, 
// pois T1, T2, e T3 estão no mesmo processo)
struct CircularBuffer1 {
    char buffer[LC1_CAPACITY][LC1_MSG_SIZE];
    int write_index;
    int read_index;
    CRITICAL_SECTION cs;    // Protege os índices
    HANDLE hEmptySlots;     // Semáforo (para produtores)
    HANDLE hFullSlots;      // Semáforo (para consumidor)
};

// --- Estruturas de Parâmetros para Threads ---
// (Usado para passar dados para T1, T2, T3)

struct TaskParams {
    CircularBuffer1* pLC1;
    HANDLE hStopEvent;       // Evento para parar (interno ao processo)
    HANDLE hPauseEvent;      // Evento para pausar (interno ao processo)
    CRITICAL_SECTION* pConsoleCS; // Para imprimir logs no console principal
};

// --- Declarações de Função ---

// Tarefa 1 (Medição)
DWORD WINAPI MeasurementTask(LPVOID lpParam);

// Tarefa 2 (CLP) - Vamos criar esta
DWORD WINAPI CLPTask(LPVOID lpParam);

// Tarefa 3 (Retirada)
DWORD WINAPI CaptureTask(LPVOID lpParam);

// Função auxiliar de log
void PrintToMainConsole(CRITICAL_SECTION* pCS, const std::string& msg);