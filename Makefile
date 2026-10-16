# Evangelion: Proyecto de Crianza
#
#   make          compila
#   make run      compila y juega
#   make check    compila con todos los avisos prendidos
#   make clean    borra lo generado
#
# En Windows usá mingw32-make o GNU make; el ejecutable sale igual.

CC      ?= gcc
FUENTE  := evangelion_loving_project.c

UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

# NAME es el nombre real del archivo; TARGET es el mismo nombre escapado para
# make. Sin el escape, "Evangelion Loving Project.exe" se lee como tres
# objetivos y el archivo se compila tres veces. En el comando va $(NAME) entre
# comillas: TARGET lleva barras que el linker se tragaría como parte del nombre.
ifeq ($(UNAME_S),Windows_NT)
NAME    := Evangelion Loving Project.exe
TARGET  := Evangelion\ Loving\ Project.exe
# Sin esto, los acentos se rompen al compilar y al imprimir.
CFLAGS  += -finput-charset=UTF-8 -fexec-charset=UTF-8
else
NAME    := evangelion_loving_project
TARGET  := $(NAME)
endif

CFLAGS  += -std=c11 -O2 -Wall -Wextra -pedantic

.PHONY: all run check clean

all: $(TARGET)

$(TARGET): $(FUENTE)
	$(CC) $(CFLAGS) -o "$(NAME)" $(FUENTE)

run: $(TARGET)
	./"$(NAME)"

# Todo prendido a la vez. Debería salir limpio.
check:
	$(CC) $(CFLAGS) -Wconversion -Wshadow -Wcast-qual -Wformat=2 \
	      -Wundef -Wwrite-strings -Wmissing-prototypes -Wstrict-prototypes \
	      -fsyntax-only $(FUENTE)
	@echo "check: sin avisos"

clean:
	rm -f "$(NAME)" *.o *.exe
