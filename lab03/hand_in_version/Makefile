CC = x86_64-linux-gnu-gcc # ARCH=x86_64 CROSS_COMPILE=x86_64-linux-gnu-
CFLAGS = -fPIC # CFLAGS = -Wall -Wextra -Werror
LDFLAGS = -shared


LIB_SRC = libmaze_dummy.c
LIB_OBJ = $(LIB_SRC:.c=.o)
LIB_TARGET = libmaze.so

LIB_SOLVER_SRC = libsolver.c
LIB_SOLVER_OBJ = $(LIB_SOLVER_SRC:.c=.o)
LIB_SOLVER_TARGET = libsolver.so

# APP_SRC = maze.c
# APP_OBJ = $(APP_SRC:.c=.o)
# APP_TARGET = maze

.PHONY: all clean

all: $(LIB_TARGET) $(APP_TARGET) $(LIB_SOLVER_TARGET)

# libmaze.so: libmaze_dummy.c
$(LIB_TARGET): $(LIB_SRC)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^ -ldl

# libsolver.so: libsolver.c
$(LIB_SOLVER_TARGET): $(LIB_SOLVER_SRC)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^ -ldl

# maze: maze.c moves.c
# linker: -L. -lmaze
# $(APP_TARGET): $(APP_SRC)
# 	$(CC) -o $@ $^ -L. -lmaze

%.o: %.c
	$(CC) $(LDFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(LIB_OBJ) $(LIB_TARGET) $(APP_OBJ) $(APP_TARGET) $(LIB_SOLVER_OBJ) $(LIB_SOLVER_TARGET)

