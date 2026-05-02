#ifndef SIMULATION_H
#define SIMULATION_H

#include "task.h"
#include "config.h"
#include "scheduler.h"
#include "gantt.h"

/*
 * SimulationState - encapsula todo o estado da simulação.
 * Usado para avançar e retroceder a simulação (req 1.5.2).
 */
typedef struct {
    Task       tasks[MAX_TASKS];  /* Vetor de tarefas (TCBs) */
    int        task_count;        /* Número de tarefas */
    CPU        cpus[MAX_CPUS];    /* Vetor de CPUs */
    Config     config;            /* Configuração geral */
    int        clock;             /* Relógio global atual */
    GanttHistory history;         /* Histórico para o Gantt e para retroceder */
} SimulationState;

/*
 * simulation_init - inicializa o estado da simulação a partir da configuração.
 */
void simulation_init(SimulationState *sim, Config *config,
                     Task tasks[], int task_count);

/*
 * simulation_step - avança a simulação em um tick.
 * Retorna 1 se ainda há tarefas rodando/prontas, 0 se terminou.
 */
int simulation_step(SimulationState *sim);

/*
 * simulation_run_complete - executa a simulação completa sem interação (req 1.5.3).
 */
void simulation_run_complete(SimulationState *sim);

/*
 * simulation_run_step_by_step - executa em modo passo-a-passo (req 1.5.1 / 1.5.2).
 * Permite avançar, retroceder e modificar estados de tarefas.
 */
void simulation_run_step_by_step(SimulationState *sim);

#endif /* SIMULATION_H */