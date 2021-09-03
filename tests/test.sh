#/usr/bin/bash

# Run the make file
# ATTENTION, ENSURE THAT ALL TESTING RETURNS INTEGERS BETWEEN 0-255
# THIS IS THE RANGE OF THE ERROR CODE, DO NOT ATTEMPT TO OVERFLOW
# IF YOU RETURN -1 FROM YOUR UNIT TEST THEN BE PREPARED TO COMPARE FOR UNSIGNED INTEGER
# 255 
make clean
make
echo -e "\n\n"
echo -e "TESTING"
echo "---------------------------------"
if [ $? -ne 0 ]; then
    echo "Make failed"
    exit -1
fi

res_code=0
echo -e "Make returned OK"

echo -e "Running variable assignment test 56+87"
./build/variable_assignment
if [ $? -ne 143 ]; then
    echo -e "Variable assignment test failed"
    res_code=1
else
    echo -e "Variable assignment test passed"
fi

echo -e "Running advanced expression test 54+25/40*90"
./build/advanced_exp
if [ $? -ne 54 ]; then
    echo -e "Advanced expression failed"
    res_code=1
else
    echo -e "Advanced expression passed"
fi

echo -e "Running advanced expression test with negation 54+25/40*90+-50"
./build/advanced_exp_neg
if [ $? -ne 4 ]; then
    echo -e "Advanced expression with negation failed"
    res_code=1
else
    echo -e "Advanced expression with negation passed"
fi

echo -e "Running function call test with one argument"
./build/function_call_test_one_argument
if [ $? -ne 60 ]; then
    echo -e "Function call test with one argument failed"
    res_code=1
else
    echo -e "Function call test with one argument passed"
fi

echo -e "Running function call test with two arguments"
./build/function_call_test_two_arguments
if [ $? -ne 32 ]; then
    echo -e "Function call test with two arguments failed"
    res_code=1
else
    echo -e "Function call test with two arguments passed"
fi

echo -e "Running IF statement test"
./build/if_statement_test
if [ $? -ne 20 ]; then
    echo -e "IF statement test failed"
    res_code=1
else
    echo -e "IF statement test passed"
fi


echo -e "Running preprocessor macro test"
./build/preprocessor_macro_test
if [ $? -ne 45 ]; then
    echo -e "preprocessor macro test failed"
    res_code=1
else
    echo -e "preprocessor macro test passed"
fi


echo -e "Running structure test"
./build/structure_test
if [ $? -ne 106 ]; then
    echo -e "structure test failed"
    res_code=1
else
    echo -e "structure test passed"
fi

echo -e "Running bitwise not with addition test"
./build/bitwise_not_with_addition
if [ $? -ne 39 ]; then
    echo -e "bitwise not with addition test failed"
    res_code=1
else
    echo -e "bitwise not with addition test passed"
fi


echo -e "Running bitshift left with and test"
./build/bitshift_and_test
if [ $? -ne 2 ]; then
    echo -e "Running bitshift left with and failed"
    res_code=1
else
    echo -e "Running bitshift left with and passed"
fi


echo -e "Running preprocessor __LINE__ macro test"
./build/preprocessor_line_macro_test
if [ $? -ne 12 ]; then
    echo -e "preprocessor __LINE__ macro failed"
    res_code=1
else
    echo -e "preprocessor __LINE__ macro passed"
fi

echo -e "Running typedef macro test"
./build/typedef_test
if [ $? -ne 50 ]; then
    echo -e "typedef macro failed"
    res_code=1
else
    echo -e "typedef macro passed"
fi

echo -e "Running while statement test"
./build/while_test
if [ $? -ne 2 ]; then
    echo -e "while statement test failed"
    res_code=1
else
    echo -e "while statement test passed"
fi

echo -e "Running do while statement test"
./build/do_while_test
if [ $? -ne 3 ]; then
    echo -e "do while statement test failed"
    res_code=1
else
    echo -e "do while statement test passed"
fi

echo -e "Running break test"
./build/do_while_test
if [ $? -ne 3 ]; then
    echo -e "break test failed"
    res_code=1
else
    echo -e "break test passed"
fi

echo -e "Running for loop test"
./build/for_loop_test
if [ $? -ne 26 ]; then
    echo -e "for loop test failed"
    res_code=1
else
    echo -e "for loop test test passed"
fi

echo -e "Running switch statement test"
./build/switch_statement_test
if [ $? -ne 17 ]; then
    echo -e "switch statement failed"
    res_code=1
else
    echo -e "switch statement test passed"
fi

echo -e "Running goto statement test"
./build/goto_test
if [ $? -ne 10 ]; then
    echo -e "goto statement failed"
    res_code=1
else
    echo -e "goto statement test passed"
fi


echo -e "Running comments test"
./build/comments_test
if [ $? -ne 50 ]; then
    echo -e "Comments test failed"
    res_code=1
else
    echo -e "Comments test passed"
fi

echo -e "Advanced expression parentheses"
./build/advanced_exp_parentheses
if [ $? -ne 144 ]; then
    echo -e "Advanced expression parentheses test failed"
    res_code=1
else
    echo -e "Advanced expression parentheses test passed"
fi

echo -e "Preprocessor macro defined test"
./build/preprocessor_macro_defined_test
if [ $? -ne 10 ]; then
    echo -e "Preprocessor macro defined test failed"
    res_code=1
else
    echo -e "Preprocessor macro defined test passed"
fi


echo -e "Tenary test"
./build/tenary_test
if [ $? -ne 100 ]; then
    echo -e "Tenary test failed"
    res_code=1
else
    echo -e "Tenary test passed"
fi


echo -e "Preprocessor logical OR test"
./build/preprocessor_logical_or_test
if [ $? -ne 1 ]; then
    echo -e "Preprocessor logical OR test failed"
    res_code=1
else
    echo -e "Preprocessor logical OR test passed"
fi


echo -e "Preprocessor new line test"
./build/preprocessor_logical_or_test
if [ $? -ne 1 ]; then
    echo -e "Preprocessor new line test failed"
    res_code=1
else
    echo -e "Preprocessor new line test passed"
fi

echo -e "New line test"
./build/new_line_seperator
if [ $? -ne 70 ]; then
    echo -e "New line test failed"
    res_code=1
else
    echo -e "New line test passed"
fi

echo -e "Preprocessor ifndef test"
./build/preprocessor_ifndef_macro
if [ $? -ne 17 ]; then
    echo -e "Preprocessor ifndef failed"
    res_code=1
else
    echo -e "Preprocessor ifndef passed"
fi


echo -e "Preprocessor nested if test"
./build/preprocessor_ifndef_macro
if [ $? -ne 17 ]; then
    echo -e "Preprocessor nested if failed"
    res_code=1
else
    echo -e "Preprocessor nested if  passed"
fi

echo -e "Advanced expression test with parentheses 2 test"
./build/advanced_exp_parentheses2
if [ $? -ne 40 ]; then
    echo -e "Advanced expression test with parentheses 2 failed"
    res_code=1
else
    echo -e "Advanced expression test with parentheses 2 passed"
fi

echo -e "Advanced expression test with parentheses 3 test"
./build/advanced_exp_parentheses3
if [ $? -ne 66 ]; then
    echo -e "Advanced expression test with parentheses 3 failed"
    res_code=1
else
    echo -e "Advanced expression test with parentheses 3 passed"
fi

echo -e "Preprocessor advanced expression nested parntheses test"
./build/preprocessor_parentheses_test
if [ $? -ne 55 ]; then
    echo -e "Preprocessor advanced expression nested parntheses failed"
    res_code=1
else
    echo -e "Preprocessor advanced expression nested parntheses passed"
fi


echo -e "Preprocessor advanced expression definition test"
./build/preprocessor_advanced_def_exp
if [ $? -ne 60 ]; then
    echo -e "Preprocessor advanced expression definition test failed"
    res_code=1
else
    echo -e "Preprocessor advanced expression definition test passed"
fi

echo -e "Preprocessor logical not test"
./build/preprocessor_logical_not_test
if [ $? -ne 50 ]; then
    echo -e "Preprocessor logical not test failed"
    res_code=1
else
    echo -e "Preprocessor logical not test passed"
fi

echo -e "Preprocessor logical not on keyword test"
./build/preprocessor_logical_not_on_keyword
if [ $? -ne 50 ]; then
    echo -e "Preprocessor logical not on keyword failed"
    res_code=1
else
    echo -e "Preprocessor logical not on keyword passed"
fi


echo -e "All tests finished"
exit $res_code
