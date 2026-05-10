# Makefile do Simulador de SO Multitarefa
# Responsável por automatizar a compilação do projeto

# Compilador utilizado
CC = gcc

# Flags de compilação:
# -Wall -Wextra → mostram avisos importantes
# -pedantic     → força seguir padrão da linguagem
# -std=c99      → usa padrão C99
# -g            → inclui informações para debug
CFLAGS = -Wall -Wextra -pedantic -std=c99 -g

# Nome do executável final
TARGET = simulador

# Lista de arquivos fonte (.c)
SRCS = main.c config.c scheduler.c simulation.c gantt.c

# Gera automaticamente a lista de objetos (.o) a partir dos .c
# Ex: main.c → main.o
OBJS = $(SRCS:.c=.o)

# Regra padrão (executada ao rodar apenas "make")
all: $(TARGET)

# Regra para gerar o executável final
# $@ → nome do alvo (simulador)
# $^ → todos os arquivos necessários (objetos .o)
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Regra genérica para compilar qualquer arquivo .c em .o
# $< → primeiro arquivo dependente (arquivo .c)
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Executa o programa no modo completo
# Usa o arquivo padrão "entrada.txt"
run: $(TARGET)
	./$(TARGET) entrada.txt completo

# Executa o programa no modo passo-a-passo (interativo)
step: $(TARGET)
	./$(TARGET) entrada.txt passo

# Remove arquivos gerados na compilação
# - arquivos objeto (.o)
# - executável
# - arquivo de saída do Gantt (svg)
clean:
	rm -f $(OBJS) $(TARGET) gantt.svg

# Indica que esses nomes não são arquivos reais,
# evitando conflitos caso existam arquivos com esses nomes
.PHONY: all run step clean