# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -D_TIMESPEC_DEFINED
LDFLAGS = -L. -lpthreadGC2

# List of all .c files
SRCS = chash.c HashFunctions.c hash_logger.c rwlock.c

# Automatically generate a list of .o files
OBJS = $(SRCS:.c=.o)

# Main executable is chash
TARGET = chash

# Default build rule
all: $(TARGET)

# Link the final program
$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Compile each .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Remove build files
clean:
	rm -f $(OBJS) $(TARGET)

# Optional: rebuild everything
rebuild: clean all