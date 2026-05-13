#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "simulation.h"

/* -----------------------------------------------------------------------
 * Funções auxiliares de entrada
 * ----------------------------------------------------------------------- */

/*
 * limpar_buffer - remove caracteres restantes do buffer de entrada stdin.
 *
 * Chamada após scanf() para evitar que o '\n' residual seja capturado
 * pela próxima leitura com fgets().
 */
void limpar_buffer(void) {
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF);
}

/*
 * ler_inteiro_com_padrao - lê um inteiro digitado pelo usuário.
 *
 * Exibe a mensagem formatada com o valor padrão entre parênteses.
 * Se o usuário pressionar ENTER sem digitar nada, mantém valor_padrao.
 *
 * Parâmetros:
 *   mensagem     - formato de printf com %d para o valor padrão
 *   valor_padrao - valor a usar se o usuário não digitar nada
 *
 * Retorna o valor digitado ou valor_padrao.
 */
int ler_inteiro_com_padrao(const char *mensagem, int valor_padrao) {
    char buffer[32];
    printf(mensagem, valor_padrao);
    printf(": ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) return valor_padrao;
    if (buffer[0] == '\n') return valor_padrao;
    return atoi(buffer);
}

/* -----------------------------------------------------------------------
 * Inicialização
 * ----------------------------------------------------------------------- */

/*
 * simulation_init - prepara o SimulationState para o início da simulação.
 *
 * Copia configurações e tarefas, inicializa CPUs e o histórico do Gantt.
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
 * Funções internas de um tick
 * ----------------------------------------------------------------------- */

/*
 * check_arrivals - verifica se alguma tarefa NEW deve ingressar no sistema.
 *
 * Tarefas com arrival_time == clock passam para o estado READY.
 * Requisito 1.4: a simulação deve respeitar o instante de ingresso.
 */
static void check_arrivals(SimulationState *sim) {
    for (int i = 0; i < sim->task_count; i++) {
        if (sim->tasks[i].state == NEW &&
            sim->tasks[i].arrival_time == sim->clock) {
            sim->tasks[i].state = READY;
            printf("  [NOVA TAREFA] T%d chegou ao sistema no tick %d.\n",
                   sim->tasks[i].id, sim->clock);
        }
    }
}

/*
 * execute_tick - executa 1 tick de processamento em cada CPU ativa.
 *
 * Decrementa remaining_time da tarefa, registra start_time na primeira
 * execução, finaliza tarefas que chegam a remaining_time == 0,
 * e incrementa ticks_this_slice para controle do quantum.
 *
 * Retorna 1 se alguma CPU estava ativa, 0 se todas estavam desligadas.
 */
static int execute_tick(SimulationState *sim) {
    int any_active = 0;

    for (int c = 0; c < sim->config.cpu_count; c++) {
        if (!sim->cpus[c].active || sim->cpus[c].task_id == -1) {
            /* CPU desligada: acumula tempo ocioso */
            sim->cpus[c].idle_time++;
            continue;
        }

        any_active = 1;

        /* Localiza a tarefa desta CPU no vetor */
        for (int i = 0; i < sim->task_count; i++) {
            if (sim->tasks[i].id != sim->cpus[c].task_id) continue;

            /* Registra o instante de início (apenas na primeira execução) */
            if (sim->tasks[i].start_time == -1) {
                sim->tasks[i].start_time = sim->clock;
            }

            /* Executa 1 tick */
            sim->tasks[i].remaining_time--;
            sim->tasks[i].ticks_this_slice++;

            /* Verifica término */
            if (sim->tasks[i].remaining_time <= 0) {
                sim->tasks[i].state        = FINISHED;
                sim->tasks[i].finish_time  = sim->clock;
                sim->tasks[i].cpu_id       = -1;
                sim->tasks[i].ticks_this_slice = 0;

                sim->cpus[c].task_id = -1;
                sim->cpus[c].active  = 0;

                printf("  [CONCLUIDA]  T%d terminou no tick %d.\n",
                       sim->tasks[i].id, sim->clock);
            }
            break;
        }
    }

    return any_active;
}

/*
 * has_pending_tasks - retorna 1 se ainda existem tarefas não finalizadas.
 */
static int has_pending_tasks(SimulationState *sim) {
    for (int i = 0; i < sim->task_count; i++) {
        if (sim->tasks[i].state != FINISHED) return 1;
    }
    return 0;
}

/*
 * print_cpu_status - exibe o estado atual de cada CPU no terminal.
 * Chamada após o escalonador distribuir as tarefas.
 */
static void print_cpu_status(SimulationState *sim) {
    for (int c = 0; c < sim->config.cpu_count; c++) {
        if (sim->cpus[c].active) {
            printf("  CPU %d  -> T%d\n", c, sim->cpus[c].task_id);
        } else {
            printf("  CPU %d  -> desligada (ocioso: %d ticks)\n",
                   c, sim->cpus[c].idle_time);
        }
    }
}

/* -----------------------------------------------------------------------
 * Passo único de simulação
 * ----------------------------------------------------------------------- */

/*
 * simulation_step - executa exatamente um tick completo da simulação.
 *
 * Sequência de operações por tick (conforme modelo de SO estudado):
 *   1. Verifica chegadas (NEW → READY)
 *   2. Escalonador distribui tarefas entre as CPUs
 *   3. Executa 1 tick de processamento (decrementa remaining_time)
 *   4. Registra snapshot no histórico do Gantt
 *   5. Avança o relógio global
 *
 * Retorna 1 se a simulação continua, 0 se todas as tarefas terminaram.
 */
int simulation_step(SimulationState *sim) {
    if (!has_pending_tasks(sim)) return 0;

    printf("\n--- Tick %d ---\n", sim->clock);

    /* Passo 1: tarefas que chegam neste tick */
    check_arrivals(sim);

    /* Passo 2: escalonador atribui tarefas às CPUs */
    int lottery = assign_tasks(sim->config.algorithm,
                               sim->tasks, sim->task_count,
                               sim->cpus, sim->config.cpu_count,
                               sim->config.quantum,
                               sim->clock);

    if (lottery) {
        printf("  [SORTEIO]    Desempate aleatorio ocorreu neste tick.\n");
    }

    /* Exibe estado das CPUs após escalonamento */
    print_cpu_status(sim);

    /* Passo 3: processa 1 tick em cada CPU ativa */
    execute_tick(sim);

    /* Passo 4: registra snapshot no histórico do Gantt */
    gantt_record(&sim->history, sim->clock,
                 sim->cpus, sim->config.cpu_count,
                 sim->tasks, sim->task_count,
                 lottery);

    /* Passo 5: avança relógio */
    sim->clock++;

    return has_pending_tasks(sim);
}

/* -----------------------------------------------------------------------
 * Modo de execução completa (req 1.5.3)
 * ----------------------------------------------------------------------- */

/*
 * simulation_run_complete - executa todos os ticks sem interação humana.
 *
 * Ao final:
 *  - Exibe o gráfico de Gantt completo no terminal (req 2.1–2.3)
 *  - Exibe tempo ocioso de cada CPU (req 1.2)
 *  - Gera o arquivo SVG do Gantt (req 2.4)
 */
void simulation_run_complete(SimulationState *sim) {
    printf("=== Modo de Execucao Completa ===\n");

    /* Executa todos os ticks até não haver mais tarefas pendentes */
    while (simulation_step(sim)) { /* vazio: simulation_step faz tudo */ }

    printf("\n=== Simulacao concluida no tick %d ===\n\n", sim->clock);

    /* Exibe tempo ocioso de cada CPU (req 1.2) */
    printf("Tempo ocioso por CPU:\n");
    for (int c = 0; c < sim->config.cpu_count; c++) {
        printf("  CPU %d: %d tick(s) desligada\n", c, sim->cpus[c].idle_time);
    }

    /* Exibe resumo das tarefas */
    printf("\nResumo das tarefas:\n");
    for (int i = 0; i < sim->task_count; i++) {
        printf("  T%d | inicio: %d | fim: %d | turnaround: %d\n",
               sim->tasks[i].id,
               sim->tasks[i].start_time,
               sim->tasks[i].finish_time,
               sim->tasks[i].finish_time - sim->tasks[i].arrival_time);
    }

    /* Exibe o gráfico de Gantt completo no terminal (req 2.1, 2.3) */
    gantt_print_terminal(&sim->history, sim->tasks, sim->task_count);

    /* Gera o arquivo SVG final (req 2.4) */
    gantt_save_svg(&sim->history, sim->tasks, sim->task_count, "gantt.svg");
}

/* -----------------------------------------------------------------------
 * Modo passo-a-passo (req 1.5.1 e 1.5.2)
 * ----------------------------------------------------------------------- */

/*
 * restore_snapshot - restaura o estado da simulação a partir do snapshot
 * no índice snapshot_index do histórico.
 *
 * Restaura: relógio, estado e remaining_time das tarefas, e estado das CPUs.
 * Usado ao retroceder a simulação (req 1.5.2).
 */
static void restore_snapshot(SimulationState *sim, int snapshot_index) {
    if (snapshot_index < 0 || snapshot_index >= sim->history.count) return;

    const GanttEntry *e = &sim->history.entries[snapshot_index];

    /* Retrocede o relógio para o tick deste snapshot */
    sim->clock = e->tick;

    /* Restaura estado e tempo restante de cada tarefa */
    for (int i = 0; i < sim->task_count; i++) {
        sim->tasks[i].state          = e->task_state[i];
        sim->tasks[i].remaining_time = e->task_remaining[i];
    }

    /* Restaura estado de cada CPU */
    for (int c = 0; c < sim->config.cpu_count; c++) {
        sim->cpus[c].task_id = e->cpu_task[c];
        sim->cpus[c].active  = e->cpu_active[c];
    }
}

/*
 * prompt_modify_task - permite ao usuário modificar manualmente o estado
 * de uma tarefa durante a simulação (requisito 3.4).
 *
 * O usuário escolhe o ID da tarefa e pode alterar o estado e o tempo restante.
 */
static void prompt_modify_task(SimulationState *sim) {
    int id;
    printf("Digite o ID da tarefa a modificar (ou -1 para cancelar): ");
    if (scanf("%d", &id) != 1) { limpar_buffer(); return; }
    limpar_buffer();

    if (id == -1) return;

    /* Procura a tarefa pelo ID */
    int idx = -1;
    for (int i = 0; i < sim->task_count; i++) {
        if (sim->tasks[i].id == id) { idx = i; break; }
    }
    if (idx == -1) {
        printf("Tarefa T%d nao encontrada.\n", id);
        return;
    }

    /* Exibe estado atual */
    const char *estados[] = { "NEW", "READY", "RUNNING", "SUSPENDED", "FINISHED" };
    printf("Estado atual de T%d: %s | Tempo restante: %d\n",
           id, estados[sim->tasks[idx].state], sim->tasks[idx].remaining_time);

    /* Pede novo estado */
    printf("Novo estado (0=NEW, 1=READY, 2=RUNNING, 3=SUSPENDED, 4=FINISHED, -1=manter): ");
    int new_state;
    if (scanf("%d", &new_state) != 1) { limpar_buffer(); return; }
    limpar_buffer();

    if (new_state >= 0 && new_state <= 4) {
        sim->tasks[idx].state = (TaskState)new_state;
        printf("Estado de T%d alterado para %s.\n", id, estados[new_state]);
    }

    /* Pede novo tempo restante */
    printf("Novo tempo restante (atual=%d, -1=manter): ",
           sim->tasks[idx].remaining_time);
    int new_rem;
    if (scanf("%d", &new_rem) == 1 && new_rem >= 0) {
        sim->tasks[idx].remaining_time = new_rem;
        printf("Tempo restante de T%d alterado para %d.\n", id, new_rem);
    }
    limpar_buffer();
}

/*
 * inspect_state - exibe o estado detalhado de todas as tarefas e CPUs.
 * Funciona como "debugger" do sistema (requisito 1.5.1).
 */
static void inspect_state(SimulationState *sim) {
    printf("\n--- Estado do sistema no tick %d ---\n", sim->clock);

    printf("Tarefas:\n");
    const char *estados[] = { "NEW", "READY", "RUNNING", "SUSPENDED", "FINISHED" };
    for (int i = 0; i < sim->task_count; i++) {
        int si = (int)sim->tasks[i].state;
        if (si < 0 || si > 4) si = 0;
        printf("  T%-3d | %-9s | Restante: %3d | Prioridade: %2d | CPU: %2d | Slice: %d/%d\n",
               sim->tasks[i].id,
               estados[si],
               sim->tasks[i].remaining_time,
               sim->tasks[i].priority,
               sim->tasks[i].cpu_id,
               sim->tasks[i].ticks_this_slice,
               sim->config.quantum);
    }

    printf("CPUs:\n");
    for (int c = 0; c < sim->config.cpu_count; c++) {
        if (sim->cpus[c].active) {
            printf("  CPU %d -> T%d\n", c, sim->cpus[c].task_id);
        } else {
            printf("  CPU %d -> desligada (%d ticks ociosa)\n",
                   c, sim->cpus[c].idle_time);
        }
    }
    printf("---\n");
}

/*
 * simulation_run_step_by_step - modo interativo com avançar, retroceder,
 * modificação de estados e inspeção detalhada (req 1.5.1, 1.5.2).
 *
 * Comandos disponíveis:
 *   n / ENTER  - avança um tick e exibe o Gantt atualizado no terminal
 *   b          - retrocede um tick (usa histórico armazenado)
 *   m          - modifica estado de uma tarefa manualmente (req 3.4)
 *   i          - inspeciona estado detalhado (debugger, req 1.5.1)
 *   q          - encerra e gera o SVG final
 */
void simulation_run_step_by_step(SimulationState *sim) {
    printf("=== Modo Passo-a-Passo ===\n");
    printf("Comandos: [n]ext | [b]ack | [m]odify | [i]nspect | [q]uit\n\n");

    int snapshot_pos = -1; /* índice no histórico ao navegar; -1 = live */
    int running = 1;

    while (running) {
        printf("\nTick atual: %d | Historico: %d entradas\n",
               sim->clock, sim->history.count);
        printf("Comando: ");

        char cmd[8];
        if (scanf("%7s", cmd) != 1) break;
        limpar_buffer();

        if (cmd[0] == 'n') {
            /*
             * Avança um tick.
             * Se estava em modo de retrocesso, descarta os snapshots
             * mais recentes que o ponto atual e retoma a execução normal.
             */
            if (snapshot_pos >= 0) {
                sim->history.count = snapshot_pos + 1;
                restore_snapshot(sim, snapshot_pos);
                sim->clock = sim->history.entries[snapshot_pos].tick + 1;
                snapshot_pos = -1;
            }

            if (!simulation_step(sim)) {
                printf("\n[SIMULACAO CONCLUIDA]\n");

                /* Resumo final */
                printf("\nResumo das tarefas:\n");
                for (int i = 0; i < sim->task_count; i++) {
                    printf("  T%d | inicio: %d | fim: %d | turnaround: %d\n",
                           sim->tasks[i].id,
                           sim->tasks[i].start_time,
                           sim->tasks[i].finish_time,
                           sim->tasks[i].finish_time - sim->tasks[i].arrival_time);
                }

                /* Exibe gráfico de Gantt completo no terminal (req 2.3) */
                gantt_print_terminal(&sim->history, sim->tasks, sim->task_count);
                running = 0;
            } else {
                /* Exibe gráfico atualizado no terminal a cada passo (req 2.3) */
                gantt_print_terminal(&sim->history, sim->tasks, sim->task_count);
            }

        } else if (cmd[0] == 'b') {
            /*
             * Retrocede um tick usando o histórico (req 1.5.2).
             * Retroceder impacta o relógio global.
             */
            int target = (snapshot_pos < 0)
                         ? sim->history.count - 2  /* do live: vai para o último */
                         : snapshot_pos - 1;        /* do histórico: volta mais um */

            if (target < 0) {
                printf("Nao ha historico para retroceder.\n");
            } else {
                snapshot_pos = target;
                restore_snapshot(sim, snapshot_pos);
                printf("Retrocedeu para tick %d.\n",
                       sim->history.entries[snapshot_pos].tick);
                /* Exibe o Gantt deste ponto histórico (req 2.3) */
                gantt_print_terminal(&sim->history, sim->tasks, sim->task_count);
            }

        } else if (cmd[0] == 'm') {
            /* Modifica estado de uma tarefa (req 3.4) */
            prompt_modify_task(sim);

        } else if (cmd[0] == 'i') {
            /* Inspeciona estado detalhado (debugger, req 1.5.1) */
            inspect_state(sim);

        } else if (cmd[0] == 'q') {
            running = 0;

        } else {
            printf("Comando desconhecido. Use: n, b, m, i, q\n");
        }
    }

    /* Exibe tempo ocioso das CPUs */
    printf("\nTempo ocioso por CPU:\n");
    for (int c = 0; c < sim->config.cpu_count; c++) {
        printf("  CPU %d: %d tick(s) desligada\n", c, sim->cpus[c].idle_time);
    }

    /* Gera SVG ao encerrar (req 2.4) */
    gantt_save_svg(&sim->history, sim->tasks, sim->task_count, "gantt.svg");
    printf("Simulacao encerrada.\n");
}