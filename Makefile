INCLUDES= -I ./ -I ./helpers
OBJECTS= ./build/misc.o ./build/lexer.o ./build/token.o ./build/expressionable.o ./build/parser.o ./build/symresolver.o ./build/scope.o ./build/resolver.o ./build/rdefault.o ./build/helper.o ./build/codegen.o ./build/helpers/vector.o ./build/helpers/buffer.o ./build/helpers/hashmap.o ./build/compiler.o ./build/cprocess.o ./build/preprocessor.o ./build/array.o ./build/node.o
all: ${OBJECTS}
	gcc main.c -o main ${OBJECTS} -g
	cd ./tests && ./test.sh

./build/misc.o: ./misc.c
	gcc misc.c ${INCLUDES} -o ./build/misc.o -g -c

./build/preprocessor.o: ./preprocessor.c
	gcc preprocessor.c ${INCLUDES} -o ./build/preprocessor.o -g -c

./build/lexer.o: ./lexer.c
	gcc lexer.c ${INCLUDES} -o ./build/lexer.o -g -c

./build/token.o: ./token.c
	gcc token.c ${INCLUDES} -o ./build/token.o -g -c

./build/expressionable.o: ./expressionable.c
	gcc expressionable.c ${INCLUDES} -o ./build/expressionable.o -g -c

./build/parser.o: ./parser.c
	gcc parser.c ${INCLUDES} -o ./build/parser.o -g -c

./build/symresolver.o: ./symresolver.c
	gcc symresolver.c ${INCLUDES} -o ./build/symresolver.o -g -c

./build/codegen.o: ./codegen.c
	gcc codegen.c ${INCLUDES} -o ./build/codegen.o -g -c

./build/scope.o: ./scope.c
	gcc scope.c ${INCLUDES} -o ./build/scope.o -g -c

./build/helper.o: ./helper.c
	gcc helper.c ${INCLUDES} -o ./build/helper.o -g -c

./build/resolver.o: ./resolver.c
	gcc resolver.c ${INCLUDES} -o ./build/resolver.o -g -c

./build/compiler.o: ./compiler.c
	gcc compiler.c ${INCLUDES} -o ./build/compiler.o -g -c

./build/cprocess.o: ./cprocess.c
	gcc cprocess.c ${INCLUDES} -o ./build/cprocess.o -g -c

./build/array.o: ./array.c
	gcc array.c ${INCLUDES} -o ./build/array.o -g -c 

./build/node.o: ./node.c
	gcc node.c ${INCLUDES} -o ./build/node.o -g -c 

./build/rdefault.o: ./rdefault.c
	gcc rdefault.c ${INCLUDES} -o ./build/rdefault.o -g -c 


# Helper files
./build/helpers/vector.o: ./helpers/vector.c
	gcc ./helpers/vector.c ${INCLUDES} -o ./build/helpers/vector.o -g -c

./build/helpers/buffer.o: ./helpers/buffer.c
	gcc ./helpers/buffer.c ${INCLUDES} -o ./build/helpers/buffer.o -g -c

./build/helpers/hashmap.o: ./helpers/hashmap.c
	gcc ./helpers/hashmap.c ${INCLUDES} -o ./build/helpers/hashmap.o -g -c



clean:
	rm -rf ${OBJECTS}
	rm -rf ./main
	rm -rf ./a.out
	rm -rf ./test.asm
	cd ./tests && $(MAKE) clean