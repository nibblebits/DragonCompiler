INCLUDES= -I ./
OBJECTS= ./build/misc.o
all:
	gcc main.c -o main ${OBJECTS}

misc.c:./build/misc.o
	gcc misc.c ${INCLUDES} -o ./build.misc.o -o 

