run: pad
	@./pad

pad: pad.c
	@$(CC) pad.c -o pad -Wall -Wextra -std=c99
