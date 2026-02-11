CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -std=c11 -O2
LDFLAGS :=

TARGET  := mapgen
SRC     := mapgen.c
OBJ     := $(SRC:.c=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJ) *.exe