#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "scheduler.h"

/*
 * init_cpus - inicializa todas as CPUs como ociosas e desligadas.
 */
void init_cpus(CPU cpus[], int cpu_count) {
    for (int i = 0; i < cpu_count; i++) {
        cpus[i].id        = i;
        cpus[i].task_id   = -1;
        cpus[i].active    = 0;
        cpus[i].idle_time = 0;
    }
}

/* -----------------------------------------------------------------------
 * Funções auxiliares de comparação para os critérios de desempate (req 4.3)
 *
 * Ordem de desempate (válida para todos os algoritmos):
 *   1. Tarefa que já está executando nesta CPU (evita troca desnecessária)
 *   2. Menor instante de ingresso (quem chegou antes tem prioridade)
 *   3. Menor duração total da tarefa
 *   4. Sorteio (aleatório; ativa flag lottery_used)
 * ----------------------------------------------------------------------- */

/*
 * tiebreak - decide entre dois candidatos usando os critérios de desempate
 * definidos no requisito 4.3 do enunciado.
 *
 * Parâmetros:
 *   tasks        - vetor de tarefas
 *   idx_a, idx_b - índices dos dois candidatos no vetor tasks[]
 *   current_idx  - índice da tarefa que já está nesta CPU (-1 = nenhuma)
 *   lottery_used - ponteiro para flag; setado para 1 se houve sorteio
 *
 * Retorna o índice do candidato vencedor.
 */
static int tiebreak(Task tasks[], int idx_a, int idx_b,
                    int current_idx, int *lottery_used) {
    /* Critério 1: favorece quem já está executando nesta CPU */
    if (idx_a == current_idx) return idx_a;
    if (idx_b == current_idx) return idx_b;

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
 * schedule_srtf - seleciona a tarefa READY com menor tempo restante (SRTF).
 *
 * SRTF (Shortest Remaining Time First) é preemptivo: a tarefa com menor
 * tempo restante sempre tem prioridade. Em caso de empate, aplica req 4.3.
 *
 * Retorna o índice da tarefa escolhida, ou -1 se nenhuma estiver READY.
 */
static int schedule_srtf(Task tasks[], int task_count,
                         int current_idx, int *lottery_used) {
    int best = -1;

    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state != READY) continue;

        if (best == -1) {
            best = i;
            continue;
        }

        /* Critério primário SRTF: menor tempo restante */
        if (tasks[i].remaining_time < tasks[best].remaining_time) {
            best = i;
        } else if (tasks[i].remaining_time == tasks[best].remaining_time) {
            best = tiebreak(tasks, best, i, current_idx, lottery_used);
        }
    }

    return best;
}

/*
 * schedule_priop - seleciona a tarefa READY com maior prioridade estática.
 *
 * PRIOP (Prioridade Preemptivo): maior valor de prioridade = mais prioritária.
 * Conforme requisito 4.4, o 1º critério de desempate é a prioridade estática,
 * seguido pelos demais critérios do req 4.3.
 *
 * Retorna o índice da tarefa escolhida, ou -1 se nenhuma estiver READY.
 */
static int schedule_priop(Task tasks[], int task_count,
                          int current_idx, int *lottery_used) {
    int best = -1;

    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state != READY) continue;

        if (best == -1) {
            best = i;
            continue;
        }

        /* Critério primário PRIOP: maior prioridade estática (req 4.4) */
        if (tasks[i].priority > tasks[best].priority) {
            best = i;
        } else if (tasks[i].priority == tasks[best].priority) {
            /* Empate de prioridade: aplica critérios do req 4.3 */
            best = tiebreak(tasks, best, i, current_idx, lottery_used);
        }
    }

    return best;
}

/*
 * schedule - ponto de entrada do escalonador para uma única CPU.
 *
 * Despacha para o algoritmo correto conforme a string 'algorithm'.
 * Algoritmos desconhecidos fazem fallback para SRTF com aviso.
 *
 * Retorna o índice da tarefa escolhida, ou -1 se nenhuma estiver READY.
 */
int schedule(const char *algorithm, Task tasks[], int task_count,
             int current_task, int *lottery_used) {
    if (strcmp(algorithm, "SRTF") == 0) {
        return schedule_srtf(tasks, task_count, current_task, lottery_used);
    } else if (strcmp(algorithm, "PRIOP") == 0) {
        return schedule_priop(tasks, task_count, current_task, lottery_used);
    } else {
        fprintf(stderr, "Aviso: algoritmo '%s' desconhecido. Usando SRTF.\n", algorithm);
        return schedule_srtf(tasks, task_count, current_task, lottery_used);
    }
}

/*
 * assign_tasks - distribui tarefas entre todas as CPUs para o tick atual.
 *
 * Algoritmo:
 *   1. Verifica quais tarefas RUNNING esgotaram o quantum; essas voltam a READY.
 *   2. Tarefas que ainda têm quantum disponível permanecem RUNNING (candidatas).
 *   3. Para cada CPU, o escalonador escolhe a melhor tarefa READY disponível.
 *      - Se a tarefa escolhida é a mesma que já está na CPU, apenas continua.
 *      - Se é diferente, ocorre preempção (nova tarefa recebe a CPU).
 *   4. CPUs sem tarefa disponível são desligadas (req 1.2).
 *
 * Retorna 1 se houve sorteio em alguma CPU durante o tick, 0 caso contrário.
 */
int assign_tasks(const char *algorithm, Task tasks[], int task_count,
                 CPU cpus[], int cpu_count, int quantum, int tick) {
    (void)tick; /* reservado para uso futuro */

    int global_lottery = 0;

    /*
     * Passo 1: libera tarefas RUNNING cujo quantum esgotou.
     * Tarefas que ainda têm slice disponível ficam RUNNING (para o critério 1
     * de desempate: favorecer quem já está executando).
     */
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state == RUNNING) {
            if (tasks[i].ticks_this_slice >= quantum) {
                /* Quantum esgotado: volta para a fila de prontos */
                tasks[i].state           = READY;
                tasks[i].cpu_id          = -1;
                tasks[i].ticks_this_slice = 0;
            }
            /* Caso contrário permanece RUNNING até o escalonador decidir */
        }
    }

    /*
     * Passo 2: para cada CPU, decide qual tarefa vai executar neste tick.
     * Tarefas RUNNING são consideradas READY para efeito de seleção,
     * mas recebem preferência no critério de desempate nº 1.
     */
    for (int c = 0; c < cpu_count; c++) {
        /*
         * Identifica o índice da tarefa que estava nesta CPU,
         * para o critério de desempate (evitar troca desnecessária).
         */
        int current_idx = -1;
        if (cpus[c].task_id != -1) {
            for (int i = 0; i < task_count; i++) {
                if (tasks[i].id == cpus[c].task_id &&
                    (tasks[i].state == READY || tasks[i].state == RUNNING)) {
                    current_idx = i;
                    break;
                }
            }
        }

        /*
         * Coloca temporariamente todas as RUNNING como READY
         * para o escalonador poder compará-las com as demais READY.
         * As tarefas já atribuídas a outra CPU neste loop foram marcadas
         * RUNNING → o escalonador as ignora (não são READY).
         */
        for (int i = 0; i < task_count; i++) {
            if (tasks[i].state == RUNNING && tasks[i].cpu_id == cpus[c].id) {
                tasks[i].state = READY;
            }
        }

        int lottery_used = 0;
        int chosen_idx   = schedule(algorithm, tasks, task_count,
                                    current_idx, &lottery_used);

        if (lottery_used) global_lottery = 1;

        if (chosen_idx == -1) {
            /* Sem tarefa disponível: desliga a CPU (req 1.2) */
            cpus[c].task_id = -1;
            cpus[c].active  = 0;
        } else {
            /* Atribui tarefa à CPU e marca como RUNNING imediatamente,
             * evitando que outras CPUs a escolham também. */
            int same_task = (cpus[c].task_id == tasks[chosen_idx].id);

            cpus[c].task_id          = tasks[chosen_idx].id;
            cpus[c].active           = 1;
            tasks[chosen_idx].state  = RUNNING;
            tasks[chosen_idx].cpu_id = c;

            /* Reseta o slice apenas se trocou de tarefa */
            if (!same_task) {
                tasks[chosen_idx].ticks_this_slice = 0;
            }
        }
    }

    return global_lottery;
}