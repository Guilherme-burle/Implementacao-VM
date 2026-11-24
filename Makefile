CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pthread
TARGET = vm

all: $(TARGET)

$(TARGET): vm.c
	$(CC) $(CFLAGS) vm.c -o $(TARGET)

clean:
	rm -f $(TARGET) saida.txt

.PHONY: all clean