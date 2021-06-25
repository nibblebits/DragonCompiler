#/bin/bash
./main > ./test.asm
nasm -c ./test.asm -o ./test.o
