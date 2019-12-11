

OBJS = beesweeper.c
OBJ_NAME = beesweeper
CC = gcc
LINKER_FLAGS = -lSDL2 -lSDL2_gfx -lSDL2_ttf -lSDL2_image -lm
COMPILER_FLAGS = -Wall -g

all: $(OBJS)
	$(CC) $(OBJS) $(COMPILER_FLAGS) $(LINKER_FLAGS) -o $(OBJ_NAME)
