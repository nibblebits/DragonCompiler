#ifndef COMPILER_H
#define COMPILER_H
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "helpers/vector.h"

// Macro's make life cleaner..
#define S_EQ(str, str2) \
    strcmp(str, str2) == 0

#define OPERATOR_CASE_EXCLUDING_DIVISON \
    case '+': \
    case '-': \
    case '*': \
    case '>': \
    case '<': \
    case '^': \
    case '%': \
    case '!': \
    case '=': \
    case '~': \
    case '|': \
    case '&'

#define NUMERIC_CASE \
    case '0': \
    case '1': \
    case '2': \
    case '3': \
    case '4': \
    case '5': \
    case '6': \
    case '7': \
    case '8': \
    case '9' \

#define SYMBOL_CASE \
    case '{': \
    case '}': \
    case '.': \
    case ':': \
    case ';': \
    case '(': \
    case ')': \
    case '[': \
    case ']': \
    case ',': \
    case '#': \
    case '\\'
    

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
    TOKEN_TYPE_OPERATOR,
    TOKEN_TYPE_SYMBOL,
    TOKEN_TYPE_NUMBER,
    TOKEN_TYPE_STRING,
    TOKEN_TYPE_COMMENT,
    TOKEN_TYPE_NEW_LINE,
};

struct token
{
    int type;
    struct pos pos;
    union
    {
        char cval;
        const char* sval;
        unsigned int inum;
        unsigned long lnum;
        unsigned long long llnum;
    };    
};

enum
{
    COMPILER_FILE_COMPILED_OK,
    COMPILER_FAILED_WITH_ERRORS,
};


/**
 * Called to issue a compiler error and terminate the compiler
 */
void compiler_error(struct compile_process* compiler, const char *msg, ...);
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

/**
 * Peeks in the stream for the next char from the current file.
 * Does not impact the file pointer.
 */
char compile_process_peek_char(struct compile_process* process);


/**
 * Unsets the given character pushing it back into the end of the input stream.
 * The next time compile_process_next_char or compile_process_peek_char is called
 * the given character provided here will be given
 */
void compile_process_push_char(struct compile_process* process, char c);



#endif