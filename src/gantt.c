#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gantt.h"

/* -----------------------------------------------------------------------
 * Constantes de layout do gráfico terminal
 * ----------------------------------------------------------------------- */
/* Largura de cada célula de tick no terminal (em caracteres) */
#define TERM_CELL_W  4

/* -----------------------------------------------------------------------
 * Constantes de layout do SVG (em pixels)
 * ----------------------------------------------------------------------- */
#define SVG_TICK_W      44   /* Largura de cada tick no SVG */
#define SVG_ROW_H       36   /* Altura de cada linha de tarefa */
#define SVG_LABEL_W    115   /* Largura da coluna de labels (eixo Y) */
#define SVG_MARGIN_TOP  62   /* Margem superior (título + eixo de tempo) */
#define SVG_LEGEND_H   240   /* Altura da área de legenda abaixo do gráfico */
#define SVG_GAP          4   /* Espaço interno entre células */

/* -----------------------------------------------------------------------
 * Funções auxiliares
 * ----------------------------------------------------------------------- */

/*
 * hex_to_rgb - converte string hexadecimal RGB (ex: "FF0000") em componentes.
 * Usada para aplicar cores de tarefas no terminal (ANSI) e no SVG.
 */
static void hex_to_rgb(const char *hex, int *r, int *g, int *b) {
    unsigned int ur = 0xAA, ug = 0xAA, ub = 0xCC; /* padrão cinza-azulado */
    sscanf(hex, "%02x%02x%02x", &ur, &ug, &ub);
    *r = (int)ur;
    *g = (int)ug;
    *b = (int)ub;
}

/*
 * sort_tasks_by_id - preenche sorted[] com os índices das tarefas ordenados
 * pelo ID em ordem crescente.
 *
 * Requisito 2.5: eixo Y decrescente em relação ao ID, ou seja, o menor ID
 * fica mais próximo do eixo X. Portanto, ao exibir de cima para baixo,
 * a ordem de exibição é do maior ID para o menor.
 */
static void sort_tasks_by_id(Task tasks[], int task_count, int sorted[]) {
    for (int i = 0; i < task_count; i++) sorted[i] = i;
    /* Bubble sort simples por ID crescente */
    for (int i = 0; i < task_count - 1; i++) {
        for (int j = i + 1; j < task_count; j++) {
            if (tasks[sorted[i]].id > tasks[sorted[j]].id) {
                int tmp   = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }
}

/*
 * brightness - calcula luminância aproximada de uma cor RGB (0–255).
 * Usado para decidir se o texto sobre o bloco colorido deve ser
 * preto ou branco, garantindo legibilidade.
 */
static int brightness(int r, int g, int b) {
    return (r * 299 + g * 587 + b * 114) / 1000;
}

/* -----------------------------------------------------------------------
 * Inicialização do histórico
 * ----------------------------------------------------------------------- */

/*
 * gantt_init - inicializa o histórico do Gantt com contadores zerados.
 */
void gantt_init(GanttHistory *history, int cpu_count, int task_count) {
    memset(history, 0, sizeof(*history));
    history->count      = 0;
    history->cpu_count  = cpu_count;
    history->task_count = task_count;
}

/* -----------------------------------------------------------------------
 * Registro de snapshot
 * ----------------------------------------------------------------------- */

/*
 * gantt_record - captura um snapshot do estado do sistema no tick atual.
 *
 * Armazena estado de cada CPU, estado/tempo_restante de cada tarefa,
 * e flags de eventos (chegada, fim, sorteio).
 */
void gantt_record(GanttHistory *history, int tick,
                  CPU cpus[], int cpu_count,
                  Task tasks[], int task_count,
                  int lottery_used) {
    if (history->count >= MAX_TICKS) {
        fprintf(stderr, "Aviso: historico do Gantt cheio (MAX_TICKS=%d).\n", MAX_TICKS);
        return;
    }

    GanttEntry *e = &history->entries[history->count];
    memset(e, 0, sizeof(*e));

    e->tick         = tick;
    e->lottery_tick = lottery_used;

    /* Inicializa cpu_task[] como -1 (CPU desligada) */
    for (int c = 0; c < MAX_CPUS; c++) e->cpu_task[c] = -1;

    /* Registra estado de cada CPU neste tick */
    for (int c = 0; c < cpu_count && c < MAX_CPUS; c++) {
        e->cpu_task[c]   = cpus[c].task_id;
        e->cpu_active[c] = cpus[c].active;
    }

    /* Registra estado e tempo restante de cada tarefa */
    for (int i = 0; i < task_count && i < MAX_TASKS; i++) {
        e->task_state[i]     = tasks[i].state;
        e->task_remaining[i] = tasks[i].remaining_time;
		e->task_slice[i] = tasks[i].ticks_this_slice;
        /* Chegada: tarefa tornou-se READY neste tick (arrival_time == tick) */
        e->task_arrived[i]  = (tasks[i].arrival_time == tick &&
                                tasks[i].state != NEW) ? 1 : 0;

        /* Fim: tarefa acabou exatamente neste tick */
        e->task_finished[i] = (tasks[i].state == FINISHED &&
                                tasks[i].finish_time == tick) ? 1 : 0;
    }

    history->count++;
}

/* -----------------------------------------------------------------------
 * Impressão no terminal com gráfico de Gantt colorido (req 2.1–2.3)
 * ----------------------------------------------------------------------- */

/*
 * print_separator - imprime uma linha separadora horizontal do gráfico.
 */
static void print_separator(int task_label_w, int ticks) {
    for (int i = 0; i < task_label_w; i++) printf("-");
    printf("+");
    for (int t = 0; t < ticks; t++) {
        for (int i = 0; i < TERM_CELL_W; i++) printf("-");
        printf("+");
    }
    printf("\n");
}

/*
 * gantt_print_terminal - exibe o gráfico de Gantt completo no terminal.
 *
 * Formato visual (req 2.1, 2.2, 2.3):
 *
 *   === Gráfico de Gantt (Tick N | Algoritmo: ALGO) ===
 *
 *         |  0 |  1 |  2 |  3 |  4 |
 *   ------+----+----+----+----+----+
 *     T3  |    |    |████|████|    |  ↓2
 *     T2  |░░░░|░░░░|░░░░|████|████|  ↓0
 *     T1  |████|████|████|    |░░░░|  ↓0 ✓3
 *   ------+----+----+----+----+----+
 *    CPU0 | T1 | T1 | T3 | T2 | T2 |
 *    CPU1 | T2 | T2 | T2 | T1 |----| (---- = desligada)
 *
 *   Legenda: [Cx] Executando  ░░░░ Pronto  ████ Suspenso  ↓ Chegada  ✓ Fim
 *
 * Cores ANSI 24-bit usadas para os blocos de execução (req 2.1).
 * Ícones de chegada (↓) e fim (✓) respeitam o req 2.2.
 * Atualização a cada passo respeita o req 2.3.
 */
void gantt_print_terminal(const GanttHistory *history, Task tasks[], int task_count) {
    if (history->count == 0) return;

    int ticks        = history->count;
    int cpu_count    = history->cpu_count;
    int last_tick    = history->entries[ticks - 1].tick;

    /* Largura do label lateral ("  T999  ") */
    const int LABEL_W = 8;

    /* ---- Título ---- */
    printf("\n\033[1;36m");
    printf("=== Grafico de Gantt | Tick: %d ", last_tick);
    printf("===\033[0m\n\n");

    /* ---- Linha de números de tick ---- */
    printf("%*s|", LABEL_W, "");
    for (int t = 0; t < ticks; t++) {
        printf("\033[90m%*d\033[0m|", TERM_CELL_W, history->entries[t].tick);
    }
    printf("\n");

    /* ---- Separador ---- */
    print_separator(LABEL_W, ticks);

    /* ---- Linhas das tarefas ----
     * Requisito 2.5: ID menor fica na parte de baixo do gráfico.
     * Exibimos do maior ID (topo) para o menor ID (base).
     */
    int sorted[MAX_TASKS];
    sort_tasks_by_id(tasks, task_count, sorted);

    for (int row = task_count - 1; row >= 0; row--) {
        /* row = task_count-1 → maior ID (topo); row = 0 → menor ID (base) */
        int tidx = sorted[row];
        int r, g, b;
        hex_to_rgb(tasks[tidx].color, &r, &g, &b);

        /* Cor do texto sobre o bloco: preto em fundo claro, branco em fundo escuro */
        const char *txt_color = (brightness(r, g, b) > 128) ? "\033[30m" : "\033[97m";

        /* Label da tarefa */
        printf("  T%-3d   |", tasks[tidx].id);

        /* Células tick a tick */
        for (int t = 0; t < ticks; t++) {
            const GanttEntry *e = &history->entries[t];

            /* Verifica se esta tarefa está rodando em alguma CPU */
            int on_cpu = -1;
            for (int c = 0; c < cpu_count; c++) {
                if (e->cpu_task[c] == tasks[tidx].id && e->cpu_active[c]) {
                    on_cpu = c;
                    break;
                }
            }

            TaskState ts = e->task_state[tidx];

            if (on_cpu >= 0) {
                /*
                 * EXECUTANDO: fundo com a cor da tarefa (req 2.1).
                 * Texto mostra "Cx" onde x é o número da CPU.
                 * Ícone de sorteio ★ se houve neste tick (req 4.3).
                 */
                if (e->lottery_tick) {
                    /* Sorteio: adiciona ★ no final da célula */
                    printf("\033[48;2;%d;%d;%dm%sC%d \033[0m\033[35m*\033[0m",
                           r, g, b, txt_color, on_cpu);
                } else {
                    printf("\033[48;2;%d;%d;%dm%s C%d \033[0m",
                           r, g, b, txt_color, on_cpu);
                }
            } else if (ts == READY) {
                /*
                 * PRONTA (na fila de prontos): ausência de cor (req 2.1).
                 * Representamos como fundo cinza escuro com caracteres ░.
                 */
                printf("\033[48;2;55;55;75m\033[90m░░░░\033[0m");
            } else if (ts == SUSPENDED) {
                /*
                 * SUSPENSA (por qualquer motivo): cor preta (req 2.1).
                 */
                printf("\033[48;2;25;25;25m\033[90m████\033[0m");
            } else if (ts == FINISHED) {
                if (e->task_finished[tidx]) {
                    /*
                     * Tick exato em que terminou: mostra ícone ✓ (req 2.2).
                     * Fundo preto conforme req 2.1 (tarefa suspensa/terminada).
                     */
                    printf("\033[48;2;20;20;20m\033[1;33m ✓  \033[0m");
                } else {
                    /* Após terminar: célula preta/vazia */
                    printf("\033[48;2;10;10;10m    \033[0m");
                }
            } else {
                /* NEW: tarefa ainda não chegou ao sistema */
                printf("    ");
            }
            printf("|");
        }

        /* ---- Anotações de eventos à direita da linha (req 2.2) ---- */
        int arrived_tick  = -1;
        int finished_tick = -1;
        for (int t = 0; t < ticks; t++) {
            if (history->entries[t].task_arrived[tidx])  arrived_tick  = history->entries[t].tick;
            if (history->entries[t].task_finished[tidx]) finished_tick = history->entries[t].tick;
        }
        if (arrived_tick  >= 0) printf("  \033[1;32m↓t=%d\033[0m", arrived_tick);
        if (finished_tick >= 0) printf("  \033[1;33m✓t=%d\033[0m", finished_tick);
        printf("\n");
    }

    /* ---- Separador ---- */
    print_separator(LABEL_W, ticks);

    /* ---- Linhas de status das CPUs ---- */
    for (int c = 0; c < cpu_count; c++) {
        printf("  CPU%-3d |", c);
        for (int t = 0; t < ticks; t++) {
            const GanttEntry *e = &history->entries[t];
            if (!e->cpu_active[c]) {
                /* CPU desligada: mostra "----" em vermelho */
                printf("\033[31m----\033[0m|");
            } else if (e->cpu_task[c] != -1) {
                /* CPU ativa: mostra o ID da tarefa em execução */
                printf("\033[36m T%-2d\033[0m|", e->cpu_task[c]);
            } else {
                printf("    |");
            }
        }
        printf("\n");
    }

    printf("\n");

    /* ---- Legenda ---- */
    printf("\033[1mLegenda:\033[0m  ");
    printf("\033[48;2;100;150;220m\033[30m Cx \033[0m Exec.(CPU x)   ");
    printf("\033[48;2;55;55;75m\033[90m░░░░\033[0m Pronto   ");
    printf("\033[48;2;25;25;25m\033[90m████\033[0m Suspenso   ");
    printf("     Nao chegou   ");
    printf("\033[31m----\033[0m CPU desligada\n");
    printf("          ");
    printf("\033[1;32m↓t=N\033[0m Chegada no tick N   ");
    printf("\033[1;33m✓t=N\033[0m Fim no tick N   ");
    printf("\033[35m*\033[0m Sorteio\n\n");
}

/* -----------------------------------------------------------------------
 * Geração do arquivo SVG (req 2.4)
 * ----------------------------------------------------------------------- */

/*
 * write_svg_defs - escreve a seção <defs> do SVG.
 * Define padrão hachurado para CPUs desligadas e marcadores reutilizáveis.
 */
static void write_svg_defs(FILE *f) {
    fprintf(f,
        "  <defs>\n"
        "    <!-- Padrão hachurado para células de CPU desligada -->\n"
        "    <pattern id='hatch_off' patternUnits='userSpaceOnUse' width='6' height='6'>\n"
        "      <rect width='6' height='6' fill='#333333'/>\n"
        "      <line x1='0' y1='6' x2='6' y2='0' stroke='#555555' stroke-width='1'/>\n"
        "    </pattern>\n"
        "    <!-- Padrão pontilhado para tarefas prontas (fila) -->\n"
        "    <pattern id='dots_ready' patternUnits='userSpaceOnUse' width='4' height='4'>\n"
        "      <rect width='4' height='4' fill='#1a1a3a'/>\n"
        "      <circle cx='2' cy='2' r='0.8' fill='#4a4a6a'/>\n"
        "    </pattern>\n"
        "  </defs>\n"
    );
}

/*
 * gantt_save_svg - gera o arquivo SVG do gráfico de Gantt final.
 *
 * Layout:
 *   - Título e eixo de tempo no topo
 *   - Uma linha por tarefa (maior ID no topo, menor ID na base – req 2.5)
 *   - Células coloridas = executando; pontilhado = pronto; preto = suspenso
 *   - Ícones: ↓ chegada, ✓ fim, ★ sorteio (req 2.2, 4.3)
 *   - Linhas de CPU abaixo das tarefas
 *   - Legenda completa no rodapé
 */
void gantt_save_svg(const GanttHistory *history, Task tasks[],
                    int task_count, const char *filename) {
    if (history->count == 0) {
        fprintf(stderr, "Aviso: historico vazio, SVG nao gerado.\n");
        return;
    }

    FILE *f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Erro: nao foi possivel criar '%s'.\n", filename);
        return;
    }

    int ticks     = history->count;
    int cpu_count = history->cpu_count;

    /* Dimensões do SVG */
    int width  = SVG_LABEL_W + ticks * SVG_TICK_W + 30;
    int height = SVG_MARGIN_TOP
               + task_count * SVG_ROW_H
               + cpu_count  * SVG_ROW_H   /* linhas de CPU */
               + 20                        /* separador */
               + SVG_LEGEND_H;

    /* Cabeçalho SVG */
    fprintf(f,
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<svg xmlns='http://www.w3.org/2000/svg' width='%d' height='%d' "
        "font-family='monospace' font-size='12'>\n",
        width, height
    );

    write_svg_defs(f);

    /* Fundo geral */
    fprintf(f, "  <rect width='%d' height='%d' fill='#1a1a2e'/>\n", width, height);

    /* Título */
    fprintf(f,
        "  <text x='%d' y='22' fill='#e0e0f0' font-size='15' font-weight='bold' "
        "text-anchor='middle'>Grafico de Gantt - Simulador SO</text>\n",
        width / 2
    );

    /* ---- Eixo de tempo ---- */
    for (int t = 0; t < ticks; t++) {
        int x = SVG_LABEL_W + t * SVG_TICK_W;
        fprintf(f,
            "  <text x='%d' y='44' fill='#888899' text-anchor='middle' font-size='11'>%d</text>\n",
            x + SVG_TICK_W / 2, history->entries[t].tick
        );
        /* Linha vertical de grade */
        fprintf(f,
            "  <line x1='%d' y1='48' x2='%d' y2='%d' "
            "stroke='#2a2a4a' stroke-width='0.5'/>\n",
            x, x, SVG_MARGIN_TOP + (task_count + cpu_count) * SVG_ROW_H + 10
        );
    }

    /* Ordena tarefas por ID */
    int sorted[MAX_TASKS];
    sort_tasks_by_id(tasks, task_count, sorted);

    /* ---- Linhas das tarefas ---- */
    fprintf(f, "  <!-- === Linhas das tarefas === -->\n");

    for (int row = 0; row < task_count; row++) {
        /*
         * Requisito 2.5: ID menor fica mais próximo do eixo X.
         * Iteramos sorted de row=task_count-1 (maior ID, topo) até row=0 (menor ID, base).
         */
        int display_row = task_count - 1 - row; /* inverte para req 2.5 */
        int tidx  = sorted[row];
        int y     = SVG_MARGIN_TOP + display_row * SVG_ROW_H;
        int r_col, g_col, b_col;
        hex_to_rgb(tasks[tidx].color, &r_col, &g_col, &b_col);

        /* Label da tarefa */
        fprintf(f,
            "  <text x='%d' y='%d' fill='#ccccdd' text-anchor='end' font-size='12'>"
            "T%d</text>\n",
            SVG_LABEL_W - 6, y + SVG_ROW_H / 2 + 4, tasks[tidx].id
        );

        /* Linha de fundo da tarefa */
        fprintf(f,
            "  <rect x='%d' y='%d' width='%d' height='%d' fill='#0e0e20' rx='2'/>\n",
            SVG_LABEL_W, y + SVG_GAP,
            ticks * SVG_TICK_W, SVG_ROW_H - SVG_GAP * 2
        );

        /* Células tick a tick */
        for (int t = 0; t < ticks; t++) {
            const GanttEntry *e = &history->entries[t];
            int x  = SVG_LABEL_W + t * SVG_TICK_W;
            int cw = SVG_TICK_W - 1;
            int ch = SVG_ROW_H - SVG_GAP * 2 - 2;
            int cy = y + SVG_GAP + 1;

            /* Verifica se a tarefa está em execução */
            int on_cpu = -1;
            for (int c = 0; c < cpu_count; c++) {
                if (e->cpu_task[c] == tasks[tidx].id && e->cpu_active[c]) {
                    on_cpu = c;
                    break;
                }
            }

            TaskState ts = e->task_state[tidx];

            if (on_cpu >= 0) {
                /* EXECUTANDO: fundo colorido com ID da CPU (req 2.1) */
                fprintf(f,
                    "  <rect x='%d' y='%d' width='%d' height='%d' "
                    "fill='rgb(%d,%d,%d)' rx='2'/>\n",
                    x + 1, cy, cw, ch, r_col, g_col, b_col
                );
                /* Texto da CPU sobre o bloco */
                const char *txt_fill = (brightness(r_col, g_col, b_col) > 128)
                                       ? "#111111" : "#eeeeee";
                fprintf(f,
                    "  <text x='%d' y='%d' fill='%s' font-size='10' "
                    "text-anchor='middle' font-weight='bold'>C%d</text>\n",
                    x + cw / 2, cy + ch / 2 + 4, txt_fill, on_cpu
                );
            } else if (ts == READY) {
                /* PRONTA: padrão pontilhado (req 2.1) */
                fprintf(f,
                    "  <rect x='%d' y='%d' width='%d' height='%d' "
                    "fill='url(#dots_ready)' rx='2'/>\n",
                    x + 1, cy, cw, ch
                );
            } else if (ts == SUSPENDED) {
                /* SUSPENSA: fundo preto (req 2.1) */
                fprintf(f,
                    "  <rect x='%d' y='%d' width='%d' height='%d' "
                    "fill='#111111' rx='2'/>\n",
                    x + 1, cy, cw, ch
                );
            } else if (ts == FINISHED) {
                /* TERMINADA: fundo muito escuro */
                fprintf(f,
                    "  <rect x='%d' y='%d' width='%d' height='%d' "
                    "fill='#080808' rx='2'/>\n",
                    x + 1, cy, cw, ch
                );
            }
            /* NEW: sem renderização (tarefa ainda não chegou) */

            /* Ícone de chegada ↓ (req 2.2) */
            if (e->task_arrived[tidx]) {
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#00ee55' font-size='13' "
                    "text-anchor='middle'>&#x2193;</text>\n",
                    x + SVG_TICK_W / 2, cy - 1
                );
            }

            /* Ícone de fim ✓ (req 2.2) */
            if (e->task_finished[tidx]) {
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#ffcc00' font-size='12' "
                    "text-anchor='middle'>&#x2713;</text>\n",
                    x + SVG_TICK_W / 2, cy + ch / 2 + 4
                );
            }

            /* Ícone de sorteio ★ (req 4.3) */
            if (e->lottery_tick && on_cpu >= 0) {
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#ee44ff' font-size='9' "
                    "text-anchor='middle'>&#x2605;</text>\n",
                    x + cw - 3, cy + 9
                );
            }
        }
    }

    /* ---- Linhas de status das CPUs ---- */
    int cpu_y_offset = SVG_MARGIN_TOP + task_count * SVG_ROW_H + 10;

    fprintf(f, "  <!-- === Linhas de status das CPUs === -->\n");
    fprintf(f,
        "  <line x1='%d' y1='%d' x2='%d' y2='%d' "
        "stroke='#3a3a5a' stroke-width='1'/>\n",
        SVG_LABEL_W, cpu_y_offset - 4,
        SVG_LABEL_W + ticks * SVG_TICK_W, cpu_y_offset - 4
    );

    for (int c = 0; c < cpu_count; c++) {
        int y = cpu_y_offset + c * SVG_ROW_H;

        /* Label da CPU */
        fprintf(f,
            "  <text x='%d' y='%d' fill='#aaaacc' text-anchor='end' font-size='11'>"
            "CPU%d</text>\n",
            SVG_LABEL_W - 6, y + SVG_ROW_H / 2 + 4, c
        );

        /* Fundo da linha de CPU */
        fprintf(f,
            "  <rect x='%d' y='%d' width='%d' height='%d' fill='#0a0a1a' rx='2'/>\n",
            SVG_LABEL_W, y + SVG_GAP,
            ticks * SVG_TICK_W, SVG_ROW_H - SVG_GAP * 2
        );

        for (int t = 0; t < ticks; t++) {
            const GanttEntry *e = &history->entries[t];
            int x  = SVG_LABEL_W + t * SVG_TICK_W;
            int cw = SVG_TICK_W - 1;
            int ch = SVG_ROW_H - SVG_GAP * 2 - 2;
            int cy = y + SVG_GAP + 1;

            if (!e->cpu_active[c]) {
                /* CPU desligada: padrão hachurado vermelho escuro (req 1.2) */
                fprintf(f,
                    "  <rect x='%d' y='%d' width='%d' height='%d' "
                    "fill='url(#hatch_off)' rx='2'/>\n",
                    x + 1, cy, cw, ch
                );
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#cc3333' font-size='9' "
                    "text-anchor='middle'>OFF</text>\n",
                    x + cw / 2, cy + ch / 2 + 3
                );
            } else if (e->cpu_task[c] != -1) {
                /* Mostra o ID da tarefa que está nesta CPU */
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#55bbee' font-size='10' "
                    "text-anchor='middle'>T%d</text>\n",
                    x + cw / 2, cy + ch / 2 + 3, e->cpu_task[c]
                );
            }
        }
    }

    /* ---- Legenda ---- */
    int ly = cpu_y_offset + cpu_count * SVG_ROW_H + 28;
    fprintf(f, "  <!-- === Legenda === -->\n");
    fprintf(f,
        "  <text x='12' y='%d' fill='#ffffff' font-size='13' font-weight='bold'>"
        "Legenda:</text>\n", ly
    );
    ly += 20;

    /* Executando */
    fprintf(f,
        "  <rect x='12' y='%d' width='24' height='14' fill='#3399ff' rx='2'/>\n"
        "  <text x='12' y='%d' fill='#ffffff' font-size='10' font-weight='bold'"
        " dominant-baseline='middle'>Cx</text>\n"
        "  <text x='42' y='%d' fill='#ccccdd' font-size='12'>"
        "Executando na CPU x (cor propria da tarefa)</text>\n",
        ly, ly + 7, ly + 7
    );
    ly += 20;

    /* Pronta */
    fprintf(f,
        "  <rect x='12' y='%d' width='24' height='14' fill='url(#dots_ready)' rx='2'/>\n"
        "  <text x='42' y='%d' fill='#ccccdd' font-size='12'>"
        "Pronta (na fila de prontos, aguardando CPU)</text>\n",
        ly, ly + 7
    );
    ly += 20;

    /* Suspensa */
    fprintf(f,
        "  <rect x='12' y='%d' width='24' height='14' fill='#111111' rx='2'/>\n"
        "  <text x='42' y='%d' fill='#ccccdd' font-size='12'>"
        "Suspensa (E/S, mutex, etc.) ou Finalizada</text>\n",
        ly, ly + 7
    );
    ly += 20;

    /* Chegada */
    fprintf(f,
        "  <text x='16' y='%d' fill='#00ee55' font-size='14'>&#x2193;</text>\n"
        "  <text x='42' y='%d' fill='#ccccdd' font-size='12'>"
        "Chegada da tarefa ao sistema</text>\n",
        ly + 10, ly + 7
    );
    ly += 20;

    /* Fim */
    fprintf(f,
        "  <text x='16' y='%d' fill='#ffcc00' font-size='13'>&#x2713;</text>\n"
        "  <text x='42' y='%d' fill='#ccccdd' font-size='12'>"
        "Tarefa concluida</text>\n",
        ly + 10, ly + 7
    );
    ly += 20;

    /* Sorteio */
    fprintf(f,
        "  <text x='16' y='%d' fill='#ee44ff' font-size='10'>&#x2605;</text>\n"
        "  <text x='42' y='%d' fill='#ccccdd' font-size='12'>"
        "Desempate por sorteio (req 4.3)</text>\n",
        ly + 10, ly + 7
    );
    ly += 20;

    /* CPU desligada */
    fprintf(f,
        "  <rect x='12' y='%d' width='24' height='14' fill='url(#hatch_off)' rx='2'/>\n"
        "  <text x='42' y='%d' fill='#ccccdd' font-size='12'>"
        "CPU desligada (sem tarefa disponivel)</text>\n",
        ly, ly + 7
    );

    fprintf(f, "</svg>\n");
    fclose(f);

    printf("Grafico de Gantt SVG salvo em '%s'.\n", filename);
}