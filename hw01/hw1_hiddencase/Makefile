CC = x86_64-linux-gnu-gcc # ARCH=x86_64 CROSS_COMPILE=x86_64-linux-gnu-
# CC = g++
CFLAGS = -fPIC -Wall -Wextra # CFLAGS = -Wall -Wextra -Werror
LDFLAGS = -shared

LIB_SRC = logger_solver.c
LIB_OBJ = $(LIB_SRC:.c=.o)
LIB_TARGET = logger.so

APP_SRC = logger.c logger.h
APP_OBJ = $(APP_SRC:.c=.o)
APP_TARGET = logger

.PHONY: all clean

all: $(APP_TARGET) $(LIB_TARGET) # $(LIB_SOLVER_TARGET)

# logger.so: logger_solver.c
$(LIB_TARGET): $(LIB_SRC)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^ -ldl

# logger: logger.c 
# linker: -L. -llogger
$(APP_TARGET): $(APP_SRC) 
	$(CC) -o $@ $^ -lc

%.o: %.c
	$(CC) $(LDFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(APP_OBJ) $(APP_TARGET) $(LIB_OBJ) $(LIB_TARGET)
