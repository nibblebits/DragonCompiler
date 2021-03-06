#ifndef COMPILER_H
#define COMPILER_H
#include <stdio.h>
#include <stdlib.h>
#include "helpers/vector.h"

struct pos
{
    int line;
    int col;
};

/**
 * This file represents a compilation process
 */
struct compile_process
{
    // The current file being compiled
    FILE* cfile;

    // Current line position information.
    struct pos pos;

    // Stack of tokens that have undergone lexcial analysis.
    struct vector* token_vec;
};


enum
{
    LEXICAL_ANALYSIS_ALL_OK,
    LEXICAL_ANLAYSIS_INPUT_ERROR
};

enum
{
    TOKEN_TYPE_IDENTIFIER,
    TOKEN_TYPE_KEYWORD,
    TOKEN_TYPE_STRING,
    
};

struct token
{
    int type;
    struct pos pos;
    union
    {
        char cval;
        const char* sval;
    };    
};

enum
{
    COMPILER_FILE_COMPILED_OK,
    COMPILER_FAILED_WITH_ERRORS,
};

/**
 * Compiles the file
 */
int compile_file(const char* filename);


/**
 * Lexical analysis
 */
int lex(struct compile_process* process);


/**
 * Compiler process
 */

/**
 * Creates a new compile process
 */
struct compile_process* compile_process_create(const char* filename);

/**
 * Returns the current file thats being processed
 */
FILE* compile_process_file(struct compile_process* process);

/**
 * Gets the next character from the current file
 */
char compile_process_next_char(struct compile_process* process);
#endif