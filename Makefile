CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -g
SRC = src
OBJ = obj

main : $(OBJ)/main.o
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJ)/%.o : $(SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf server $(OBJ)/*.o
