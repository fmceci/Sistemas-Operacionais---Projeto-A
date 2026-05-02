#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "task.h"
#include "config.h"

/*
 * CPU - representa um processador do sistema simulado.
 * Conforme enunciado, o sistema possui múltiplos processadores.
 */
typedef struct {
    int id;       /* Identificador da CPU (0 a cpu_count-1) */
    int task_id;  /* ID da tarefa em execução (-1 = CPU ociosa/desligada) */
    int active;   /* 1 = CPU ligada, 0 = CPU desligada (sem tarefa pronta) */
} CPU;

/*
 * schedule - executa um passo do escalonador para uma única CPU.
 *
 * O escalonador:
 * 1. Seleciona a melhor tarefa READY para a CPU conforme o algoritmo configurado.
 * 2. Aplica os critérios de desempate definidos no requisito 4.3.
 * 3. Preempta a tarefa atual se necessário.
 * 4. Retorna o índice da tarefa escolhida, ou -1 se não houver tarefa disponível.
 *
 * Parâmetros:
 *   algorithm    - string do algoritmo ("SRTF" ou "PRIOP")
 *   tasks        - vetor de todas as tarefas
 *   task_count   - número de tarefas
 *   current_task - índice da tarefa atualmente na CPU (-1 = nenhuma)
 *   current_tick - instante atual do relógio global
 */
int schedule(const char *algorithm, Task tasks[], int task_count,
             int current_task, int current_tick);

/*
 * assign_tasks - distribui tarefas entre todas as CPUs para o tick atual.
 * Liga/desliga CPUs conforme disponibilidade de tarefas prontas (req 1.2).
 *
 * Parâmetros:
 *   algorithm  - algoritmo de escalonamento
 *   tasks      - vetor de tarefas
 *   task_count - número de tarefas
 *   cpus       - vetor de CPUs
 *   cpu_count  - número de CPUs
 *   tick       - instante atual
 */
void assign_tasks(const char *algorithm, Task tasks[], int task_count,
                  CPU cpus[], int cpu_count, int tick);

/*
 * init_cpus - inicializa o vetor de CPUs com os valores padrão.
 */
void init_cpus(CPU cpus[], int cpu_count);

#endif /* SCHEDULER_H */
