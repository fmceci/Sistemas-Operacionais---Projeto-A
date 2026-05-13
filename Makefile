CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -Iinclude
TARGET = simulador.exe

SRC = src/main.c src/config.c src/scheduler.c src/simulation.c src/gantt.c
OBJ = build/main.o build/config.o build/scheduler.o build/simulation.o build/gantt.o

all: build $(TARGET)

build:
	mkdir -p build

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)

build/main.o: src/main.c
	$(CC) $(CFLAGS) -c src/main.c -o build/main.o

build/config.o: src/config.c
	$(CC) $(CFLAGS) -c src/config.c -o build/config.o

build/scheduler.o: src/scheduler.c
	$(CC) $(CFLAGS) -c src/scheduler.c -o build/scheduler.o

build/simulation.o: src/simulation.c
	$(CC) $(CFLAGS) -c src/simulation.c -o build/simulation.o

build/gantt.o: src/gantt.c
	$(CC) $(CFLAGS) -c src/gantt.c -o build/gantt.o

run: all
	./$(TARGET)

step: all
	./$(TARGET) entrada.txt passo

clean:
	rm -rf build $(TARGET) gantt.svg
