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



echo -e "All tests finished"
exit $res_code