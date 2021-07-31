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
if [ $? -ne 103 ]; then
    echo -e "structure test failed"
    res_code=1
else
    echo -e "structuretest passed"
fi






echo -e "All tests finished"
exit $res_code