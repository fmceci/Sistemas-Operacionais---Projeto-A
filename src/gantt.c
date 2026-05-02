#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gantt.h"

/* -----------------------------------------------------------------------
 * Constantes de layout do SVG (todas em pixels)
 * ----------------------------------------------------------------------- */
#define SVG_TICK_W      40   /* Largura de cada tick no gráfico */
#define SVG_ROW_H       36   /* Altura de cada linha (tarefa) */
#define SVG_LABEL_W    110   /* Largura da coluna de labels à esquerda */
#define SVG_MARGIN_TOP  60   /* Margem superior (para título e eixo de tempo) */
#define SVG_LEGEND_H   120   /* Altura da área de legenda abaixo do gráfico */
#define SVG_GAP          4   /* Espaço entre células do Gantt */

/* -----------------------------------------------------------------------
 * Inicialização
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
 * Registro de histórico
 * ----------------------------------------------------------------------- */

/*
 * gantt_record - captura um snapshot do estado do sistema no tick atual.
 *
 * Armazena: qual tarefa está em cada CPU, estados e tempos restantes
 * de todas as tarefas, e flags de eventos (chegada, fim, sorteio).
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

    /* Registra estado de cada CPU */
    for (int c = 0; c < cpu_count && c < MAX_CPUS; c++) {
        e->cpu_task[c]   = cpus[c].task_id;
        e->cpu_active[c] = cpus[c].active;
    }

    /* Registra estado e tempo restante de cada tarefa */
    for (int i = 0; i < task_count && i < MAX_TASKS; i++) {
        e->task_state[i]     = tasks[i].state;
        e->task_remaining[i] = tasks[i].remaining_time;

        /* Detecta chegada: tarefa passou de NEW para READY neste tick */
        e->task_arrived[i]  = (tasks[i].state == READY &&
                               tasks[i].arrival_time == tick) ? 1 : 0;

        /* Detecta fim: tarefa terminou exatamente agora */
        e->task_finished[i] = (tasks[i].state == FINISHED &&
                               tasks[i].finish_time == tick) ? 1 : 0;
    }

    history->count++;
}

/* -----------------------------------------------------------------------
 * Impressão no terminal (modo passo-a-passo)
 * ----------------------------------------------------------------------- */

/*
 * gantt_print_tick - imprime o estado de um tick no terminal.
 * Útil como "debugger" do sistema (req 1.5.1).
 */
void gantt_print_tick(const GanttHistory *history, int tick,
                      Task tasks[], int task_count) {
    /* Localiza a entrada do tick solicitado */
    const GanttEntry *e = NULL;
    for (int i = 0; i < history->count; i++) {
        if (history->entries[i].tick == tick) {
            e = &history->entries[i];
            break;
        }
    }
    if (!e) {
        printf("Tempo %d: (sem registro)\n", tick);
        return;
    }

    printf("\n=== Tempo %d ===\n", tick);

    /* Estado das CPUs */
    for (int c = 0; c < history->cpu_count; c++) {
        if (e->cpu_active[c] && e->cpu_task[c] != -1) {
            printf("  CPU %d -> Tarefa %d", c, e->cpu_task[c]);
            if (e->lottery_tick) printf(" [SORTEIO]");
            printf("\n");
        } else {
            printf("  CPU %d -> desligada\n", c);
        }
    }

    /* Estado individual das tarefas */
    printf("  Tarefas:\n");
    for (int i = 0; i < task_count; i++) {
        const char *state_str;
        switch (e->task_state[i]) {
            case NEW:      state_str = "NEW";      break;
            case READY:    state_str = "READY";    break;
            case RUNNING:  state_str = "RUNNING";  break;
            case FINISHED: state_str = "FINISHED"; break;
            default:       state_str = "?";
        }
        printf("    Tarefa %d | Estado: %-8s | Restante: %d",
               tasks[i].id, state_str, e->task_remaining[i]);
        if (e->task_arrived[i])  printf(" [CHEGOU]");
        if (e->task_finished[i]) printf(" [TERMINOU]");
        printf("\n");
    }
}

/* -----------------------------------------------------------------------
 * Geração do SVG
 * ----------------------------------------------------------------------- */

/*
 * hex_to_rgb - converte string hexadecimal (ex: "FF0000") em componentes RGB.
 * Usado para aplicar cores das tarefas no SVG.
 */
static void hex_to_rgb(const char *hex, int *r, int *g, int *b) {
    unsigned int ur = 0, ug = 0, ub = 0;
    sscanf(hex, "%02x%02x%02x", &ur, &ug, &ub);
    *r = (int)ur; *g = (int)ug; *b = (int)ub;
}

/*
 * find_task_index - encontra o índice da tarefa com o ID dado no vetor de tarefas.
 * Retorna -1 se não encontrada.
 */
static int find_task_index(Task tasks[], int task_count, int task_id) {
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].id == task_id) return i;
    }
    return -1;
}

/*
 * write_svg_defs - escreve a seção <defs> do SVG com padrões e marcadores.
 * Define o padrão de hachurado para CPUs desligadas e ícones de eventos.
 */
static void write_svg_defs(FILE *f) {
    /* Padrão hachurado para CPU desligada */
    fprintf(f,
        "  <defs>\n"
        "    <pattern id='hatch_off' patternUnits='userSpaceOnUse' width='6' height='6'>\n"
        "      <rect width='6' height='6' fill='#333333'/>\n"
        "      <line x1='0' y1='6' x2='6' y2='0' stroke='#555555' stroke-width='1'/>\n"
        "    </pattern>\n"
        "    <!-- Seta indicando chegada da tarefa -->\n"
        "    <marker id='arrow_in' markerWidth='6' markerHeight='6'\n"
        "            refX='3' refY='3' orient='auto'>\n"
        "      <path d='M0,0 L6,3 L0,6 Z' fill='#00cc44'/>\n"
        "    </marker>\n"
        "  </defs>\n"
    );
}

/*
 * gantt_save_svg - gera o arquivo SVG do gráfico de Gantt.
 *
 * Layout do SVG (de cima para baixo):
 *   - Título
 *   - Eixo de tempo (ticks)
 *   - Uma linha por tarefa, com ID no eixo Y (ordem crescente de ID, req 2.5)
 *   - Cada célula colorida = tarefa executando
 *   - Célula cinza = tarefa pronta (na fila)
 *   - Célula preta = tarefa suspensa (NEW ou após fim)
 *   - Ícone de seta verde = chegada da tarefa (req 2.2)
 *   - Ícone de checkmark = fim da tarefa (req 2.2)
 *   - Ícone de dado = sorteio aconteceu (req 4.3)
 *   - Legenda ao final (req geral)
 *
 * Eixo Y: ID menor mais próximo do eixo X (parte de baixo), conforme req 2.5.
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

    int ticks      = history->count;
    int cpu_count  = history->cpu_count;

    /* Dimensões do SVG */
    int width  = SVG_LABEL_W + ticks * SVG_TICK_W + 40;
    int height = SVG_MARGIN_TOP + task_count * SVG_ROW_H + SVG_LEGEND_H + 20;

    /* Cabeçalho SVG */
    fprintf(f,
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<svg xmlns='http://www.w3.org/2000/svg' width='%d' height='%d' "
        "font-family='monospace' font-size='12'>\n",
        width, height
    );

    write_svg_defs(f);

    /* Fundo */
    fprintf(f, "  <rect width='%d' height='%d' fill='#1a1a2e'/>\n", width, height);

    /* Título */
    fprintf(f,
        "  <text x='%d' y='22' fill='#e0e0e0' font-size='16' font-weight='bold' "
        "text-anchor='middle'>Grafico de Gantt - Simulador SO</text>\n",
        width / 2
    );

    /* ---- Eixo de tempo ---- */
    fprintf(f, "  <!-- Eixo de tempo -->\n");
    for (int t = 0; t < ticks; t++) {
        int x = SVG_LABEL_W + t * SVG_TICK_W;
        fprintf(f,
            "  <text x='%d' y='45' fill='#aaaaaa' text-anchor='middle'>%d</text>\n",
            x + SVG_TICK_W / 2, history->entries[t].tick
        );
        /* Linha vertical de grade */
        fprintf(f,
            "  <line x1='%d' y1='50' x2='%d' y2='%d' "
            "stroke='#333355' stroke-width='0.5'/>\n",
            x, x, SVG_MARGIN_TOP + task_count * SVG_ROW_H
        );
    }

    /* ---- Linhas das tarefas ----
     * Requisito 2.5: ID menor fica mais próximo do eixo X (parte de baixo).
     * Logo, a tarefa com menor ID ocupa a última linha (maior y).
     * Ordenamos o desenho de cima para baixo do maior ID para o menor.
     */
    fprintf(f, "  <!-- Linhas das tarefas -->\n");

    /* Monta lista de IDs em ordem crescente */
    int sorted_ids[MAX_TASKS];
    for (int i = 0; i < task_count; i++) sorted_ids[i] = i; /* índices */
    /* Bubble sort simples por ID crescente */
    for (int i = 0; i < task_count - 1; i++) {
        for (int j = i + 1; j < task_count; j++) {
            if (tasks[sorted_ids[i]].id > tasks[sorted_ids[j]].id) {
                int tmp = sorted_ids[i];
                sorted_ids[i] = sorted_ids[j];
                sorted_ids[j] = tmp;
            }
        }
    }

    for (int row = 0; row < task_count; row++) {
        /*
         * row=0 → tarefa com maior ID (topo do gráfico)
         * row=task_count-1 → tarefa com menor ID (mais próxima do eixo X)
         */
        int display_row = task_count - 1 - row; /* inverte para req 2.5 */
        int tidx = sorted_ids[row];
        int y    = SVG_MARGIN_TOP + display_row * SVG_ROW_H;

        int r, g, b;
        hex_to_rgb(tasks[tidx].color, &r, &g, &b);

        /* Label da tarefa */
        fprintf(f,
            "  <text x='%d' y='%d' fill='#cccccc' text-anchor='end'>T%d (CPU)</text>\n",
            SVG_LABEL_W - 6, y + SVG_ROW_H / 2 + 4, tasks[tidx].id
        );

        /* Linha de fundo da tarefa */
        fprintf(f,
            "  <rect x='%d' y='%d' width='%d' height='%d' fill='#12122a' rx='2'/>\n",
            SVG_LABEL_W, y + SVG_GAP,
            ticks * SVG_TICK_W, SVG_ROW_H - SVG_GAP * 2
        );

        /* Células tick a tick */
        for (int t = 0; t < ticks; t++) {
            const GanttEntry *e = &history->entries[t];
            int x = SVG_LABEL_W + t * SVG_TICK_W;
            int cw = SVG_TICK_W - 1;
            int ch = SVG_ROW_H - SVG_GAP * 2 - 2;
            int cy = y + SVG_GAP + 1;

            /* Descobre em qual CPU esta tarefa estava (se estava) */
            int on_cpu = -1;
            for (int c = 0; c < cpu_count; c++) {
                if (e->cpu_task[c] == tasks[tidx].id && e->cpu_active[c]) {
                    on_cpu = c;
                    break;
                }
            }

            TaskState ts = e->task_state[tidx];

            if (on_cpu >= 0) {
                /* EXECUTANDO: cor da tarefa com label da CPU */
                fprintf(f,
                    "  <rect x='%d' y='%d' width='%d' height='%d' "
                    "fill='rgb(%d,%d,%d)' rx='2'/>\n",
                    x + 1, cy, cw, ch, r, g, b
                );
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#000000' font-size='10' "
                    "text-anchor='middle' font-weight='bold'>C%d</text>\n",
                    x + cw / 2, cy + ch / 2 + 4, on_cpu
                );
            } else if (ts == READY) {
                /* PRONTA (na fila): ausência de cor (cinza claro) - req 2.1 */
                fprintf(f,
                    "  <rect x='%d' y='%d' width='%d' height='%d' "
                    "fill='#2a2a4a' rx='2' stroke='#444466' stroke-width='0.5'/>\n",
                    x + 1, cy, cw, ch
                );
            } else if (ts == NEW) {
                /* Ainda não chegou: vazio */
                /* sem desenho */
                (void)ch;
            } else if (ts == FINISHED) {
                /* TERMINADA: célula preta - req 2.1 */
                fprintf(f,
                    "  <rect x='%d' y='%d' width='%d' height='%d' "
                    "fill='#111111' rx='2'/>\n",
                    x + 1, cy, cw, ch
                );
            }

            /* Evento: chegada da tarefa (seta verde para baixo) - req 2.2 */
            if (e->task_arrived[tidx]) {
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#00ee55' font-size='14' "
                    "text-anchor='middle'>&#x2193;</text>\n",
                    x + SVG_TICK_W / 2, cy - 2
                );
            }

            /* Evento: tarefa terminou (checkmark) - req 2.2 */
            if (e->task_finished[tidx]) {
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#ffdd00' font-size='13' "
                    "text-anchor='middle'>&#x2713;</text>\n",
                    x + SVG_TICK_W / 2, cy + ch - 2
                );
            }

            /* Evento: sorteio aconteceu - req 4.3 */
            if (e->lottery_tick && on_cpu >= 0) {
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#ff44ff' font-size='10' "
                    "text-anchor='middle'>&#x2684;</text>\n",
                    x + SVG_TICK_W - 6, cy + 10
                );
            }
        }
    }

    /* ---- Informação de CPUs desligadas ---- */
    fprintf(f, "  <!-- Linha de status das CPUs -->\n");
    for (int t = 0; t < ticks; t++) {
        const GanttEntry *e = &history->entries[t];
        int x = SVG_LABEL_W + t * SVG_TICK_W;
        int y = SVG_MARGIN_TOP + task_count * SVG_ROW_H;

        for (int c = 0; c < cpu_count; c++) {
            if (!e->cpu_active[c]) {
                /* Marca CPU desligada no eixo inferior */
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#ff4444' font-size='9' "
                    "text-anchor='middle'>C%d&#x25BC;</text>\n",
                    x + SVG_TICK_W / 2, y + 14 + c * 12, c
                );
            }
        }
    }

    /* ---- Legenda ---- */
    int ly = SVG_MARGIN_TOP + task_count * SVG_ROW_H + 30 + cpu_count * 12;
    fprintf(f, "  <!-- Legenda -->\n");
    fprintf(f,
        "  <text x='10' y='%d' fill='#ffffff' font-size='13' font-weight='bold'>"
        "Legenda:</text>\n", ly
    );
    ly += 18;

    /* Executando */
    fprintf(f,
        "  <rect x='10' y='%d' width='22' height='14' fill='#3399ff' rx='2'/>\n"
        "  <text x='38' y='%d' fill='#cccccc'>Executando (cor da tarefa)</text>\n",
        ly, ly + 11
    );
    ly += 18;

    /* Pronta */
    fprintf(f,
        "  <rect x='10' y='%d' width='22' height='14' fill='#2a2a4a' "
        "rx='2' stroke='#444466' stroke-width='1'/>\n"
        "  <text x='38' y='%d' fill='#cccccc'>Pronta (na fila de prontos)</text>\n",
        ly, ly + 11
    );
    ly += 18;

    /* Finalizada */
    fprintf(f,
        "  <rect x='10' y='%d' width='22' height='14' fill='#111111' rx='2'/>\n"
        "  <text x='38' y='%d' fill='#cccccc'>Finalizada</text>\n",
        ly, ly + 11
    );
    ly += 18;

    /* Chegada */
    fprintf(f,
        "  <text x='14' y='%d' fill='#00ee55' font-size='14'>&#x2193;</text>\n"
        "  <text x='38' y='%d' fill='#cccccc'>Chegada da tarefa</text>\n",
        ly + 11, ly + 11
    );
    ly += 18;

    /* Fim */
    fprintf(f,
        "  <text x='14' y='%d' fill='#ffdd00' font-size='13'>&#x2713;</text>\n"
        "  <text x='38' y='%d' fill='#cccccc'>Tarefa concluida</text>\n",
        ly + 11, ly + 11
    );
    ly += 18;

    /* Sorteio */
    fprintf(f,
        "  <text x='14' y='%d' fill='#ff44ff' font-size='10'>&#x2684;</text>\n"
        "  <text x='38' y='%d' fill='#cccccc'>Desempate por sorteio</text>\n",
        ly + 11, ly + 11
    );
    ly += 18;

    /* CPU desligada */
    fprintf(f,
        "  <text x='14' y='%d' fill='#ff4444' font-size='9'>C0&#x25BC;</text>\n"
        "  <text x='38' y='%d' fill='#cccccc'>CPU desligada (sem tarefa pronta)</text>\n",
        ly + 11, ly + 11
    );

    fprintf(f, "</svg>\n");
    fclose(f);

    printf("Grafico de Gantt salvo em '%s'.\n", filename);
}
