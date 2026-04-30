CC = gcc
CFLAGS = -Wall -Wextra -Werror
LDFLAGS =

TARGET = displayer
OBJS = main.o

DRM_CFLAGS := $(shell pkg-config --cflags libdrm)
DRM_LIBS   := $(shell pkg-config --libs libdrm)

# Append to standard flags
CFLAGS += -Wall -Wextra $(DRM_CFLAGS)
LDFLAGS += $(DRM_LIBS)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)