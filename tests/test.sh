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

#LINE MACRO TEST DISABLED FOR NOW.
#echo -e "Running preprocessor __LINE__ macro test"
#./build/preprocessor_line_macro_test
#if [ $? -ne 12 ]; then
#    echo -e "preprocessor __LINE__ macro failed"
#    res_code=1
#else
#    echo -e "preprocessor __LINE__ macro passed"
#fi

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

echo -e "Preprocessor undef test"
./build/preprocessor_undef_test
if [ $? -ne 22 ]; then
    echo -e "Preprocessor undef failed"
    res_code=1
else
    echo -e "Preprocessor undef passed"
fi

echo -e "Preprocessor warning test"
./build/preprocessor_warning_test
if [ $? -ne 22 ]; then
    echo -e "Preprocessor warning test failed"
    res_code=1
else
    echo -e "Preprocessor warning test passed"
fi

echo -e "Binary number test"
./build/binary_number_test
if [ $? -ne 3 ]; then
    echo -e "Binary number test failed"
    res_code=1
else
    echo -e "Binary number test passed"
fi

echo -e "hex number test"
./build/hex_test
if [ $? -ne 21 ]; then
    echo -e "hex number test failed"
    res_code=1
else
    echo -e "hex number test passed"
fi


echo -e "Long directive test"
./build/long_directive_test
if [ $? -ne 10 ]; then
    echo -e "Long directive test failed"
    res_code=1
else
    echo -e "Long directive test passed"
fi

echo -e "Preprocessor macro function in IF test"
./build/preprocessor_macro_func_in_if
if [ $? -ne 50 ]; then
    echo -e "Preprocessor macro function in IF test failed"
    res_code=1
else
    echo -e "Preprocessor macro function in IF test passed"
fi

echo -e "Preprocessor macro function in IF 2 test"
./build/preprocessor_macro_func_in_if_2
if [ $? -ne 50 ]; then
    echo -e "Preprocessor macro function in IF 2 test failed"
    res_code=1
else
    echo -e "Preprocessor macro function in IF 2 test passed"
fi

echo -e "Preprocessor multi definition with macro if test"
./build/preprocessor_definition_with_macro_if
if [ $? -ne 26 ]; then
    echo -e "Preprocessor multi definition with macro if test failed"
    res_code=1
else
    echo -e "Preprocessor multi definition with macro if test passed"
fi


echo -e "Preprocessor elif test"
./build/preprocessor_elif_test
if [ $? -ne 90 ]; then
    echo -e "Preprocessor elif test failed"
    res_code=1
else
    echo -e "Preprocessor elif test passed"
fi
echo -e "Preprocessor typedef in definition test"
./build/preprocessor_typedef_in_def
if [ $? -ne 50 ]; then
    echo -e "Preprocessor typedef in definition test failed"
    res_code=1
else
    echo -e "Preprocessor typedef in definition test passed"
fi


echo -e "Structure forward declare test"
./build/struct_forward_declr_test
if [ $? -ne 22 ]; then
    echo -e "Structure forward declare test failed"
    res_code=1
else
    echo -e "Structure forward declare test passed"
fi

echo -e "Structure with declaration test"
./build/struct_with_declaration_test
if [ $? -ne 30 ]; then
    echo -e "Structure with declaration test failed"
    res_code=1
else
    echo -e "Structure with declaration test passed"
fi

echo -e "Structure with no type name test"
./build/struct_no_name_test
if [ $? -ne 20 ]; then
    echo -e "Structure with no type name test failed"
    res_code=1
else
    echo -e "Structure with no type name test passed"
fi

echo -e "Union test"
./build/union_test
if [ $? -ne 50 ]; then
    echo -e "Union test failed"
    res_code=1
else
    echo -e "Union test passed"
fi

echo -e "Sub-struct test"
./build/substruct_test
if [ $? -ne 10 ]; then
    echo -e "Sub-struct test failed"
    res_code=1
else
    echo -e "Sub-struct test passed"
fi

echo -e "printf test"
./build/printf_test
if [ $? -ne 22 ]; then
    echo -e "printf test failed"
    res_code=1
else
    echo -e "printf test passed"
fi

echo -e "Preprocessor concat test"
./build/preprocessor_concat_test
if [ $? -ne 0 ]; then
    echo -e "Preprocessor concat failed"
    res_code=1
else
    echo -e "Preprocessor concat passed"
fi


echo -e "Pointer assignment test"
./build/pointer_assignment
if [ $? -ne 30 ]; then
    echo -e "Pointer assignment test failed"
    res_code=1
else
    echo -e "Pointer assignment test passed"
fi

echo -e "Multi-variable test"
./build/multi-variable
if [ $? -ne 14 ]; then
    echo -e "Multi-variable test failed"
    res_code=1
else
    echo -e "Multi-variable test passed"
fi

echo -e "Array test"
./build/array_test
if [ $? -ne 111 ]; then
    echo -e "Array test failed"
    res_code=1
else
    echo -e "Array test passed"
fi


echo -e "Advanced access test"
./build/advanced_access
if [ $? -ne 50 ]; then
    echo -e "Advanced access test failed"
    res_code=1
else
    echo -e "Advanced access test passed"
fi

echo -e "Structure pointer returned from function then accessed test"
./build/structure_pointer_ret_func
if [ $? -ne 75 ]; then
    echo -e "Structure pointer returned from function then accessed test failed"
    res_code=1
else
    echo -e "Structure pointer returned from function then accessed test passed"
fi

echo -e "Structure pointer casted to another structure test"
./build/struct_casted
if [ $? -ne 50 ]; then
    echo -e "Structure pointer casted to another structure test failed"
    res_code=1
else
    echo -e "Structure pointer casted to another structure test passed"
fi


echo -e "Structure array set test.."
./build/structure_array_set_test
if [ $? -ne 50 ]; then
    echo -e "Structure array set test failed"
    res_code=1
else
    echo -e "Structure array set test passed"
fi


echo -e "Pointer cast test.."
./build/pointer_cast_test
if [ $? -ne 50 ]; then
    echo -e "Pointer cast test failed"
    res_code=1
else
    echo -e "Pointer cast test passed"
fi



echo -e "Structure access with pointers and array test"
./build/pointer_cast_test
if [ $? -ne 50 ]; then
    echo -e "Structure access with pointers and array test failed"
    res_code=1
else
    echo -e "Structure access with pointers and array test passed"
fi

echo -e "Pointer addition test"
./build/pointer_addition_test
if [ $? -ne 20 ]; then
    echo -e "Pointer addition test failed"
    res_code=1
else
    echo -e "Pointer addition test passed"
fi
echo -e "Array get pointer test"
./build/array_get_pointer_test
if [ $? -ne 20 ]; then
    echo -e "Array get pointer test failed"
    res_code=1
else
    echo -e "Array get pointer test passed"
fi

echo -e "Logical Operator Test"
./build/logical_operator_test
if [ $? -ne 1 ]; then
    echo -e "Logical operator test failed"
    res_code=1
else
    echo -e "Logical operator test passed"
fi


echo -e "Decrement Operator Test"
./build/decrement_operator_test
if [ $? -ne 0 ]; then
    echo -e "Decrement operator test failed"
    res_code=1
else
    echo -e "Decrement operator test passed"
fi


echo -e "Const Char pointer Test"
./build/const_char_pointer_test
if [ $? -ne 104 ]; then
    echo -e "Const Char pointer test failed"
    res_code=1
else
    echo -e "Const Char pointer test passed"
fi

echo -e "Macro string test Test"
./build/preprocessor_macro_string_test
if [ $? -ne 1 ]; then
    echo -e "Macro string test failed"
    res_code=1
else
    echo -e "Macro string test passed"
fi


echo -e "Logical not test "
./build/logical_not_test
if [ $? -ne 1 ]; then
    echo -e "Logical not test failed"
    res_code=1
else
    echo -e "Logical not test passed"
fi


echo -e "Offset Of test "
./build/offsetof_test
if [ $? -ne 150 ]; then
    echo -e "Offsetof test failed"
    res_code=1
else
    echo -e "Offsetof test passed"
fi


echo -e "Valist test "
./build/valist_test
if [ $? -ne 100 ]; then
    echo -e "Valist test failed"
    res_code=1
else
    echo -e "Valist test passed"
fi



echo -e "All tests finished"
exit $res_code
