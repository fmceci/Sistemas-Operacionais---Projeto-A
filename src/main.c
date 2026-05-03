#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "task.h"
#include "config.h"
#include "simulation.h"

/*
 * main - ponto de entrada do simulador.
 *
 * Uso: ./simulador [arquivo_config] [modo]
 *   arquivo_config: caminho do arquivo de entrada (padrão: entrada.txt)
 *   modo: "completo" ou "passo" (padrão: exibe menu)
 */
int main(int argc, char *argv[]) {
    /* Inicializa o gerador de números aleatórios (usado em sorteios - req 4.3) */
    srand((unsigned int)time(NULL));

    const char *config_file = "entrada.txt";
    if (argc >= 2) config_file = argv[1];

    Config config;
    Task   tasks[MAX_TASKS];
    int    task_count = 0;

    /* Carrega configuração e tarefas do arquivo */
    if (!load_config(config_file, &config, tasks, &task_count)) {
        fprintf(stderr, "Falha ao carregar configuracao. Encerrando.\n");
        return 1;
    }

    /* Exibe resumo da configuração */
    printf("=== Simulador de SO Multitarefa ===\n\n");
    printf("Configuracao:\n");
    printf("  Algoritmo : %s\n", config.algorithm);
    printf("  Quantum   : %d\n", config.quantum);
    printf("  CPUs      : %d\n\n", config.cpu_count);

    printf("Tarefas carregadas (%d):\n", task_count);
    for (int i = 0; i < task_count; i++) {
        printf("  ID: %d | Cor: #%s | Ingresso: %d | Duracao: %d | Prioridade: %d\n | Lista de Eventos: %s\n",
               tasks[i].id,
               tasks[i].color,
               tasks[i].arrival_time,
               tasks[i].duration,
               tasks[i].priority,
               tasks[i].events);
    }
    printf("\n");

    /* Inicializa simulação */
    SimulationState sim;
    simulation_init(&sim, &config, tasks, task_count);

    /* Determina modo de execução */
    int mode = 0; /* 0 = menu, 1 = completo, 2 = passo-a-passo */
    if (argc >= 3) {
        if (strcmp(argv[2], "completo") == 0) mode = 1;
        else if (strcmp(argv[2], "passo") == 0) mode = 2;
    }

    if (mode == 0) {
        printf("Selecione o modo de execucao:\n");
        printf("  1 - Completo (sem interacao)\n");
        printf("  2 - Passo-a-passo (interativo)\n");
        printf("Opcao: ");
        if (scanf("%d", &mode) != 1) mode = 1;
        printf("\n");
    }

    if (mode == 2) {
        simulation_run_step_by_step(&sim);
    } else {
        simulation_run_complete(&sim);
    }

    return 0;
}