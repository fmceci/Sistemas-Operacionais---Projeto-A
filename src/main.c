#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "task.h"       // Definição da struct Task
#include "config.h"     // Definição da struct Config e leitura de config
#include "simulation.h" // Funções relacionadas à simulação

/*
 * main - ponto de entrada do simulador.
 *
 * Uso: ./simulador [arquivo_config] [modo]
 *   arquivo_config: caminho do arquivo de entrada (padrão: entrada.txt)
 *   modo: "completo" ou "passo" (padrão: exibe menu)
 */
int main(int argc, char *argv[]) {

    /* Inicializa o gerador de números aleatórios
       time(NULL) gera uma semente baseada no tempo atual,
       garantindo resultados diferentes a cada execução */
    srand((unsigned int)time(NULL));

    /* Define arquivo padrão de configuração */
    const char *config_file = "entrada.txt";

    /* Se o usuário passar um arquivo como argumento, usa ele */
    if (argc >= 2) config_file = argv[1];

    /* Declara estruturas principais */
    Config config;                 // Armazena configurações do sistema
    Task   tasks[MAX_TASKS];       // Vetor de tarefas
    int    task_count = 0;         // Quantidade de tarefas carregadas


    /* Carrega configuração e tarefas do arquivo */
    if (!load_config(config_file, &config, tasks, &task_count)) {
        /* Se falhar, exibe erro e encerra o programa */
        fprintf(stderr, "Falha ao carregar configuracao. Encerrando.\n");
        return 1;
    }
    int alg_opcao;

    printf("Selecione o algoritmo de escalonamento:\n");
    printf("  1 - SRTF\n");
    printf("  2 - PRIOP\n");
    printf("Opcao: ");

    if (scanf("%d", &alg_opcao) != 1) {
        alg_opcao = 1;
    }

    switch (alg_opcao) {
        case 1:
            strcpy(config.algorithm, "SRTF");
        break;
        case 2:
            strcpy(config.algorithm, "PRIOP");
        break;
        default:
            strcpy(config.algorithm, "SRTF");
        break;
    }

    printf("\nAlgoritmo selecionado: %s\n\n", config.algorithm);



    /* Exibe resumo da configuração carregada */
    printf("=== Simulador de SO Multitarefa ===\n\n");

    printf("Configuracao:\n");
    printf("  Algoritmo : %s\n", config.algorithm);  // Algoritmo de escalonamento
    printf("  Quantum   : %d\n", config.quantum);    // Tempo de quantum (Round Robin)
    printf("  CPUs      : %d\n\n", config.cpu_count);// Número de CPUs

    /* Exibe todas as tarefas carregadas */
    printf("Tarefas carregadas (%d):\n", task_count);

    for (int i = 0; i < task_count; i++) {
        printf("  ID: %d | Cor: #%s | Ingresso: %d | Duracao: %d | Prioridade: %d\n | Lista de Eventos: %s\n",
               tasks[i].id,             // Identificador da tarefa
               tasks[i].color,          // Cor (para visualização)
               tasks[i].arrival_time,   // Tempo de chegada
               tasks[i].duration,       // Tempo total de execução
               tasks[i].priority,       // Prioridade da tarefa
               tasks[i].events);        // Eventos associados (ex: I/O)
    }

    printf("\n");

    /* Inicializa o estado da simulação */
    SimulationState sim;

    /* Passa configuração e tarefas para o simulador */
    simulation_init(&sim, &config, tasks, task_count);

    /* Define o modo de execução:
       0 = menu
       1 = completo
       2 = passo-a-passo */
    int mode = 0;

    /* Verifica se o usuário passou o modo via argumento */
    if (argc >= 3) {
        if (strcmp(argv[2], "completo") == 0)
            mode = 1;
        else if (strcmp(argv[2], "passo") == 0)
            mode = 2;
    }

    /* Caso não tenha sido definido, exibe menu */
    if (mode == 0) {
        printf("Selecione o modo de execucao:\n");
        printf("  1 - Completo (sem interacao)\n");
        printf("  2 - Passo-a-passo (interativo)\n");
        printf("Opcao: ");

        /* Lê a opção do usuário */
        if (scanf("%d", &mode) != 1)
            mode = 1; // fallback para modo completo

        printf("\n");
    }

    /* Executa a simulação conforme o modo escolhido */
    if (mode == 2) {
        simulation_run_step_by_step(&sim); // Executa passo a passo
    } else {
        simulation_run_complete(&sim);     // Executa direto até o final
    }

    return 0;
}