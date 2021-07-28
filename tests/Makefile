# Builds the tests
OBJECTS=./build/variable_assignment.o
EXECUTABLES=./build/variable_assignment
all: ${OBJECTS}

./build/variable_assignment.o:./units/variable_assignment.c
	../main ./units/variable_assignment.c ./build/variable_assignment


clean:
	rm -rf ${OBJECTS}
	rm -rf ${EXECUTABLES}