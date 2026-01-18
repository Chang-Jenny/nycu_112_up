savedcmd_/build/verify/maze.mod := printf '%s\n'   maze.o | awk '!x[$$0]++ { print("/build/verify/"$$0) }' > /build/verify/maze.mod
