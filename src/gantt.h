#ifndef GANTT_H
#define GANTT_H

#include "task.h"
#include "scheduler.h"
#include "config.h"

/* Número máximo de ticks que o histórico do Gantt pode armazenar */
#define MAX_TICKS 1024

/*
 * GanttEntry - representa o estado do sistema em um único tick do relógio.
 * Armazena qual tarefa estava em cada CPU e o estado de cada tarefa.
 * Usado para avançar/retroceder a simulação (req 1.5.2) e para gerar o SVG.
 */
typedef struct {
    int tick;                    /* Instante de tempo deste registro */
    int cpu_task[MAX_CPUS];      /* cpu_task[i] = ID da tarefa na CPU i (-1 = desligada) */
    int cpu_active[MAX_CPUS];    /* cpu_active[i] = 1 se CPU i estava ligada */
    TaskState task_state[MAX_TASKS]; /* Estado de cada tarefa neste tick */
    int task_remaining[MAX_TASKS];   /* Tempo restante de cada tarefa neste tick */
    int lottery_tick;            /* 1 se houve sorteio neste tick */
    int task_arrived[MAX_TASKS]; /* 1 se a tarefa chegou neste tick */
    int task_finished[MAX_TASKS];/* 1 se a tarefa terminou neste tick */
} GanttEntry;

/*
 * GanttHistory - histórico completo da simulação.
 * Permite avançar e retroceder livremente (req 1.5.2).
 */
typedef struct {
    GanttEntry entries[MAX_TICKS]; /* Um registro por tick */
    int count;                     /* Número de entradas armazenadas */
    int cpu_count;                 /* Número de CPUs (para geração do SVG) */
    int task_count;                /* Número de tarefas */
} GanttHistory;

/*
 * gantt_init - inicializa o histórico do Gantt.
 */
void gantt_init(GanttHistory *history, int cpu_count, int task_count);

/*
 * gantt_record - registra o estado do sistema no tick atual.
 * Deve ser chamada uma vez por tick, após todas as atualizações.
 */
void gantt_record(GanttHistory *history, int tick,
                  CPU cpus[], int cpu_count,
                  Task tasks[], int task_count,
                  int lottery_used);

/*
 * gantt_print_tick - imprime o estado do tick atual no terminal.
 * Usado no modo passo-a-passo (req 1.5.1).
 */
void gantt_print_tick(const GanttHistory *history, int tick,
                      Task tasks[], int task_count);

/*
 * gantt_save_svg - gera o arquivo SVG com o gráfico de Gantt completo.
 * Requisito 2.4: deve gerar imagem ao final da simulação.
 *
 * Parâmetros:
 *   history   - histórico completo da simulação
 *   tasks     - vetor de tarefas (para nomes/cores)
 *   task_count - número de tarefas
 *   filename  - caminho do arquivo SVG de saída
 */
void gantt_save_svg(const GanttHistory *history, Task tasks[],
                    int task_count, const char *filename);

#endif /* GANTT_H */
