#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "simulation.h"

/* -----------------------------------------------------------------------
 * Inicialização
 * ----------------------------------------------------------------------- */

/*
 * simulation_init - copia as tarefas e configurações para o estado da simulação,
 * inicializa CPUs e o histórico do Gantt.
 */
void simulation_init(SimulationState *sim, Config *config,
                     Task tasks[], int task_count) {
    memcpy(&sim->config, config, sizeof(Config));

    sim->task_count = task_count;
    for (int i = 0; i < task_count; i++) {
        sim->tasks[i] = tasks[i];
    }

    sim->clock = 0;

    init_cpus(sim->cpus, config->cpu_count);
    gantt_init(&sim->history, config->cpu_count, task_count);
}

/* -----------------------------------------------------------------------
 * Tick único
 * ----------------------------------------------------------------------- */

/*
 * check_arrivals - atualiza tarefas que chegam no tick atual de NEW → READY.
 * Requisito 1.4: a simulação deve respeitar o instante de ingresso de cada tarefa.
 */
static void check_arrivals(SimulationState *sim) {
    for (int i = 0; i < sim->task_count; i++) {
        if (sim->tasks[i].state == NEW &&
            sim->tasks[i].arrival_time == sim->clock) {
            sim->tasks[i].state = READY;
        }
    }
}

/*
 * execute_tick - executa um tick de processamento em cada CPU ativa.
 * Decrementa remaining_time e finaliza tarefas que chegam a zero.
 * Registra start_time na primeira execução da tarefa.
 */
static int execute_tick(SimulationState *sim) {
    int any_active = 0;

    for (int c = 0; c < sim->config.cpu_count; c++) {
        if (!sim->cpus[c].active || sim->cpus[c].task_id == -1) continue;

        any_active = 1;

        /* Encontra a tarefa desta CPU */
        for (int i = 0; i < sim->task_count; i++) {
            if (sim->tasks[i].id == sim->cpus[c].task_id) {
                /* Registra instante de início (apenas na primeira execução) */
                if (sim->tasks[i].start_time == -1) {
                    sim->tasks[i].start_time = sim->clock;
                }

                sim->tasks[i].remaining_time--;

                /* Verifica se a tarefa terminou */
                if (sim->tasks[i].remaining_time <= 0) {
                    sim->tasks[i].state       = FINISHED;
                    sim->tasks[i].finish_time = sim->clock;
                    sim->tasks[i].cpu_id      = -1;
                    sim->cpus[c].task_id      = -1;
                    sim->cpus[c].active       = 0;
                    printf("  [CONCLUIDA] Tarefa %d terminou no tick %d.\n",
                           sim->tasks[i].id, sim->clock);
                }
                break;
            }
        }
    }

    return any_active;
}

/*
 * has_pending_tasks - retorna 1 se ainda há tarefas não terminadas.
 */
static int has_pending_tasks(SimulationState *sim) {
    for (int i = 0; i < sim->task_count; i++) {
        if (sim->tasks[i].state != FINISHED) return 1;
    }
    return 0;
}

/*
 * simulation_step - executa um único tick completo da simulação.
 *
 * Sequência por tick (conforme planejamento da equipe):
 *   1. Verifica chegadas de tarefas (NEW → READY)
 *   2. Distribui tarefas entre as CPUs (escalonador + preempção)
 *   3. Executa 1 tick de processamento
 *   4. Registra no histórico do Gantt
 *   5. Avança o relógio
 *
 * Retorna 1 se ainda há trabalho, 0 se a simulação terminou.
 */
int simulation_step(SimulationState *sim) {
    if (!has_pending_tasks(sim)) return 0;

    printf("\nTempo %d:\n", sim->clock);

    /* Passo 1: atualiza chegadas */
    check_arrivals(sim);

    /* Passo 2: escalonador distribui tarefas */
    assign_tasks(sim->config.algorithm, sim->tasks, sim->task_count,
                 sim->cpus, sim->config.cpu_count, sim->clock);

    /* Imprime estado das CPUs */
    for (int c = 0; c < sim->config.cpu_count; c++) {
        if (sim->cpus[c].active) {
            printf("  CPU %d -> Tarefa %d\n", c, sim->cpus[c].task_id);
        } else {
            printf("  CPU %d -> desligada\n", c);
        }
    }

    /* Passo 3: executa 1 tick */
    execute_tick(sim);

    /* Passo 4: registra histórico (lottery_used vem do scheduler via stdout por ora) */
    gantt_record(&sim->history, sim->clock, sim->cpus, sim->config.cpu_count,
                 sim->tasks, sim->task_count, 0);

    /* Passo 5: avança relógio */
    sim->clock++;

    return has_pending_tasks(sim);
}

/* -----------------------------------------------------------------------
 * Modo completo (req 1.5.3)
 * ----------------------------------------------------------------------- */

/*
 * simulation_run_complete - executa todos os ticks sem interação humana.
 * Ao final, gera o SVG do Gantt.
 */
void simulation_run_complete(SimulationState *sim) {
    printf("=== Modo de execucao completa ===\n");

    while (simulation_step(sim)) { /* executa até acabar */ }

    printf("\n=== Simulacao concluida no tick %d ===\n", sim->clock);

    /* Gera o gráfico de Gantt como imagem SVG (req 2.4) */
    gantt_save_svg(&sim->history, sim->tasks, sim->task_count, "gantt.svg");
}

/* -----------------------------------------------------------------------
 * Modo passo-a-passo (req 1.5.1 e 1.5.2)
 * ----------------------------------------------------------------------- */

/*
 * restore_snapshot - restaura o estado da simulação a partir de um snapshot
 * do histórico. Usado ao retroceder a simulação.
 *
 * Nota: restaura apenas os campos que mudam durante a simulação.
 */
static void restore_snapshot(SimulationState *sim, int snapshot_index) {
    if (snapshot_index < 0 || snapshot_index >= sim->history.count) return;

    const GanttEntry *e = &sim->history.entries[snapshot_index];
    sim->clock = e->tick;

    /* Restaura estados e remaining_time das tarefas */
    for (int i = 0; i < sim->task_count; i++) {
        sim->tasks[i].state          = e->task_state[i];
        sim->tasks[i].remaining_time = e->task_remaining[i];
    }

    /* Restaura estado das CPUs */
    for (int c = 0; c < sim->config.cpu_count; c++) {
        sim->cpus[c].task_id = e->cpu_task[c];
        sim->cpus[c].active  = e->cpu_active[c];
    }
}

/*
 * prompt_modify_task - permite ao usuário modificar o estado de uma tarefa (req 3.4).
 */
static void prompt_modify_task(SimulationState *sim) {
    int id;
    printf("Digite o ID da tarefa a modificar (ou -1 para cancelar): ");
    if (scanf("%d", &id) != 1 || id == -1) return;

    int idx = -1;
    for (int i = 0; i < sim->task_count; i++) {
        if (sim->tasks[i].id == id) { idx = i; break; }
    }
    if (idx == -1) { printf("Tarefa nao encontrada.\n"); return; }

    printf("Estado atual: ");
    switch (sim->tasks[idx].state) {
        case NEW:      printf("NEW\n");      break;
        case READY:    printf("READY\n");    break;
        case RUNNING:  printf("RUNNING\n");  break;
        case FINISHED: printf("FINISHED\n"); break;
    }

    printf("Novo estado (0=NEW, 1=READY, 2=RUNNING, 3=FINISHED): ");
    int new_state;
    if (scanf("%d", &new_state) != 1) return;
    if (new_state < 0 || new_state > 3) { printf("Estado invalido.\n"); return; }

    sim->tasks[idx].state = (TaskState)new_state;
    printf("Estado da tarefa %d alterado.\n", id);

    printf("Novo tempo restante (atual=%d, -1 para manter): ",
           sim->tasks[idx].remaining_time);
    int new_rem;
    if (scanf("%d", &new_rem) == 1 && new_rem >= 0) {
        sim->tasks[idx].remaining_time = new_rem;
    }
}

/*
 * simulation_run_step_by_step - modo interativo com avançar, retroceder
 * e modificação de estados. Implementa os requisitos 1.5.1 e 1.5.2.
 *
 * Comandos:
 *   n / Enter - avança um tick
 *   b         - retrocede um tick
 *   m         - modifica estado de uma tarefa
 *   i         - inspeciona estado atual detalhado
 *   q         - encerra e gera SVG
 */
void simulation_run_step_by_step(SimulationState *sim) {
    printf("=== Modo passo-a-passo ===\n");
    printf("Comandos: [n]ext | [b]ack | [m]odify | [i]nspect | [q]uit\n\n");

    int snapshot_pos = -1; /* posição atual no histórico ao retroceder */
    int running = 1;

    while (running) {
        /* Imprime estado atual antes de pedir comando */
        if (snapshot_pos >= 0) {
            gantt_print_tick(&sim->history, sim->history.entries[snapshot_pos].tick,
                             sim->tasks, sim->task_count);
        }

        printf("\nTick atual: %d | Historico: %d entradas\n",
               sim->clock, sim->history.count);
        printf("Comando: ");

        char cmd[8];
        if (scanf("%7s", cmd) != 1) break;

        if (cmd[0] == 'n' || cmd[0] == '\n') {
            /* Avança */
            snapshot_pos = -1;
            if (!simulation_step(sim)) {
                printf("Simulacao concluida.\n");
                running = 0;
            }

        } else if (cmd[0] == 'b') {
            /* Retrocede - usa o histórico armazenado */
            int target = (snapshot_pos < 0)
                         ? sim->history.count - 2  /* volta do live para último */
                         : snapshot_pos - 1;

            if (target < 0) {
                printf("Nao ha historico para retroceder.\n");
            } else {
                snapshot_pos = target;
                restore_snapshot(sim, snapshot_pos);
                printf("Retrocedeu para tick %d.\n", sim->clock);
                gantt_print_tick(&sim->history, sim->clock,
                                 sim->tasks, sim->task_count);
            }

        } else if (cmd[0] == 'm') {
            /* Modifica estado de tarefa (req 3.4) */
            prompt_modify_task(sim);

        } else if (cmd[0] == 'i') {
            /* Inspeciona estado detalhado de todas as tarefas */
            printf("\n--- Estado detalhado das tarefas no tick %d ---\n", sim->clock);
            for (int i = 0; i < sim->task_count; i++) {
                const char *st;
                switch (sim->tasks[i].state) {
                    case NEW:      st = "NEW";      break;
                    case READY:    st = "READY";    break;
                    case RUNNING:  st = "RUNNING";  break;
                    case FINISHED: st = "FINISHED"; break;
                    default:       st = "?";
                }
                printf("  T%d | Estado: %-8s | Restante: %3d | Prioridade: %d | CPU: %d\n",
                       sim->tasks[i].id, st,
                       sim->tasks[i].remaining_time,
                       sim->tasks[i].priority,
                       sim->tasks[i].cpu_id);
            }
            printf("--- CPUs ---\n");
            for (int c = 0; c < sim->config.cpu_count; c++) {
                if (sim->cpus[c].active)
                    printf("  CPU %d -> Tarefa %d\n", c, sim->cpus[c].task_id);
                else
                    printf("  CPU %d -> desligada\n", c);
            }

        } else if (cmd[0] == 'q') {
            running = 0;

        } else {
            printf("Comando desconhecido. Use: n, b, m, i, q\n");
        }
    }

    /* Gera SVG ao sair (req 2.4) */
    gantt_save_svg(&sim->history, sim->tasks, sim->task_count, "gantt.svg");
    printf("Simulacao encerrada.\n");
}
