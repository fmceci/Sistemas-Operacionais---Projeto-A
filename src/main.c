#include <stdio.h>
#include "task.h"
#include"config.h"

int main() {
    Config config;
    Task tasks[MAX_TASKS];
    int task_count = 0;

    if (!load_config("src/entrada.txt", &config, tasks, &task_count)) {
        return 1;
    }

    printf("Configuração carregada:\n");
    printf("Algoritmo: %s\n", config.algorithm);
    printf("Quantum: %d\n", config.quantum);
    printf("CPUs: %d\n\n", config.cpu_count);

    printf("Tarefas carregadas:\n");

    for (int i = 0; i < task_count; i++) {
        printf(
            "ID: %d | Cor: %s | Ingresso: %d | Duração: %d | Prioridade: %d\n",
            tasks[i].id,
            tasks[i].color,
            tasks[i].arrival_time,
            tasks[i].duration,
            tasks[i].priority
        );
    }

    return 0;
}