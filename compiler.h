#ifndef COMPILER_H
#define COMPILER_H
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <memory.h>
#include <string.h>
#include <stdbool.h>
#include "helpers/vector.h"

// Macro's make life cleaner..
#define S_EQ(str, str2) \
    (strcmp(str, str2) == 0)

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



enum
{
    // Signifies we are on the right node of a single part of an expression
    // I.e (50+20+40+96). 20 is the right node and so is 96.
    // This flag will be set in the event the right node of an expression part is being processed
    EXPRESSION_FLAG_RIGHT_NODE = 0b00000001,
};

/**
 * An expression state describes the state of an individual expression. 
 * These expression states are created when ever we enter a new expression i.e (50+20).
 * They are important as they help describe the state of an expression so during iterations
 * any function call will have a better understanding of what to do and what not to do.
 * 
 * The last thing we need is a corrupted assembly output that has unexpected behaviour
 * expression states help ensure we can avoid this problem by obaying rules we set our program.
 */
struct expression_state
{
    int flags;
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
    // Vector of struct <struct token> individual tokens (not pointers)
    struct vector* token_vec;

    // Contains pointers to the root of the tree
    // Vector of <struct node*> node pointers
    struct vector* node_tree_vec;

    // Used to store nodes that can be popped during parsing.
    // All nodes get pushed here, they can be popped to form other bigger nodes such as expressions
    // Vector of <struct node*> node pointers
    struct vector* node_vec;
  
    struct generator
    {
        struct states
        {
            // Expression state vector - A vector used as a stack of expression states.
            // Most recently pushed element is the current expression state rules.
            // Should pop from this vector when an expression ends, push when an expression starts
            // I.e opening brackets starts a new expression (50+20)
            // See enum expression_state for more information
            struct vector* expr;
        } states;
    } generator;
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
    NODE_TYPE_EXPRESSION,
    NODE_TYPE_NUMBER,
    NODE_TYPE_IDENTIFIER,
    NODE_TYPE_VARIABLE,
    NODE_TYPE_FUNCTION
};



enum
{
    PARSE_ALL_OK,
    PARSE_GENERAL_ERROR
};

enum
{
    CODEGEN_ALL_OK,
    CODEGEN_GENERAL_ERROR
};

enum
{
    DATATYPE_FLAG_IS_SIGNED = 0b00000001,
    DATATYPE_FLAG_IS_STATIC = 0b00000010
};

enum
{
    DATA_TYPE_CHAR,
    DATA_TYPE_SHORT,
    DATA_TYPE_INTEGER,
    DATA_TYPE_FLOAT,
    DATA_TYPE_DOUBLE,
    DATA_TYPE_LONG,
    DATA_TYPE_USER_DEFINED
};

struct datatype
{
    int flags;
    // The type this is i.e float, double, long
    int type;
    // The string equivilant for this type. Does not include unsigned or signed keywords.
    // if the type is "unsigned int" here will be written only "int"
    const char* type_str;
};

struct node
{
    int type;
    union
    {
        struct exp
        {
            struct node* left;
            struct node* right;
            // Operator for the expression
            const char* op;
        } exp;

        struct function
        {
            // The return type of this function.. I.e long, double, int
            struct datatype rtype;

        } func;

        struct variable
        {
            struct datatype type;
            const char* name;
            struct node* val;
        } var;
    };

    // Possible literal values for all nodes.
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
 * Parses the tree provided from lexical analysis
 */
int parse(struct compile_process *process);

/**
 * Generates the assembly output for the given AST
 */
int codegen(struct compile_process* process);

bool keyword_is_datatype(const char* str);

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