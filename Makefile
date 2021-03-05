INCLUDES= -I ./
OBJECTS= ./build/misc.o ./build/lexer.o ./build/stack.o
all: ${OBJECTS}
	gcc main.c -o main ${OBJECTS} -g

./build/misc.o: ./misc.c
	gcc misc.c ${INCLUDES} -o ./build/misc.o -g -c

./build/lexer.o: ./lexer.c
	gcc lexer.c ${INCLUDES} -o ./build/lexer.o -g -c

./build/stack.o: ./stack.c
	gcc stack.c ${INCLUDES} -o ./build/stack.o -g -c

clean:
	rm -rf ${OBJECTS}
	rm ./main