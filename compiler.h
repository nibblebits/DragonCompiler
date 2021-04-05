#ifndef COMPILER_H
#define COMPILER_H
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <memory.h>
#include <string.h>
#include <stdbool.h>
#include "helpers/vector.h"

// 16 byte alignment for C programs.
#define C_STACK_ALIGNMENT 16

// Macro's make life cleaner..
#define S_EQ(str, str2) \
    (strcmp(str, str2) == 0)

#define OPERATOR_CASE_EXCLUDING_DIVISON \
    case '+':                           \
    case '-':                           \
    case '*':                           \
    case '>':                           \
    case '<':                           \
    case '^':                           \
    case '%':                           \
    case '!':                           \
    case '=':                           \
    case '~':                           \
    case '|':                           \
    case '&'

#define NUMERIC_CASE \
    case '0':        \
    case '1':        \
    case '2':        \
    case '3':        \
    case '4':        \
    case '5':        \
    case '6':        \
    case '7':        \
    case '8':        \
    case '9'

#define SYMBOL_CASE \
    case '{':       \
    case '}':       \
    case '.':       \
    case ':':       \
    case ';':       \
    case '(':       \
    case ')':       \
    case '[':       \
    case ']':       \
    case ',':       \
    case '#':       \
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

enum
{
    REGISTER_EAX_IS_USED = 0b00000001,
    REGISTER_EBX_IS_USED = 0b00000010,
    REGISTER_ECX_IS_USED = 0b00000100,
    REGISTER_EDX_IS_USED = 0b00001000,
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
    FILE *cfile;

    // Current line position information.
    struct pos pos;

    // Stack of tokens that have undergone lexcial analysis.
    // Vector of struct <struct token> individual tokens (not pointers)
    struct vector *token_vec;

    // Contains pointers to the root of the tree
    // Vector of <struct node*> node pointers
    struct vector *node_tree_vec;

    // Used to store nodes that can be popped during parsing.
    // All nodes get pushed here, they can be popped to form other bigger nodes such as expressions
    // Vector of <struct node*> node pointers
    struct vector *node_vec;

    struct generator
    {
        struct states
        {
            // Expression state vector - A vector used as a stack of expression states.
            // Most recently pushed element is the current expression state rules.
            // Should pop from this vector when an expression ends, push when an expression starts
            // I.e opening brackets starts a new expression (50+20)
            // See enum expression_state for more information
            struct vector *expr;
        } states;

        // This is a bitmask of flags for registers that are in use
        // if the bit is set the register is currently being used
        // this helps the system control how to do things.
        int used_registers;
    } generator;

    struct
    {
        struct scope *root;
        struct scope *current;
    } scope;
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
};

struct token
{
    int type;
    struct pos pos;
    union
    {
        char cval;
        const char *sval;
        unsigned int inum;
        unsigned long lnum;
        unsigned long long llnum;
    };
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
    DATATYPE_FLAG_IS_STATIC = 0b00000010,
    DATATYPE_FLAG_IS_CONST = 0b00000100
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
    const char *type_str;

    // The size in bytes of this datatype.
    size_t size;
};

/**
 * Scopes are composed of a hireachy of variable nodes
 */
struct scope
{
    // These are a vector of scope entities the actual element pushed to the vector
    // is creator defined, whoever decides to create a scope can push what they like
    struct vector *entities;

    // The parent scope NULL if we are at the root.
    struct scope *parent;
};

enum
{
    NODE_TYPE_EXPRESSION,
    NODE_TYPE_NUMBER,
    NODE_TYPE_IDENTIFIER,
    NODE_TYPE_VARIABLE,
    NODE_TYPE_FUNCTION,
    NODE_TYPE_BODY
};

struct node
{
    int type;
    union
    {
        struct exp
        {
            struct node *left;
            struct node *right;
            // Operator for the expression
            const char *op;
        } exp;

        struct function
        {
            // The return type of this function.. I.e long, double, int
            struct datatype rtype;

            // The name of the function
            const char *name;

            // a vector of "variable" nodes that represents the function arguments.
            // I.e int abc(int a, int b) here we have two arguments in the vector a and b.
            struct vector *argument_vector;

            // The body of this function, everything between the { } brackets.
            // This is NULL if this function is just a definition and its a pointer
            // to the body node if this function is declared. and has a full body
            struct node *body_node;
        } func;

        struct body
        {
            // Body nodes have a vector of nodes that represent statements
            // vector<struct node*>
            struct vector *statements;
        } body;

        struct variable
        {
            struct datatype type;
            const char *name;
            struct node *val;
            // If the variable is a constant value i.e it has the "DATATYPE_FLAG_IS_CONST" flag
            // Then during code generation the "const_val" may be accessed to use a precomputed
            // value of what it should be
            struct const_val
            {
                union
                {
                    char cval;
                    const char *sval;
                    unsigned int inum;
                    unsigned long lnum;
                    unsigned long long llnum;
                };
            } const_val;
        } var;
    };

    // Literal values for nodes of generic types. I.e numbers and identifiers
    union
    {
        char cval;
        const char *sval;
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
void compiler_error(struct compile_process *compiler, const char *msg, ...);
/**
 * Compiles the file
 */
int compile_file(const char *filename);

/**
 * Lexical analysis
 */
int lex(struct compile_process *process);

/**
 * Parses the tree provided from lexical analysis
 */
int parse(struct compile_process *process);

/**
 * Generates the assembly output for the given AST
 */
int codegen(struct compile_process *process);

bool keyword_is_datatype(const char *str);

/**
 * Compiler process
 */

/**
 * Creates a new compile process
 */
struct compile_process *compile_process_create(const char *filename);

/**
 * Returns the current file thats being processed
 */
FILE *compile_process_file(struct compile_process *process);

/**
 * Gets the next character from the current file
 */
char compile_process_next_char(struct compile_process *process);

/**
 * Peeks in the stream for the next char from the current file.
 * Does not impact the file pointer.
 */
char compile_process_peek_char(struct compile_process *process);

/**
 * Unsets the given character pushing it back into the end of the input stream.
 * The next time compile_process_next_char or compile_process_peek_char is called
 * the given character provided here will be given
 */
void compile_process_push_char(struct compile_process *process, char c);

struct scope *scope_create_root(struct compile_process *process);
struct scope *scope_new(struct compile_process *process);
void scope_finish(struct compile_process *process);
/**
 * Pushes an element to the current scope
 */
void scope_push(struct compile_process *process, void *ptr);

/**
 * Returns the last element from the given scope. If no element is in this scope
 * then it will take from the nearest scope that an element has been pushed too.
 * If still no variable was found NULL is returned.
 */
void *scope_last_entity(struct compile_process *process);

void *scope_iterate_back(struct scope *scope);
void scope_iteration_start(struct scope *scope);
void scope_iteration_end(struct scope *scope);

#endif