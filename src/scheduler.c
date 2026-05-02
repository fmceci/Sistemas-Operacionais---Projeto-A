#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "scheduler.h"

/*
 * init_cpus - inicializa todas as CPUs como ociosas e desligadas.
 */
void init_cpus(CPU cpus[], int cpu_count) {
    for (int i = 0; i < cpu_count; i++) {
        cpus[i].id      = i;
        cpus[i].task_id = -1;
        cpus[i].active  = 0;
    }
}

/* -----------------------------------------------------------------------
 * Funções auxiliares de comparação para os critérios de desempate (req 4.3)
 * Ordem de desempate:
 *   1. Tarefa que já está executando (evita troca de contexto desnecessária)
 *   2. Menor instante de ingresso (quem chegou antes)
 *   3. Menor duração total
 *   4. Sorteio (rand)
 * ----------------------------------------------------------------------- */

/*
 * tiebreak - decide entre dois candidatos usando os critérios de desempate
 * definidos no requisito 4.3 do enunciado.
 *
 * Retorna o índice do vencedor (idx_a ou idx_b).
 * O parâmetro current_task indica quem está executando atualmente (favorece ele).
 * O ponteiro lottery_used indica se o sorteio foi necessário.
 */
static int tiebreak(Task tasks[], int idx_a, int idx_b,
                    int current_task, int *lottery_used) {
    /* Critério 1: tarefa que já está executando tem preferência */
    if (idx_a == current_task) return idx_a;
    if (idx_b == current_task) return idx_b;

    /* Critério 2: menor instante de ingresso */
    if (tasks[idx_a].arrival_time != tasks[idx_b].arrival_time)
        return (tasks[idx_a].arrival_time < tasks[idx_b].arrival_time) ? idx_a : idx_b;

    /* Critério 3: menor duração total */
    if (tasks[idx_a].duration != tasks[idx_b].duration)
        return (tasks[idx_a].duration < tasks[idx_b].duration) ? idx_a : idx_b;

    /* Critério 4: sorteio */
    if (lottery_used) *lottery_used = 1;
    return (rand() % 2 == 0) ? idx_a : idx_b;
}

/*
 * schedule_srtf - seleciona a tarefa com menor tempo restante (Shortest
 * Remaining Time First). Em caso de empate, aplica os critérios do req 4.3.
 *
 * Retorna o índice da tarefa escolhida, ou -1 se não houver tarefa READY.
 */
static int schedule_srtf(Task tasks[], int task_count,
                         int current_task, int *lottery_used) {
    int best = -1;

    for (int i = 0; i < task_count; i++) {
        /* Apenas tarefas READY ou a que já está RUNNING nesta CPU são candidatas */
        if (tasks[i].state != READY) continue;

        if (best == -1) {
            best = i;
            continue;
        }

        /* Critério primário SRTF: menor tempo restante */
        if (tasks[i].remaining_time < tasks[best].remaining_time) {
            best = i;
        } else if (tasks[i].remaining_time == tasks[best].remaining_time) {
            best = tiebreak(tasks, best, i, current_task, lottery_used);
        }
    }

    return best;
}

/*
 * schedule_priop - seleciona a tarefa com maior prioridade estática (Preemptive
 * Priority). Em caso de empate na prioridade, aplica os critérios do req 4.3.
 *
 * Retorna o índice da tarefa escolhida, ou -1 se não houver tarefa READY.
 */
static int schedule_priop(Task tasks[], int task_count,
                          int current_task, int *lottery_used) {
    int best = -1;

    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state != READY) continue;

        if (best == -1) {
            best = i;
            continue;
        }

        /* Critério primário PRIOP: maior valor de prioridade = mais prioritária */
        if (tasks[i].priority > tasks[best].priority) {
            best = i;
        } else if (tasks[i].priority == tasks[best].priority) {
            best = tiebreak(tasks, best, i, current_task, lottery_used);
        }
    }

    return best;
}

/*
 * schedule - ponto de entrada do escalonador para uma única CPU.
 *
 * Seleciona a próxima tarefa a executar considerando o algoritmo configurado.
 * Considera apenas tarefas no estado READY (tarefas RUNNING já foram atribuídas
 * a outras CPUs por assign_tasks antes desta chamada).
 *
 * Retorna o índice da tarefa escolhida, ou -1 se não houver candidata.
 */
int schedule(const char *algorithm, Task tasks[], int task_count,
             int current_task, int current_tick) {
    (void)current_tick; /* reservado para uso futuro */

    int lottery_used = 0;
    int chosen = -1;

    if (strcmp(algorithm, "SRTF") == 0) {
        chosen = schedule_srtf(tasks, task_count, current_task, &lottery_used);
    } else if (strcmp(algorithm, "PRIOP") == 0) {
        chosen = schedule_priop(tasks, task_count, current_task, &lottery_used);
    } else {
        fprintf(stderr, "Aviso: algoritmo '%s' desconhecido. Usando SRTF.\n", algorithm);
        chosen = schedule_srtf(tasks, task_count, current_task, &lottery_used);
    }

    if (lottery_used) {
        printf("[SORTEIO] Desempate por sorteio.\n");
    }

    return chosen;
}

/*
 * assign_tasks - distribui tarefas entre todas as CPUs para o tick atual.
 *
 * Algoritmo:
 *   1. Coloca todas as tarefas RUNNING de volta como READY.
 *   2. Para cada CPU, escolhe a melhor tarefa READY disponível.
 *   3. A tarefa escolhida é marcada imediatamente como RUNNING para que
 *      a próxima CPU não a selecione também (evita duplicidade).
 *   4. Liga a CPU se há tarefa disponível; desliga se não há (req 1.2).
 */
void assign_tasks(const char *algorithm, Task tasks[], int task_count,
                  CPU cpus[], int cpu_count, int tick) {
    /*
     * Primeira passagem: libera todas as tarefas RUNNING de volta para READY
     * para que o escalonador possa reavaliar todas as atribuições do tick.
     */
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state == RUNNING) {
            tasks[i].state  = READY;
            tasks[i].cpu_id = -1;
        }
    }

    /*
     * Segunda passagem: atribui uma tarefa diferente para cada CPU.
     * A tarefa recém-atribuída é imediatamente marcada como RUNNING,
     * tornando-a invisível para as CPUs seguintes.
     */
    for (int c = 0; c < cpu_count; c++) {
        /* Identifica a tarefa que estava nesta CPU no tick anterior
         * para o critério de desempate nº 1 (evitar troca de contexto). */
        int current_idx = -1;
        if (cpus[c].task_id != -1) {
            for (int i = 0; i < task_count; i++) {
                if (tasks[i].id == cpus[c].task_id &&
                    tasks[i].state == READY) {
                    current_idx = i;
                    break;
                }
            }
        }

        int chosen_idx = schedule(algorithm, tasks, task_count, current_idx, tick);

        if (chosen_idx == -1) {
            /* Sem tarefa disponível: desliga a CPU (req 1.2) */
            cpus[c].task_id = -1;
            cpus[c].active  = 0;
        } else {
            /* Atribui e marca como RUNNING para não ser escolhida por outra CPU */
            cpus[c].task_id          = tasks[chosen_idx].id;
            cpus[c].active           = 1;
            tasks[chosen_idx].state  = RUNNING;
            tasks[chosen_idx].cpu_id = c;
        }
    }
}
