#ifndef GANTT_H
#define GANTT_H

#include "task.h"
#include "scheduler.h"
#include "config.h"

/* Número máximo de ticks que o histórico do Gantt pode armazenar */
#define MAX_TICKS 1024

/*
 * GanttEntry - snapshot do estado completo do sistema em um único tick.
 *
 * Armazena: qual tarefa estava em cada CPU, estado e tempo restante de cada
 * tarefa, e flags de eventos especiais (chegada, fim, sorteio).
 * Usado para avançar/retroceder a simulação (req 1.5.2) e para gerar o SVG.
 */
typedef struct {
    int tick;                        /* Instante de tempo deste snapshot */
    int cpu_task[MAX_CPUS];          /* cpu_task[i]   = ID da tarefa na CPU i (-1 = desligada) */
    int cpu_active[MAX_CPUS];        /* cpu_active[i] = 1 se CPU i estava ligada neste tick */
    TaskState task_state[MAX_TASKS]; /* Estado de cada tarefa neste tick */
    int task_remaining[MAX_TASKS];   /* Tempo restante de cada tarefa neste tick */
    int lottery_tick;                /* 1 se houve desempate por sorteio neste tick */
    int task_arrived[MAX_TASKS];     /* 1 se a tarefa chegou (NEW→READY) neste tick */
    int task_finished[MAX_TASKS];    /* 1 se a tarefa terminou exatamente neste tick */
} GanttEntry;

/*
 * GanttHistory - histórico completo da simulação, tick a tick.
 *
 * Permite navegar livremente (avançar/retroceder) pela simulação (req 1.5.2).
 */
typedef struct {
    GanttEntry entries[MAX_TICKS]; /* Um snapshot por tick */
    int count;                     /* Número de entradas armazenadas */
    int cpu_count;                 /* Número de CPUs (para exibição) */
    int task_count;                /* Número de tarefas */
} GanttHistory;

/*
 * gantt_init - inicializa o histórico do Gantt com contadores zerados.
 */
void gantt_init(GanttHistory *history, int cpu_count, int task_count);

/*
 * gantt_record - registra o estado do sistema no tick atual.
 *
 * Deve ser chamada uma vez por tick, após todas as atualizações de estado.
 * Detecta automaticamente chegadas e fins de tarefas para os ícones do gráfico.
 */
void gantt_record(GanttHistory *history, int tick,
                  CPU cpus[], int cpu_count,
                  Task tasks[], int task_count,
                  int lottery_used);

/*
 * gantt_print_terminal - exibe o gráfico de Gantt completo no terminal.
 *
 * Usa blocos Unicode coloridos via ANSI escape codes (requisitos 2.1–2.3).
 * Exibe o histórico completo até o tick atual.
 *
 * Legenda visual:
 *   [Cx] bloco colorido  = Tarefa executando na CPU x
 *   ░░░░ (cinza)         = Tarefa pronta (na fila de prontos)
 *   ████ (preto)         = Tarefa suspensa (por qualquer motivo)
 *   (vazio)              = Tarefa ainda não chegou ao sistema
 *   ↓                    = Ícone de chegada da tarefa (req 2.2)
 *   ✓                    = Ícone de fim da tarefa (req 2.2)
 *   ★                    = Desempate por sorteio (req 4.3)
 *   ---- (vermelho)      = CPU desligada (req 1.2)
 */
void gantt_print_terminal(const GanttHistory *history, Task tasks[], int task_count);

/*
 * gantt_save_svg - gera o arquivo SVG do gráfico de Gantt ao final da simulação.
 *
 * Requisito 2.4: ao final, gera imagem (JPG, PNG, SVG, etc.) com o Gantt final.
 * O SVG espelha o mesmo layout do gráfico terminal, com maior resolução visual.
 *
 * Parâmetros:
 *   history    - histórico completo da simulação
 *   tasks      - vetor de tarefas (para cores e IDs)
 *   task_count - número de tarefas
 *   filename   - caminho do arquivo SVG de saída
 */
void gantt_save_svg(const GanttHistory *history, Task tasks[],
                    int task_count, const char *filename);

#endif /* GANTT_H */