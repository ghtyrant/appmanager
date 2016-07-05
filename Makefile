CC := gcc
LD := $(CC)

# Glib/GObject/Gio includes and libraries
GLIB_CFLAGS  := $(shell pkg-config --cflags gtk+-3.0)
GLIB_LDFLAGS := $(shell pkg-config --libs gtk+-3.0)

CFLAGS  := -std=c11 -Wall -pedantic -g $(GLIB_CFLAGS)
LDFLAGS := $(GLIB_LDFLAGS) -ldl

TARGET  := appmanager
SOURCES := $(wildcard src/*.c)
OBJECTS := $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LD) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

run: $(TARGET)
	G_MESSAGES_DEBUG=all ./$(TARGET)

gdb: $(TARGET)
	G_MESSAGES_DEBUG=all gdb ./$(TARGET)

valgrind: all
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=definite,indirect,possible --show-reachable=no --leak-resolution=high --num-callers=20 --trace-children=no --child-silent-after-fork=yes --track-fds=yes --track-origins=yes ./$(TARGET) 2>&1 | tee valgrind.log

clean:
	rm $(TARGET) $(OBJECTS)
