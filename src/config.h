#ifndef CONFIG_H
#define CONFIG_H

#include "task.h"

/* Tamanho máximo de uma linha do arquivo de configuração */
#define MAX_LINE 512

/* Tamanho máximo do nome do algoritmo de escalonamento */
#define MAX_ALGO 32

/* Número máximo de CPUs suportadas */
#define MAX_CPUS 16

/*
 * Config - parâmetros gerais da simulação, lidos da primeira linha
 * do arquivo de configuração (ex: "SRTF;5;2").
 */
typedef struct {
    char algorithm[MAX_ALGO]; /* Nome do algoritmo: "SRTF" ou "PRIOP" */
    int  quantum;             /* Quantum de tempo máximo por execução */
    int  cpu_count;           /* Número de CPUs disponíveis */
} Config;

/*
 * load_config - lê o arquivo de configuração e preenche Config e vetor de Task.
 * Retorna 1 em sucesso, 0 em erro.
 */
int load_config(const char *filename, Config *config, Task tasks[], int *task_count);

#endif /* CONFIG_H */