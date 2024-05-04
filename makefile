CFLAGS=-std=c++17 -Wall -Wextra -g
LIBS=.\SDL2-2.28.1\x86_64-w64-mingw32\lib -lmingw32 -lSDL2main -lSDL2
INCLUDES=.\SDL2-2.28.1\x86_64-w64-mingw32\include\SDL2
all:
	gcc chip8.cpp -o chip8 $(CFLAGS) -L$(LIBS) -I$(INCLUDES)

debug:
	gcc chip8.cpp -o chip8 $(CFLAGS) -L$(LIBS) -I$(INCLUDES) -DDEBUG