CC = gcc
CFLAGS = -g -Wall -Wextra
TARGET = josh
SRC = src/main.c \
		src/shell.c \
		src/parser.c \
		src/execute.c \
		src/builtins.c \
		src/history.c

OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ) $(TARGET)

.PHONY: all clean