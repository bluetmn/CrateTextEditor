CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)
TARGET = build/crate

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS) $(TARGET)

# crate: crate.c
# 	$(CC) crate.c -o crate -Wall -Wextra -pedantic -std=c99