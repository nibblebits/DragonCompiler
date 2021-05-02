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

// Have to skip the pushed base pointer and the return address
// when accessing function arguments passed to us and looking up the stack
#define C_OFFSET_FROM_FIRST_FUNCTION_ARGUMENT 8

// Macro's make life cleaner..
#define S_EQ(str, str2) \
    (strcmp(str, str2) == 0)

/**
 * Note: we do not include ")" as an operator only "(". ")" is classed as a symbol.
 */
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
    case '&':                           \
    case '(':                           \
    case '[':                           \
    case ']':                           \
    case ',':                            \
    case '.'                           \

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

/**
 * Cases for symbols, currently we don't treat ")" as an operator because
 * it is used to end an expression. "(" is used as an operator however but its special
 * the resulting operator will become "()" when parsing an expression, however
 * this change is done during parsing not lexing.
 */
#define SYMBOL_CASE \
    case '{':       \
    case '}':       \
    case ':':       \
    case ';':       \
    case '#':       \
    case '\\':      \
    case ')'

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
    EXPRESSION_FLAG_RIGHT_NODE = 0b0000000000000001,

    // This flag is set if the current expression is representing function arguments
    // seperated by a comma.
    EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS = 0b0000000000000010,
    // This flag is set if the current function call expression is on the left operand
    // for example test(50). Whilst we generate left node "test" this flag will be set.
    EXPRESSION_IN_FUNCTION_CALL_LEFT_OPERAND = 0b0000000000000100,
    // This flag is set if the expression represents an addition
    EXPRESSION_IS_ADDITION = 0b0000000000001000,
    EXPRESSION_IS_SUBTRACTION = 0b000000000010000,
    EXPRESSION_IS_MULTIPLICATION = 0b0000000000100000,
    EXPRESSIPON_IS_DIVISION = 0b0000000001000000,
};

#define EXPRESSION_UNINHERITABLE_FLAGS \
      (EXPRESSION_FLAG_RIGHT_NODE | EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS | \
      EXPRESSION_IS_ADDITION | EXPRESSION_IS_SUBTRACTION | EXPRESSION_IS_MULTIPLICATION | \
      EXPRESSIPON_IS_DIVISION)

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

// 4 bytes for a stack push and pop on 32 bit arch.
#define STACK_PUSH_SIZE 4
#define FUNCTION_CALL_ARGUMENTS_GET_STACK_SIZE(total_args) total_args *STACK_PUSH_SIZE

struct expression_state
{
    int flags;
    union
    {

        // Should be used when EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS flag is set.
        struct function_call_argument
        {
            // Total arguments in the current function call expression i.e (50, 20, 40) = 3 args
            size_t total_args;
        } fca;
    };
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

    // The symbol table that holds things like function names, global variables
    // data can point to the node in question, along with other relevant information
    struct vector *symbol_tbl;

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


struct sizeable_node
{
    size_t size;
    struct node* node;
};

enum
{
    SYMBOL_TYPE_NODE,
    SYMBOL_TYPE_UNKNOWN
};

struct symbol
{
    const char *name;
    int type;
    void *data;
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
    DATATYPE_FLAG_IS_SIGNED =  0b00000001,
    DATATYPE_FLAG_IS_STATIC =  0b00000010,
    DATATYPE_FLAG_IS_CONST  =  0b00000100,
    DATATYPE_FLAG_IS_POINTER = 0b00001000,
};

enum
{
    DATA_TYPE_CHAR,
    DATA_TYPE_SHORT,
    DATA_TYPE_INTEGER,
    DATA_TYPE_FLOAT,
    DATA_TYPE_DOUBLE,
    DATA_TYPE_LONG,
    DATA_TYPE_STRUCT,
    DATA_TYPE_UNION
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

    // The pointer depth of this datatype
    int pointer_depth;
};


/**
 * Scopes are composed of a hireachy of variable nodes
 */
struct scope
{
    /**
     * Flags of the scope that describe how it should be treated during certain operations
     * such as iteration
     */
    int flags;

    // These are a vector of scope entities the actual element pushed to the vector
    // is creator defined, whoever decides to create a scope can push what they like
    struct vector *entities;

    // The total size in bytes for the scope including all parent scopes
    // at this point in time. 16-byte aligned as required by C standard
    size_t size;

    // The parent scope NULL if we are at the root.
    struct scope *parent;
};

enum
{
    // Terrible name for this enum think of another
    // if this flag is set in any expression node then this means that it should be
    // treated as a unique expression such as ((50+20)+90) the (50+20) is a unique expression
    // that is what this flag means...
    EXPRESSION_FLAG_TREAT_UNIQUE = 0b00000001
};

enum
{
    NODE_TYPE_EXPRESSION,
    NODE_TYPE_EXPRESSION_PARENTHESIS,
    NODE_TYPE_NUMBER,
    NODE_TYPE_IDENTIFIER,
    NODE_TYPE_VARIABLE,
    NODE_TYPE_FUNCTION,
    NODE_TYPE_BODY,
    NODE_TYPE_STATEMENT_RETURN,
    NODE_TYPE_UNARY,
    NODE_TYPE_STRUCT
};

enum
{
    // Represents that this node is part of an expressions left or right operands.
    // For example the node that represents decimal 50 would not have this flag set test(50)
    // Since it is the only function argument. However in this case test(50*a) the number node
    // of "50" would have this flag set as its apart of an expression.
    NODE_FLAG_INSIDE_EXPRESSION  = 0b00000001,

};

struct node
{
    int type;
    // Generic flags for the given node
    int flags;
    union
    {
        struct exp
        {
            struct node *left;
            struct node *right;
            // Operator for the expression
            const char *op;
        } exp;

        /**
         * Represents a parenthesis expression i.e (50+20) (EXP)
         */
        struct parenthesis
        {
            // The expression between the brackets ()
            struct node* exp;
        } parenthesis;

        // Represents unary expressions i.e "~abc", "-a", "-90"
        struct unary
        {
            // In the case of indirection the "op" variable will only equal to one "*"
            // you can find the depth in the indirection structure within.
            const char* op;
            struct node* operand;

            union
            {
                // Represents a pointer access unary. i.e "***dog"
                struct indirection
                {
                    // The depth of the "indrection" i.e "***dog" has a depth of 3
                    int depth;

                } indirection;
            };
        } unary;
        
        // Represents a C structure in the tree.
        struct _struct
        {
            const char* name;
            struct node* body_n;
        } _struct;

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
            struct node *body_n;
        } func;

        struct body
        {
            // Body nodes have a vector of nodes that represent statements
            // vector<struct node*>
            struct vector *statements;

            // The size of the combined variables in this body.
            // Useful for accessing the bodies of structures and unions
            // where you need to know the size.
            size_t variable_size;
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

        union statement
        {
            // Represents a return statement.
            struct return_stmt
            {
                // The expresion of the return statement.
                struct node *exp;
            } ret;
        } stmt;
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
struct scope *scope_new(struct compile_process *process, int flags);
void scope_finish(struct compile_process *process);
/**
 * Pushes an element to the current scope
 */
void scope_push(struct compile_process *process, void *ptr, size_t elem_size);

/**
 * Returns the last element from the given scope. If no element is in this scope
 * then it will take from the nearest scope that an element has been pushed too.
 * If still no variable was found NULL is returned.
 */
void *scope_last_entity(struct compile_process *process);

void *scope_iterate_back(struct scope *scope);
void scope_iteration_start(struct scope *scope);
void scope_iteration_end(struct scope *scope);

/**
 * Returns the current scope for the code generator
 */
struct scope* scope_current(struct compile_process* process);

/**
 * Registers a symbol to the symbol table
 */
struct symbol *symresolver_register_symbol(struct compile_process *process, const char *sym_name, int type, void *data);
/**
 * Gets the registered symbol from the symbol table for the given name
 */
struct symbol *symresolver_get_symbol(struct compile_process *process, const char *name);

/**
 * Builds a symbol for the given node.
 * Note: The node must be fully parsed and initialized
 */
void symresolver_build_for_node(struct compile_process* process, struct node* node);


/**
 * Helper routines helper.c
 */

/**
 * Gets the offset from the given structure stored in the "compile_proc".
 * Looks for the given variable specified named by "var"
 * Returns the absolute position starting from 0 upwards.
 * 
 * I.e
 * 
 * struct abc
 * {
 *    int a;
 *    int b;
 * };
 * 
 * If we did abc.a then 0 would be returned. If we did abc.b then 4 would be returned. because
 * int is 4 bytes long. 
 * 
 * \param compile_proc The compiler process to peek for a structure for
 * \param struct_name The name of the given structure we must peek into
 * \param var_name The variable in the structure that we want the offset for.
 */
int struct_offset(struct compile_process* compile_proc, const char* struct_name, const char* var_name, struct node** var_node_out);

/**
 * Finds the first node of the given type.
 * 
 * For example lets imagine the expression "a.b.e.f"
 * if you called this function with NODE_TYPE_IDENTIFIER as the type and you passed in 
 * the right operand of the expression i.e a.E then you would find that the node of "b"
 * would be returned
 */
struct node* first_node_of_type(struct node* node, int type);

/**
 * If the node provided has the same type we are looking for then its self is returned
 * otherwise if its an expression we will iterate through the left operands of this expression
 * and all sub-expressions until the given type is found. Otherwise NULL is returned.
 * 
 * The depth specifies how deep to search, if the depth is 1 then only the left operand
 * of an expression will be checked and if its not found NULL will be returned. No deeper searching
 * will be done.
 * 
 * Passing a depth of zero is essentially the same as just checking if the given node if the type provider
 * no deeper searching will be done at all.
 */
struct node *first_node_of_type_from_left(struct node *node, int type, int depth);

/**
 * Returns true if the given node is apart of an expression
 */
bool node_in_expression(struct node* node);

bool node_is_root_expression(struct node* node);

/**
 * Returns true if the given operator is an access operator.
 * 
 * I.e a.b.c
 * 
 * a->b.k
 */
bool is_access_operator(const char *op);


/**
 * Just like is_access_operator except it checks on a node basis rather than operator
 */
bool is_access_operator_node(struct node* node);
#endif