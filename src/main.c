#include "task.h"
#include "scheduler.h"
#include<stdio.h>

#define MAX 100

int main() {
    Task tarefas[MAX];

    int n= carregar_tarefas(tarefas, MAX);

    printf("Carregadas %d tarefas\n", n);

    return 0;
}