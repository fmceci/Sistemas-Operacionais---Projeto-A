# Makefile do Simulador de SO Multitarefa
# Compila sem dependências externas (req 5 do enunciado)

CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -std=c99 -g
TARGET  = simulador
SRCS    = main.c config.c scheduler.c simulation.c gantt.c
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Executa no modo completo com o arquivo de entrada padrão
run: $(TARGET)
	./$(TARGET) entrada.txt completo

# Executa no modo passo-a-passo
step: $(TARGET)
	./$(TARGET) entrada.txt passo

clean:
	rm -f $(OBJS) $(TARGET) gantt.svg

.PHONY: all run step clean

