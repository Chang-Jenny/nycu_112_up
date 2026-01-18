MAZEMOD_DIR="/mazemod"
PROC_MAZE="/proc/maze"
MAZE_MODULE=$MAZEMOD_DIR/mazemod.ko
MODULE_NAME="mazemod"
MAZETEST=$MAZEMOD_DIR/mazetest

# if /proc/maze not exists, run insmod
if [ ! -f $PROC_MAZE ]; then
    insmod $MAZE_MODULE
fi

# read argument from command line representing test case
# if no argument, run all test cases

# run mazetest 
if [ $# -eq 0 ]; then
    for i in $(seq 0 6); do
        $MAZETEST $i
    done
fi

if [ $# -eq 1 ]; then
    $MAZETEST $1
fi

# if /proc/maze exists, run rmmod
if [ -f $PROC_MAZE ]; then
    rmmod $MODULE_NAME
fi