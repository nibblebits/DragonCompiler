#ifndef MISC_H
#define MISC_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

/**
 * No idea what to appropiatly call this, the name change can wait for now..
 */


// Used to represent a pointer variable that will be modified to provide
// an output value. Obviously theirs no real impact in the language
// just allows us to be tidy
#define __out__ 

// The opposit to __out__. Represents a pointer value that is coming in.
#define __in__



/**
 * Matches the given input with the second input whilst taking the delimieter into account.
 * "input2" must be null terminated with no delmieter.
 * "input1" can end with either a null terminator of the given delimieter.
 */
int str_matches(const char *input, const char *input2, char delim);
bool file_exists(const char* filename);

// Move to a better place
bool is_unary_operator(const char* op);

bool is_right_operanded_unary_operator(const char* op);
#endif