CC = gcc
CFLAGS = -g
LDFLAGS =

TARGET = gif_tester
OBJS = gif.o gif_tester.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

gif.o: gif.h

clean:
	rm -f $(TARGET) $(OBJS)