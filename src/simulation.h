#ifndef SIMULATION_H
#define SIMULATION_H

#include "task.h"
#include "config.h"
#include "scheduler.h"
#include "gantt.h"

/*
 * SimulationState - encapsula todo o estado mutável da simulação.
 *
 * Mantém as tarefas, CPUs, relógio e histórico em um único objeto,
 * o que facilita o avanço/retrocesso da simulação (req 1.5.2).
 */
typedef struct {
    Task         tasks[MAX_TASKS]; /* Vetor de TCBs (Task Control Blocks) */
    int          task_count;       /* Número de tarefas carregadas */
    CPU          cpus[MAX_CPUS];   /* Vetor de CPUs */
    Config       config;           /* Configurações gerais (algoritmo, quantum, CPUs) */
    int          clock;            /* Relógio global atual (em ticks) */
    GanttHistory history;          /* Histórico completo para Gantt e para retroceder */
} SimulationState;

/*
 * limpar_buffer - remove caracteres pendentes do buffer stdin.
 * Necessário após scanf() para evitar leituras incorretas com fgets().
 */
void limpar_buffer(void);

/*
 * ler_inteiro_com_padrao - lê um inteiro do stdin.
 * Se o usuário pressionar ENTER sem digitar nada, retorna valor_padrao.
 */
int ler_inteiro_com_padrao(const char *mensagem, int valor_padrao);

/*
 * simulation_init - inicializa o SimulationState a partir das configurações.
 * Copia tarefas, inicializa CPUs e o histórico do Gantt.
 */
void simulation_init(SimulationState *sim, Config *config,
                     Task tasks[], int task_count);

/*
 * simulation_step - avança a simulação em exatamente um tick.
 *
 * Sequência:
 *   1. Verifica chegadas de tarefas (NEW → READY)
 *   2. Distribui tarefas entre as CPUs (escalonador + preempção)
 *   3. Executa 1 tick de processamento (decrementa remaining_time)
 *   4. Registra snapshot no histórico do Gantt
 *   5. Avança o relógio
 *
 * Retorna 1 se ainda há tarefas pendentes, 0 se a simulação terminou.
 */
int simulation_step(SimulationState *sim);

/*
 * simulation_run_complete - executa todos os ticks sem interação humana.
 * Ao final, exibe o gráfico terminal e gera o SVG (req 1.5.3, 2.4).
 */
void simulation_run_complete(SimulationState *sim);

/*
 * simulation_run_step_by_step - modo interativo: avançar, retroceder,
 * modificar estados e inspecionar o sistema tick a tick (req 1.5.1, 1.5.2).
 *
 * Comandos:
 *   n / ENTER  - avança um tick (exibe Gantt atualizado)
 *   b          - retrocede um tick
 *   m          - modifica estado de uma tarefa (req 3.4)
 *   i          - inspeciona estado detalhado de todas as tarefas e CPUs
 *   q          - encerra e gera SVG
 */
void simulation_run_step_by_step(SimulationState *sim);

#endif /* SIMULATION_H */