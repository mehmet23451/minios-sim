CC      := gcc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -pthread -Isrc -Isched
LDFLAGS := -pthread -lm

TARGET  := minios
SRCS    := src/main.c sched/sched.c
OBJS    := $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	./$(TARGET) sched -a fcfs --trace tests/sched-io.txt
	./$(TARGET) sched -a fcfs --csv tests/sched-io.txt
	./$(TARGET) sched -a fcfs --compare tests/sched-io.txt

asan: CFLAGS += -fsanitize=address,undefined -g -O1
asan: clean $(TARGET)

tsan: CFLAGS += -fsanitize=thread -g -O1
tsan: clean $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all test asan tsan clean
