#ifndef TASK_H
#define TASK_H

/* Número máximo de tarefas suportadas na simulação */
#define MAX_TASKS 64

/* Tamanho máximo da string de eventos de uma tarefa */
#define MAX_EVENTS_STR 256

/*
 * TaskState - estados possíveis de uma tarefa no sistema operacional.
 *
 * NEW      : Tarefa criada, aguardando o instante de ingresso.
 * READY    : Tarefa pronta para executar, aguardando CPU.
 * RUNNING  : Tarefa em execução em alguma CPU.
 * SUSPENDED: Tarefa suspensa (por E/S, mutex, etc.) - Projeto B.
 * FINISHED : Tarefa finalizou sua execução.
 */
typedef enum {
    NEW = 0,
    READY,
    RUNNING,
    SUSPENDED,
    FINISHED
} TaskState;

/*
 * Task - Task Control Block (TCB).
 *
 * Armazena todas as informações de uma tarefa antes, durante e
 * após a simulação (requisito 1.3 do enunciado).
 */
typedef struct {
    int        id;                      /* Identificador único da tarefa */
    char       color[8];                /* Cor no formato RRGGBB hex, ex: "FF0000" */
    int        arrival_time;            /* Instante de ingresso da tarefa */
    int        duration;                /* Duração total (tempo de execução) */
    int        remaining_time;          /* Tempo restante para terminar */
    int        priority;                /* Prioridade estática (maior = mais prioritário) */
    char       events[MAX_EVENTS_STR];  /* Lista de eventos (Projeto B) */

    TaskState  state;                   /* Estado atual da tarefa */
    int        cpu_id;                  /* ID da CPU onde está executando (-1 = nenhuma) */
    int        start_time;              /* Tick em que começou a executar pela 1ª vez */
    int        finish_time;             /* Tick em que terminou */
    int        ticks_this_slice;        /* Ticks executados no slice atual (para quantum) */
} Task;

#endif /* TASK_H */