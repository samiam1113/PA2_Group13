# ===== PA2 Makefile (MSYS2 / MinGW-w64, pthreads) =====

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O2
LDFLAGS := -pthread -lwinpthread
BIN     := chash
SRC     := chash.c HashFunctions.c
OBJ     := $(SRC:.c=.o)

.PHONY: all run clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# run with commands.txt (no other filenames)
run: $(BIN)
	./$(BIN) commands.txt

clean:
	rm -f $(OBJ) $(BIN) $(BIN).exe hash.log
