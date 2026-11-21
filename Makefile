CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pthread
LDFLAGS = -pthread -lm

TARGET = minios
SRCS = src/main.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET)
