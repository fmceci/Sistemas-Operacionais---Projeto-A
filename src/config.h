#ifndef CONFIG_H
#define CONFIG_H

#include "task.h"

#define MAX_TASKS 100
#define MAX_LINE 256

typedef struct {
    char algorithm[20];
    int quantum;
    int cpu_count;
} Config;

int load_config(const char *filename, Config *config, Task tasks[], int *task_count);

#endif