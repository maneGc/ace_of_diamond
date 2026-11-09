# Makefile del motor Ace of Diamond
#   make            compila build/aod (con OpenMP si el compilador lo soporta)
#   make OPENMP=0   compila sin OpenMP (un solo hilo; p. ej. clang de macOS)
#   make clean      borra la compilación

CC      ?= gcc
OPENMP  ?= 1
CFLAGS  = -std=c11 -O3 -Wall -Wextra -Iinclude
LDLIBS  = -lm
ifeq ($(OPENMP),1)
  CFLAGS  += -fopenmp
  LDFLAGS += -fopenmp
endif

SRC = $(wildcard src/*.c)
OBJ = $(SRC:src/%.c=build/%.o)
BIN = build/aod

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.c include/aod.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build

.PHONY: all clean
