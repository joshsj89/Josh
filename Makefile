CC = gcc
CFLAGS = -g -Wall -Wextra
TARGET = josh
SRC = src/main.c \
		src/shell.c \
		src/parser.c \
		src/execute.c \
		src/builtins.c \
		src/history.c \
		src/line_editor.c \
		src/completion.c \
		src/expand.c

OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ) $(TARGET)

tokenize: src/tokenize_line.c
	$(CC) $(CFLAGS) -o tokenize src/tokenize_line.c

.PHONY: all clean
