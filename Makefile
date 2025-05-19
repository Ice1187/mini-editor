PROG_NAME = minied

all: 
	gcc -o $(PROG_NAME) main.c

.PHONY: clean
clean:
	rm $(PROG_NAME)
