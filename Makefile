CC=g++
CFLAGS=-Wall -O2
LDFLAGS=-lfmt

TARGET=test

.PHONY: all
all: $(TARGET)

$(TARGET): tcp.o main.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

tcp.o: tcp.cpp logger.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c $@ $<

main.o: main.cpp
	$(CC) $(CFLAGS) $(LDFLAGS) -c $@ $<

.PHONY: clean
clean:
	rm -rf tcp.o main.o $(TARGET)