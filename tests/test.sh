#/usr/bin/bash

# Run the make file
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

echo -e "All tests finished"
exit $res_code