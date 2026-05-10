#ifndef TASK_H
#define TASK_H

/* Número máximo de tarefas suportadas pelo simulador */
#define MAX_TASKS 64
/*Número máximo de eventos suportados pelo sumulador*/
#define MAX_EVENTS_STR 256

/*
 * TaskState - representa os estados possíveis de uma tarefa no sistema.
 * Segue o ciclo de vida clássico de um processo em SO.
 */
typedef enum {
    NEW,        /* Tarefa ainda não chegou ao sistema */
    READY,      /* Tarefa pronta para executar, aguardando CPU */
    RUNNING,    /* Tarefa atualmente em execução em alguma CPU */
    FINISHED    /* Tarefa concluiu sua execução */
} TaskState;

/*
 * Task - Task Control Block (TCB).
 * Armazena todas as informações de uma tarefa durante todo o ciclo de vida,
 * conforme requisito 1.3 do enunciado.
 */
typedef struct {
    int id;             /* Identificador único da tarefa */
    char color[8];      /* Cor RGB hexadecimal (ex: "FF0000"), 6 chars + '\0' */

    int arrival_time;   /* Instante em que a tarefa chega ao sistema (ingresso) */
    int duration;       /* Duração total de execução da tarefa */
    int remaining_time; /* Tempo restante de execução (usado pelo SRTF) */
    int priority;       /* Prioridade estática da tarefa (usado pelo PRIOP) */
    char events[MAX_EVENTS_STR]; /* Lista de eventos lida do arquivo.
                                 * No Projeto A, é apenas armazenada.
                                 * Ex: "IO:3-2,ML01:5,MU01:7" ou "-"
                                 */

    TaskState state;    /* Estado atual da tarefa */
    int cpu_id;         /* ID da CPU em que está executando (-1 = nenhuma) */

    int start_time;     /* Instante em que começou a executar pela primeira vez */
    int finish_time;    /* Instante em que terminou a execução */
} Task;

#endif /* TASK_H */