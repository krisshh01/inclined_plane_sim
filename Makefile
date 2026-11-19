# Block on an Inclined Plane - Raylib + Raygui
#
#   make                 build ./incline_sim
#   make test            run the console physics + simulation tests (no Raylib needed)
#   make clean
#
# If Raylib / raygui.h are not in the default include/library paths, pass them in:
#   make INC="-I/path/to/raylib/src -I/path/to/raygui/src" LIB="-L/path/to/raylib/src"

CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -O2
INC     ?=
LIB     ?=

SRC     = main.c physics.c sim.c scene.c ui.c graphs.c fonts.c theme.c
TARGET  = incline_sim

ifeq ($(OS),Windows_NT)
    TARGET  = incline_sim.exe
    LDLIBS  = -lraylib -lopengl32 -lgdi32 -lwinmm
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        LDLIBS = -lraylib -framework IOKit -framework Cocoa -framework OpenGL
    else
        LDLIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
    endif
endif

$(TARGET): $(SRC) types.h physics.h sim.h scene.h ui.h graphs.h fonts.h font_data.h theme.h
	$(CC) $(CFLAGS) $(INC) $(SRC) -o $@ $(LIB) $(LDLIBS)

test:
	$(CC) -std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -O2 tests/test_physics.c physics.c -o tests/test_physics -lm
	$(CC) -std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -O2 tests/test_sim.c physics.c sim.c -o tests/test_sim -lm
	./tests/test_physics
	./tests/test_sim

clean:
	rm -f $(TARGET) tests/test_physics tests/test_sim

.PHONY: test clean
