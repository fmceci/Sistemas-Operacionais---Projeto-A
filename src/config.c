#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"

/*
 * str_to_upper - converte uma string para maiúsculas in-place.
 * Necessário para atender ao requisito 3.3.2: tratar "SRTF", "srtf" e "SrTf"
 * como equivalentes.
 */
static void str_to_upper(char *s) {
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

/*
 * load_config - abre e interpreta o arquivo de configuração.
 *
 * Formato esperado:
 *   Linha 1: algoritmo_escalonamento;quantum;qtde_cpus
 *   Linha 2+: id;cor;ingresso;duracao;prioridade
 *
 * Parâmetros:
 *   filename   - caminho do arquivo de configuração
 *   config     - ponteiro para a estrutura Config a preencher
 *   tasks      - vetor de Task a preencher
 *   task_count - ponteiro para o contador de tarefas lidas
 *
 * Retorna 1 em sucesso, 0 em falha.
 */
int load_config(const char *filename, Config *config, Task tasks[], int *task_count) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo '%s'.\n", filename);
        return 0;
    }

    char line[MAX_LINE];

    /* --- Lê a primeira linha: parâmetros gerais do sistema --- */
    if (fgets(line, sizeof(line), file) == NULL) {
        fprintf(stderr, "Erro: arquivo '%s' esta vazio.\n", filename);
        fclose(file);
        return 0;
    }
    line[strcspn(line, "\r\n")] = '\0'; /* Remove newline/carriage return */

    char *token = strtok(line, ";");
    if (token != NULL) {
        strncpy(config->algorithm, token, MAX_ALGO - 1);
        config->algorithm[MAX_ALGO - 1] = '\0';
        str_to_upper(config->algorithm); /* Normaliza para maiúsculas (req 3.3.2) */
    }

    token = strtok(NULL, ";");
    config->quantum = (token != NULL) ? atoi(token) : 1;

    token = strtok(NULL, ";");
    config->cpu_count = (token != NULL) ? atoi(token) : 2;

    /* Garante pelo menos 2 CPUs conforme enunciado */
    if (config->cpu_count < 2) config->cpu_count = 2;

    /* --- Lê as linhas seguintes: uma tarefa por linha --- */
    *task_count = 0;
    while (fgets(line, sizeof(line), file) != NULL && *task_count < MAX_TASKS) {
        line[strcspn(line, "\r\n")] = '\0';

        /* Ignora linhas vazias */
        if (strlen(line) == 0) continue;

        Task *t = &tasks[*task_count];

        token = strtok(line, ";");
        if (token == NULL) continue;
        t->id = atoi(token);

        token = strtok(NULL, ";");
        if (token != NULL) {
            strncpy(t->color, token, 7);
            t->color[7] = '\0';
        } else {
            strcpy(t->color, "FFFFFF");
        }

        token = strtok(NULL, ";");
        t->arrival_time = (token != NULL) ? atoi(token) : 0;

        token = strtok(NULL, ";");
        t->duration = (token != NULL) ? atoi(token) : 1;
        t->remaining_time = t->duration; /* Inicializa tempo restante */

        token = strtok(NULL, ";");
        t->priority = (token != NULL) ? atoi(token) : 0;

        /* Estado inicial e CPU */
        t->state      = NEW;
        t->cpu_id     = -1;
        t->start_time = -1;
        t->finish_time = -1;

        (*task_count)++;
    }

    fclose(file);
    return 1;
}