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
		src/expand.c \
		src/prompt.c \
		src/jobs.c

OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ) $(TARGET)

tokenizer: src/tokenize_line.c
	$(CC) $(CFLAGS) -o tokenizer src/tokenize_line.c

scanner: src/scanner.c
	$(CC) $(CFLAGS) -o scanner src/scanner.c

.PHONY: all clean
