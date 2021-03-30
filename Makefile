INCLUDES= -I ./ -I ./helpers
OBJECTS= ./build/misc.o ./build/lexer.o ./build/parser.o ./build/helpers/vector.o ./build/helpers/buffer.o ./build/compiler.o ./build/cprocess.o
all: ${OBJECTS}
	gcc main.c -o main ${OBJECTS} -g

./build/misc.o: ./misc.c
	gcc misc.c ${INCLUDES} -o ./build/misc.o -g -c

./build/lexer.o: ./lexer.c
	gcc lexer.c ${INCLUDES} -o ./build/lexer.o -g -c

./build/parser.o: ./parser.c
	gcc parser.c ${INCLUDES} -o ./build/parser.o -g -c


./build/compiler.o: ./compiler.c
	gcc compiler.c ${INCLUDES} -o ./build/compiler.o -g -c

./build/cprocess.o: ./cprocess.c
	gcc cprocess.c ${INCLUDES} -o ./build/cprocess.o -g -c


# Helper files
./build/helpers/vector.o: ./helpers/vector.c
	gcc ./helpers/vector.c ${INCLUDES} -o ./build/helpers/vector.o -g -c

./build/helpers/buffer.o: ./helpers/buffer.c
	gcc ./helpers/buffer.c ${INCLUDES} -o ./build/helpers/buffer.o -g -c


clean:
	rm -rf ${OBJECTS}
	rm ./main