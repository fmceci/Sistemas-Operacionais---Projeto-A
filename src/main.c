#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "task.h"
#include "config.h"
#include "simulation.h"
#include "scheduler.h"
#include "gantt.h"

/*
 * main - ponto de entrada do simulador de SO multitarefa.
 *
 * Uso:
 *   ./simulador [arquivo_config] [modo]
 *
 *   arquivo_config : caminho do arquivo de configuração (padrão: entrada.txt)
 *   modo           : "completo" ou "passo" (se omitido, exibe menu interativo)
 *
 * Fluxo:
 *   1. Inicializa gerador de números aleatórios (para sorteios de desempate)
 *   2. Carrega configurações e tarefas do arquivo
 *   3. Permite ao usuário revisar/ajustar parâmetros antes da simulação
 *   4. Executa a simulação no modo escolhido
 */
int main(int argc, char *argv[]) {

    #ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    #endif
    /* Inicializa o gerador aleatório com o tempo atual, garantindo
     * resultados diferentes a cada execução (necessário para sorteios). */
    srand((unsigned int)time(NULL));

    /* Arquivo de configuração padrão */
    const char *config_file = "entrada.txt";
    if (argc >= 2) config_file = argv[1];

    /* Estruturas principais */
    Config config;
    Task   tasks[MAX_TASKS];
    int    task_count = 0;

    /* Carrega o arquivo de configuração (req 3.3) */
    if (!load_config(config_file, &config, tasks, &task_count)) {
        fprintf(stderr, "Falha ao carregar configuracao. Encerrando.\n");
        return 1;
    }

    /* ---- Exibe e permite ajustar configurações (req 3.2) ---- */
    printf("\n========================================\n");
    printf("  Simulador de SO Multitarefa - v0.5\n");
    printf("========================================\n\n");

    printf("Configuracoes carregadas de '%s':\n", config_file);
    printf("  Algoritmo : %s\n", config.algorithm);
    printf("  Quantum   : %d\n", config.quantum);
    printf("  CPUs      : %d\n\n", config.cpu_count);

    /* Permite trocar o algoritmo de escalonamento */
    printf("Selecione o algoritmo (ENTER para manter %s):\n", config.algorithm);
    printf("  1 - SRTF  (Shortest Remaining Time First)\n");
    printf("  2 - PRIOP (Prioridade Preemptivo)\n");
    printf("Opcao: ");

    char buffer[32];
    if (fgets(buffer, sizeof(buffer), stdin) != NULL && buffer[0] != '\n') {
        int opcao = atoi(buffer);
        switch (opcao) {
            case 1: strcpy(config.algorithm, "SRTF");  break;
            case 2: strcpy(config.algorithm, "PRIOP"); break;
            default:
                printf("Opcao invalida. Mantendo: %s\n", config.algorithm);
        }
    }

    /* Permite ajustar o quantum */
    config.quantum = ler_inteiro_com_padrao(
        "Quantum (ENTER para manter %d)", config.quantum);
    if (config.quantum < 1) config.quantum = 1;

    /* Permite ajustar o número de CPUs */
    config.cpu_count = ler_inteiro_com_padrao(
        "Numero de CPUs (ENTER para manter %d)", config.cpu_count);
    if (config.cpu_count < 2) {
        printf("Minimo de 2 CPUs. Ajustando para 2.\n");
        config.cpu_count = 2;
    }
    if (config.cpu_count > MAX_CPUS) {
        printf("Maximo de %d CPUs. Ajustando.\n", MAX_CPUS);
        config.cpu_count = MAX_CPUS;
    }

    printf("\n--- Configuracoes finais ---\n");
    printf("  Algoritmo : %s\n", config.algorithm);
    printf("  Quantum   : %d\n", config.quantum);
    printf("  CPUs      : %d\n\n", config.cpu_count);

    /* Exibe tarefas carregadas */
    printf("Tarefas carregadas (%d):\n", task_count);
    for (int i = 0; i < task_count; i++) {
        printf("  T%-3d | Cor: #%s | Ingresso: %2d | Duracao: %2d | Prioridade: %2d | Eventos: %s\n",
               tasks[i].id,
               tasks[i].color,
               tasks[i].arrival_time,
               tasks[i].duration,
               tasks[i].priority,
               tasks[i].events);
    }
    printf("\n");

    /* ---- Inicializa a simulação ---- */
    SimulationState sim;
    simulation_init(&sim, &config, tasks, task_count);

    /* ---- Seleciona o modo de execução ---- */
    int mode = 0;

    /* Verifica se o modo foi passado como argumento */
    if (argc >= 3) {
        if (strcmp(argv[2], "completo") == 0) mode = 1;
        else if (strcmp(argv[2], "passo") == 0) mode = 2;
    }

    /* Se não foi definido, exibe menu */
    if (mode == 0) {
        printf("Selecione o modo de execucao:\n");
        printf("  1 - Completo   (executa tudo de uma vez)\n");
        printf("  2 - Passo-a-passo (interativo, com Gantt ao vivo)\n");
        printf("Opcao: ");

        if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
            mode = atoi(buffer);
        }
        if (mode != 1 && mode != 2) {
            printf("Opcao invalida. Usando modo completo.\n");
            mode = 1;
        }
        printf("\n");
    }

    /* ---- Executa a simulação ---- */
    if (mode == 2) {
        simulation_run_step_by_step(&sim);
    } else {
        simulation_run_complete(&sim);
    }

    return 0;
}