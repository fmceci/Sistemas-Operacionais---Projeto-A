#include <stdio.h>
#include "task.h"

int carregar_tarefas(Task tarefas[], int max) {
    FILE *f = fopen("config/entrada.txt", "r");
    if (!f) {
        printf("Erro ao abrir arquivo\n");
        return 0;
    }

    char linha[256];

    // pula primeira linha
    fgets(linha, sizeof(linha), f);

    int count = 0;

    while (fgets(linha, sizeof(linha), f) && count < max) {
        sscanf(linha, "%d;%*[^;];%d;%d;%d",
            &tarefas[count].id,
            &tarefas[count].ingresso,
            &tarefas[count].duracao,
            &tarefas[count].prioridade);

        tarefas[count].restante = tarefas[count].duracao;
        count++;
    }

    fclose(f);
    return count;
}