# Builds the tests
OBJECTS=./build/variable_assignment.o ./build/advanced_exp.o ./build/advanced_exp_neg.o
EXECUTABLES=./build/variable_assignment ./build/advanced_exp ./build/advanced_exp_neg
all: ${OBJECTS}

./build/variable_assignment.o:./units/variable_assignment.c
	../main ./units/variable_assignment.c ./build/variable_assignment

./build/advanced_exp.o:./units/advanced_exp.c
	../main ./units/advanced_exp.c ./build/advanced_exp

./build/advanced_exp_neg.o:./units/advanced_exp_neg.c
	../main ./units/advanced_exp_neg.c ./build/advanced_exp_neg

clean:
	rm -rf ${OBJECTS}
	rm -rf ${EXECUTABLES}