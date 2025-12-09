CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O2
LDFLAGS := -pthread
BIN     := chash

# Only .c files here
SRC     := chash.c HashFunctions.c locks.c
OBJ     := $(SRC:.c=.o)

HEADERS := HashFunctions.h locks.h

.PHONY: all run clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(BIN)
	./$(BIN) commands.txt

clean:
	rm -f $(OBJ) $(BIN) $(BIN).exe hash.log
