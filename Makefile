# Makefile - The Legend of June
# Arquitectura de Computadores, TEC, II Semestre 2026
#
# Uso:
#   make                 compila la version en C++            -> ./june
#   make USAR_ASM=1      compila usando las rutinas en AA32   (Etapa 3)
#   make run             compila y ejecuta el juego
#   make bench           corre --bench para N = 10, 20, 40 y 64 (sin pantalla)
#   make bench-pantalla  igual, pero con pantalla (mide refresh() de ncurses)
#   make clean           borra lo compilado
#
# Requisitos en Raspberry Pi OS / Debian:
#   sudo apt install build-essential libncurses-dev

CXX      ?= g++
TARGET   := june
SRC      := main.cpp
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
LDLIBS   := -lncurses

# Interruptor de la Etapa 3: rutinas en ensamblador AA32
# Con USAR_ASM=1 se define -DUSAR_ASM (main.cpp deja de usar sus versiones
# en C++) y se ensamblan todos los archivos asm/*.s
ifeq ($(USAR_ASM),1)
    CXXFLAGS += -DUSAR_ASM
    ASM_SRC  := $(wildcard asm/*.s)
    ASM_OBJ  := $(ASM_SRC:.s=.o)
    ifneq ($(filter armv7l armv8l,$(shell uname -m)),)
        CXXFLAGS += -mfpu=neon
        ASFLAGS  += -mfpu=neon
    endif
else
    ASM_OBJ :=
endif

.PHONY: all run bench bench-pantalla clean

all: $(TARGET)

$(TARGET): $(SRC) $(ASM_OBJ)
	$(CXX) $(CXXFLAGS) $(SRC) $(ASM_OBJ) -o $@ $(LDLIBS)

asm/%.o: asm/%.s
	$(AS) $(ASFLAGS) $< -o $@

run: $(TARGET)
	./$(TARGET)

bench: $(TARGET)
	@for n in 10 20 40 64; do ./$(TARGET) --bench $$n --sin-pantalla; echo; done

bench-pantalla: $(TARGET)
	@for n in 10 20 40 64; do ./$(TARGET) --bench $$n; echo; done

clean:
	rm -f $(TARGET) asm/*.o
