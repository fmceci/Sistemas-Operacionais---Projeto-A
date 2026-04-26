#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include"config.h"


int load_config(const char *filename, Config *config, Task tasks[], int *task_count) {
    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        printf("Erro ao abrir o arquivo.\n");
        return 0;
    }

    char line[MAX_LINE];

    if (fgets(line, sizeof(line), file) == NULL) {
        printf("Arquivo vazio.\n");
        fclose(file);
        return 0;
    }

    line[strcspn(line, "\n")] = '\0';

    char *token = strtok(line, ";");

    if (token != NULL) {
        strcpy(config->algorithm, token);
    }

    token = strtok(NULL, ";");
    if (token != NULL) {
        config->quantum = atoi(token);
    }

    token = strtok(NULL, ";");
    if (token != NULL) {
        config->cpu_count = atoi(token);
    }

    *task_count = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\n")] = '\0';

        if (strlen(line) == 0) {
            continue;
        }

        token = strtok(line, ";");

        if (token != NULL) {
            tasks[*task_count].id = atoi(token);
        }

        token = strtok(NULL, ";");
        if (token != NULL) {
            strcpy(tasks[*task_count].color, token);
        }

        token = strtok(NULL, ";");
        if (token != NULL) {
            tasks[*task_count].arrival_time = atoi(token);
        }

        token = strtok(NULL, ";");
        if (token != NULL) {
            tasks[*task_count].duration = atoi(token);
            tasks[*task_count].remaining_time = tasks[*task_count].duration;
        }

        token = strtok(NULL, ";");
        if (token != NULL) {
            tasks[*task_count].priority = atoi(token);
        }

        tasks[*task_count].state = NEW;
        tasks[*task_count].cpu_id = -1;

        (*task_count)++;
    }

    fclose(file);
    return 1;
}