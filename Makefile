# Compiler
CC = gcc

# Compiler flags (add optimization and warning flags as necessary)
CFLAGS = -Wall -g -I/usr/include/libnl3

# Linker flags for required libraries
LDFLAGS = -lcap -lnl-3 -lnl-route-3

# Target executable name
TARGET = container

# Find all C source files in the current directory
SRCS = $(wildcard *.c)

# Object files (each .c becomes an .o)
OBJS = $(SRCS:.c=.o)

# Default rule to build the target executable
all: $(TARGET)

# Rule to link the target executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Rule to compile .c files into .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule to remove object files and the target
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets (prevent name conflict with files)
.PHONY: all clean
