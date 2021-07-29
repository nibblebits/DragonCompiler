# Builds the tests
OBJECTS=./build/variable_assignment.o ./build/advanced_exp.o ./build/advanced_exp_neg.o ./build/function_call_test_one_argument.o ./build/function_call_test_two_arguments.o
EXECUTABLES=./build/variable_assignment ./build/advanced_exp ./build/advanced_exp_neg ./build/function_call_test_one_argument ./build/function_call_test_two_arguments
all: ${OBJECTS}

./build/variable_assignment.o:./units/variable_assignment.c
	../main ./units/variable_assignment.c ./build/variable_assignment

./build/advanced_exp.o:./units/advanced_exp.c
	../main ./units/advanced_exp.c ./build/advanced_exp

./build/advanced_exp_neg.o:./units/advanced_exp_neg.c
	../main ./units/advanced_exp_neg.c ./build/advanced_exp_neg

./build/function_call_test_one_argument.o:./units/function_call_test_one_argument.c
	../main ./units/function_call_test_one_argument.c ./build/function_call_test_one_argument

./build/function_call_test_two_arguments.o:./units/function_call_test_two_arguments.c
	../main ./units/function_call_test_two_arguments.c ./build/function_call_test_two_arguments
clean:
	rm -rf ${OBJECTS}
	rm -rf ${EXECUTABLES}