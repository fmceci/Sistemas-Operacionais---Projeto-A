#ifndef TASK_H
#define TASK_H

typedef enum {
    NEW,
    READY,
    RUNNING,
    FINISHED
} TaskState;

typedef struct {
    int id;
    char color[7];

    int arrival_time;
    int duration;
    int remaining_time;
    int priority;

    TaskState state;
    int cpu_id;
} Task;

#endif