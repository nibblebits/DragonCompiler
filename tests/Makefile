# Builds the tests
OBJECTS=./build/variable_assignment.o ./build/advanced_exp.o ./build/advanced_exp_neg.o ./build/function_call_test_one_argument.o ./build/function_call_test_two_arguments.o ./build/if_statement_test.o ./build/preprocessor_macro_test.o ./build/structure_test.o ./build/bitwise_not_with_addition.o ./build/bitshift_and_test.o ./build/preprocessor_line_macro_test.o ./build/typedef_test.o ./build/while_test.o
EXECUTABLES=./build/variable_assignment ./build/advanced_exp ./build/advanced_exp_neg ./build/function_call_test_one_argument ./build/function_call_test_two_arguments ./build/if_statement_test ./build/preprocessor_macro_test ./build/structure_test ./build/bitwise_not_with_addition ./build/bitshift_and_test ./build/preprocessor_line_macro_test ./build/typedef_test ./build/while_test.o
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

./build/if_statement_test.o:./units/if_statement_test.c
	../main ./units/if_statement_test.c ./build/if_statement_test

./build/preprocessor_macro_test.o:./units/preprocessor_macro_test.c
	../main ./units/preprocessor_macro_test.c ./build/preprocessor_macro_test

./build/structure_test.o:./units/structure_test.c
	../main ./units/structure_test.c ./build/structure_test

./build/bitwise_not_with_addition.o:./units/bitwise_not_with_addition.c
	../main ./units/bitwise_not_with_addition.c ./build/bitwise_not_with_addition


./build/bitshift_and_test.o:./units/bitshift_and_test.c
	../main ./units/bitshift_and_test.c ./build/bitshift_and_test


./build/preprocessor_line_macro_test.o:./units/preprocessor_line_macro_test.c
	../main ./units/preprocessor_line_macro_test.c ./build/preprocessor_line_macro_test

./build/typedef_test.o:./units/typedef_test.c
	../main ./units/typedef_test.c ./build/typedef_test

./build/while_test.o:./units/while_test.c
	../main ./units/while_test.c ./build/while_test

clean:
	rm -rf ${OBJECTS}
	rm -rf ${EXECUTABLES}