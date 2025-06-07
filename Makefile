PROG_NAME = minied
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -Werror

all: 
	gcc -o $(PROG_NAME) $(CFLAGS) main.c

.PHONY: clean
clean:
	rm $(PROG_NAME)
