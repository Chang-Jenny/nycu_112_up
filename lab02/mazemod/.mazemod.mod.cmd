savedcmd_/build/mazemod/mazemod.mod := printf '%s\n'   mazemod.o | awk '!x[$$0]++ { print("/build/mazemod/"$$0) }' > /build/mazemod/mazemod.mod
