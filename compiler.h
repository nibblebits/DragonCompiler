#ifndef COMPILER_H
#define COMPILER_H
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <memory.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "helpers/vector.h"

#define FAIL_ERR(message) assert(0 == 1 && message)

// 16 byte alignment for C programs.
#define C_STACK_ALIGNMENT 16

// Have to skip the pushed base pointer and the return address
// when accessing function arguments passed to us and looking up the stack
#define C_OFFSET_FROM_FIRST_FUNCTION_ARGUMENT 8

// Magic number for an infinite depth for iterating through expressions
// and grabbing information from operands.
#define DEPTH_INFINITE 0xffff

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
    case ',':                           \
    case '.'

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
    case ')':       \
    case ']'

struct pos
{
    int line;
    int col;
};

enum
{
    DATA_SIZE_BYTE = 1,
    DATA_SIZE_WORD = 2,
    DATA_SIZE_DWORD = 4,
    DATA_SIZE_DDWORD = 8
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
    EXPRESSION_IS_DIVISION = 0b0000000001000000,
    EXPRESSION_IN_FUNCTION_CALL = 0b0000000010000000,
    EXPRESSION_INDIRECTION = 0b0000000100000000,
    EXPRESSION_GET_ADDRESS = 0b0000001000000000,
    EXPRESSION_IS_ABOVE = 0b0000010000000000,
    EXPRESSION_IS_ABOVE_OR_EQUAL = 0b0000100000000000,
    EXPRESSION_IS_BELOW = 0b0001000000000000,
    EXPRESSION_IS_BELOW_OR_EQUAL = 0b0010000000000000,
    EXPRESSION_IS_EQUAL = 0b0100000000000000,
    EXPRESSION_IS_NOT_EQUAL = 0b1000000000000000,
    EXPRESSION_LOGICAL_AND = 0b10000000000000000,
    EXPRESSION_LOGICAL_OR = 0b100000000000000000,
    EXPRESSION_IN_LOGICAL_EXPRESSION = 0b1000000000000000000,
    EXPRESSION_IS_BITSHIFT_LEFT = 0b10000000000000000000,
    EXPRESSION_IS_BITSHIFT_RIGHT = 0b100000000000000000000,
    EXPRESSION_IS_BITWISE_OR = 0b1000000000000000000000,
    EXPRESSION_IS_BITWISE_AND = 0b10000000000000000000000,
    EXPRESSION_IS_BITWISE_XOR = 0b100000000000000000000000,

    // Signifies that this is notthe root of the expression and the root has already
    // been dealt with.
    EXPRESSION_IS_NOT_ROOT_NODE = 0b1000000000000000000000000,

};

#define EXPRESSION_UNINHERITABLE_FLAGS                                                               \
    (EXPRESSION_FLAG_RIGHT_NODE | EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS |                            \
     EXPRESSION_IS_ADDITION | EXPRESSION_IS_SUBTRACTION | EXPRESSION_IS_MULTIPLICATION |             \
     EXPRESSION_IS_DIVISION | EXPRESSION_IS_ABOVE | EXPRESSION_IS_ABOVE_OR_EQUAL |                   \
     EXPRESSION_IS_BELOW | EXPRESSION_IS_BELOW_OR_EQUAL | EXPRESSION_IS_EQUAL |                      \
     EXPRESSION_IS_NOT_EQUAL | EXPRESSION_LOGICAL_AND |                                              \
     EXPRESSION_IN_LOGICAL_EXPRESSION | EXPRESSION_IS_BITSHIFT_LEFT | EXPRESSION_IS_BITSHIFT_RIGHT | \
     EXPRESSION_IS_BITWISE_OR | EXPRESSION_IS_BITWISE_AND | EXPRESSION_IS_BITWISE_XOR)

// Flags for structure access functions.
enum
{
    /**
     * Signifies that any structure access should be calculated backwards,
     * this is useful for calculating offsets for structures on the stack
     */
    STRUCT_ACCESS_BACKWARDS = 0b00000001,
    /**
     * This bit is set for when you access "struct_offset" but do not care about
     * the offset that is returned. But you might care about the variable that gets returned
     */
    STRUCT_STOP_AT_POINTER_ACCESS = 0b00000010
};
enum
{
    STRUCT_ACCESS_DETAILS_FLAG_NOT_FINISHED = 0b00000001
};

struct struct_access_details
{
    int flags;
    /**
     * The next node you should pass to the struct_for_access function when you call it next
     */
    struct node *next_node;

    /**
     * The first node in this structure query
     */
    struct node *first_node;

    /**
     * The calculated offset to be used to access the data in memory of this structure access
     * with a memory address base of 0. To be added to stack address or global address memory
     * to locate actual location.
     */
    int offset;
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

// 4 bytes for a stack push and pop on 32 bit arch.
#define STACK_PUSH_SIZE 4
#define FUNCTION_CALL_ARGUMENTS_GET_STACK_SIZE(total_args) total_args *STACK_PUSH_SIZE
#define C_ALIGN(size) (size % C_STACK_ALIGNMENT) ? size + (C_STACK_ALIGNMENT - (size % C_STACK_ALIGNMENT)) : size
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

enum
{
    PREPROCESSOR_DEFINITION_STANDARD,
    PREPROCESSOR_DEFINITION_MACRO_FUNCTION,
    // Signifies that this preprocessor definition must call a
    // function pointer that decides what value should be returned.
    PREPROCESSOR_DEFINITION_NATIVE_CALLBACK,
    PREPROCESSOR_DEFINITION_TYPEDEF
};

struct preprocessor;
struct preprocessor_definition;
/**
 * Should evaluate the given definition into an integer
 */
typedef int (*PREPROCESSOR_DEFINITION_NATIVE_CALL_EVALUATE)(struct preprocessor_definition *definition);

/**
 * Function pointer must return a vector of struct token. That represents the value
 * of this native definition
 */
typedef struct vector *(*PREPROCESSOR_DEFINITION_NATIVE_CALL_VALUE)(struct preprocessor_definition *definition);
struct preprocessor_definition
{
    // The type of definition i.e standard or macro function
    int type;

    // The name of this definition i.e #define ABC . ABC would be the name

    const char *name;
    union
    {
        struct standard_preprocessor_definition
        {
            // A vector of definition value tokens, values can be multiple lines
            // Values can also be multiple tokens.
            // vector of "struct token"
            struct vector *value;

            // A vector of const char* representing function arguments in order.
            // i.e ABC(a, b, c) a b and c would be in this arguments vector.
            struct vector *arguments;
        } standard;

        struct typdef_preprocessor_definition
        {
            struct vector *value;
        } _typedef;

        struct native_callback_preprocessor_definition
        {
            PREPROCESSOR_DEFINITION_NATIVE_CALL_EVALUATE evaluate;
            PREPROCESSOR_DEFINITION_NATIVE_CALL_VALUE value;
        } native;
    };

    struct preprocessor *preprocessor;
};

struct preprocessor
{

    // Vector of preprocessor definitions struct preprocessor_definition*
    struct vector *definitions;

    // vector of (struct preprocessor_node*) .
    struct vector *exp_vector;

    // Used for parsing expressions.
    struct expressionable *expressionable;

    struct compile_process *compiler;
};

struct string_table_element
{
    // The string in question
    const char *str;
    // The code generator label that represents this string in memory
    const char label[50];
};

struct parsed_switch_case
{
    // The index of the case
    int index;
};

struct code_generator
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

    /**
     * We must keep track of the current code generator switch statement,
     * so that we know how to jump to the correct case that
     * was evaluated by a given expression
     */
    struct generator_switch_stmt
    {
        struct generator_switch_stmt_entity
        {
            int id;
        } current;

        // A stack/vector of struct generator_switch_stmt_entity representing the switch statements
        struct vector *switches;
    } _switch;

    // This is a bitmask of flags for registers that are in use
    // if the bit is set the register is currently being used
    // this helps the system control how to do things.
    int used_registers;

    // Vector of struct string_table_element*
    struct vector *string_table;

    // A vector/stack of struct codegen_exit_point
    // In the event of a "break" we must go to the current exit point.
    // i.e .exit_point_%i where %i is stored in this exit_points vector;
    struct vector *exit_points;

    // A vector/stack of the struct codegen_entry_point
    // In the even of a "continue" we must go to the current entry point.
    // This will allow a loop to restart.
    struct vector *entry_points;
};

enum
{
    COMPILE_PROCESS_EXPORT_AS_OBJECT = 0b00000001,
    // If this flag is set NASM will be used after compliation, to assemble
    // the file.
    COMPILE_PROCESS_EXECUTE_NASM = 0b00000010
};

/**
 * This file represents a compilation process
 */
struct compile_process
{
    // The flags in regards to how this file should be compiled
    int flags;

    // The current file being compiled
    FILE *cfile;

    // The output file to compile to. NULL if this is a sub-file included with "include"
    FILE *ofile;

    // Current line position information.
    struct pos pos;

    // The original untampered token vector
    // This contains definitions, and source code tokens
    // The preprocessor will go through this vector and populate the "token_vec"
    // after it is done.
    struct vector *token_vec_original;

    // Stack of tokens that have undergone lexcial analysis.
    // Vector of struct <struct token> individual tokens (not pointers)
    // This is the final output preprocessed tokens.
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

    struct
    {
        struct scope *root;
        struct scope *current;
    } scope;

    // The preprocessor for this compiler instance.
    struct preprocessor *preprocessor;

    // The future of "scope" The replacement.
    struct resolver_process *resolver;

    // The code generator
    struct code_generator *generator;
};

struct datatype
{
    int flags;
    // The type this is i.e float, double, long
    int type;
    // The string equivilant for this type. Does not include unsigned or signed keywords.
    // if the type is "unsigned int" here will be written only "int"
    const char *type_str;

    // The size in bytes of this datatype. Does not include array size. THis is the size
    // of the primitive type or the structure size. Arrays not included!!!
    size_t size;

    // The pointer depth of this datatype
    int pointer_depth;

    // If this is a data type of structure or union then you can access one of the modifiers here
    union
    {
        struct node *struct_node;
        struct node *union_node;
    };

    struct array
    {
        struct array_brackets *brackets;
        // This datatype size for the full array its self.
        // Calculation is datatype.size * EACH_INDEX.
        size_t size;
    } array;
};

enum
{
    // Represents this resolver scope is a stack scope not a global scope.
    // I.e stack variables are present not global variables.
    RESOLVER_SCOPE_FLAG_IS_STACK = 0b00000001
};
struct resolver_scope
{
    // Flags for the resolver scope that describe it.
    int flags;

    struct vector *entities;

    // The next scope.
    struct resolver_scope *next;
    // The previous scope.
    struct resolver_scope *prev;

    // Private data for the resolver scope.
    void *private;
};

struct compile_process;

enum
{
    RESOLVER_ENTITY_FLAG_COMPILE_TIME_ENTITY = 0b00000001,
    RESOLVER_ENTITY_FLAG_IS_STACK = 0b00000010,
    RESOLVER_ENTITY_FLAG_ARRAY_FOR_RUNTIME = 0b00000100
};

enum
{
    RESOLVER_ENTITY_TYPE_VARIABLE,
    RESOLVER_ENTITY_TYPE_FUNCTION,
    RESOLVER_ENTITY_TYPE_STRUCTURE,
    RESOLVER_ENTITY_TYPE_FUNCTION_CALL
};

struct resolver_entity
{
    int type;
    int flags;

    // The name that represents this entity in the given scope that it is in
    // If this is a variable this would be the variable name,
    // if this is a function it would be the function name
    // if this is a structure it would be the structure name.
    const char *name;

    // Can be NULL if no variable is present. otherwise equal to a node
    // that was resolved.
    struct node *node;
    union
    {
        // This data is set if the resolver entity type is a variable. RESOLVER_ENTITY_TYPE_VARIABLE type
        struct resolver_entity_var_data
        {
            struct datatype dtype;
            struct resolver_array_runtime
            {
                struct node *index_node;
                int multiplier;
            } array_runtime;

        } var_data;

        struct resolver_entity_function_call_data
        {
            // The function call arguments, this is a vector of struct node*
            // Must have a type of RESOLVER_ENTITY_FUNCTION_CALL
            struct vector *arguments;

            // The total bytes of the stack used by the function arguments
            // for this function call.
            size_t stack_size;
        } func_call_data;
    };
    // The scope that this entity belongs too.
    struct resolver_scope *scope;

    // The result this entity is apart of, NULL if no result is binded
    struct resolver_result *result;

    // The resolver process for this entity
    struct resolver_process *resolver;

    // Private data that can be stored by the creator of the resolver entity
    void *private;

    // The next entity in the list
    struct resolver_entity *next;
    // The previous entity in the list.
    struct resolver_entity *prev;
};

enum
{
    // This flag is set if the new structure entity should offset
    // from the last entities base address.
    NEW_STRUCT_ENTITY_FLAG_LAST_ENTITY_USE_BASE = 0b00000001
};

/**
 * Resolver handler for new struct entities. The function must return the private data
 * to be set in the resolver_entity
 */
typedef void *(*RESOLVER_NEW_STRUCT_ENTITY)(struct resolver_result *result, struct node *var_node, int offset, struct resolver_scope *scope);

/**
 * Used to merge two struct entities, generally their offsets for example. Receiver should merge them
 * so that they become one.
 * 
 * Should return private data for the merged entities
 */
typedef void *(*RESOLVER_MERGE_STRUCT_ENTITY)(struct resolver_result *result, struct resolver_entity *left_entity, struct resolver_entity *right_entity, struct resolver_scope *scope);

/**
 * THis function pointer is called when we have an array expression processed
 * by the resolver.
 * 
 * \param array_var_entity the Variable entity representing the array access. I.e a[5] this would be "a"
*  \param index_val numerical value passed to this index.
 * \param index The numerical index in the array we are computing for, we need an array private that represents all you need for this given index
 */
typedef void *(*RESOLVER_NEW_ARRAY_ENTITY)(struct resolver_result *result, struct resolver_entity *array_var_entity, int index_val, int index);

/**
 * Used for when you need to join a calculated result with a previous entity.
 * Rather than creating a new entity like RESOLVER_NEW_ARRAY_ENTITY we join the result with the previous one
 */
typedef void (*RESOLVER_JOIN_ARRAY_ENTITY_WITH_NEW_INDEX)(struct resolver_result *result, struct resolver_entity *join_entity, int index_val, int index);

/**
 * User must delete the resolver scope private data. DO not delete the "scope" pointer!
 */
typedef void (*RESOLVER_DELETE_SCOPE)(struct resolver_scope *scope);

/**
 * User must delete the resolver entity private data. DO not delete the "entity pointer!!!"
 */
typedef void (*RESOLVER_DELETE_ENTITY)(struct resolver_entity *entity);

struct resolver_callbacks
{
    // Must not be NULL!
    // This function pointer is called when their is access to a new structure expression
    // in the resolver. The function in question is required to return a new entity representing
    // that structure variable.
    RESOLVER_NEW_STRUCT_ENTITY new_struct_entity;
    RESOLVER_MERGE_STRUCT_ENTITY merge_struct_entity;

    /**
     * Called when we need to create a new resolver_entity for the given array expression
     */
    RESOLVER_NEW_ARRAY_ENTITY new_array_entity;
    RESOLVER_JOIN_ARRAY_ENTITY_WITH_NEW_INDEX join_array_entity_index;

    RESOLVER_DELETE_SCOPE delete_scope;
    RESOLVER_DELETE_ENTITY delete_entity;
};

struct resolver_process
{
    struct resolver_scopes
    {
        struct resolver_scope *root;
        struct resolver_scope *current;
    } scope;

    struct compile_process *compiler;

    struct resolver_callbacks callbacks;
};

enum
{
    // This bit is set if the result failed for some reason.
    // We was not able to locate the variable in question
    // at some point during the resolving we hit a variable that didnt exist
    // or had an error ...
    RESOLVER_RESULT_FLAG_FAILED = 0b00000001,
    // THis bit is set if the full address has been computed
    // no runtime extras need to be done...
    RESOLVER_RESULT_FLAG_RUNTIME_NEEDED_TO_FINISH_PATH = 0b00000010,

    // True if we are currently resolving an array in this part of the expression
    RESOLVER_RESULT_FLAG_PROCESSING_ARRAY_ENTITIES = 0b00000100
};

/**
 * Temporary structure to help guide array data.
 */
struct resolver_array_data
{
    // Holds nodes of type resolver_entity, representing array entities.
    // That are currently being processed.
    // Used to help guide the algorithm in calculating static offset.
    struct vector *array_entities;
};

struct resolver_result
{
    // The first entity of this resolver result, it is never destroyed or removed
    // even when popping from the result. Use this as a base to know how the expression started
    struct resolver_entity *first_entity_const;

    // This variable represents the variable of the start of this expression
    struct resolver_entity *identifier;

    // Equal to the last structure entity discovered.
    struct resolver_entity *last_struct_entity;

    struct resolver_array_data array_data;

    // The root entity of this result
    struct resolver_entity *entity;
    // The last processed entity in the list
    struct resolver_entity *last_entity;
    int flags;
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
    TOKEN_TYPE_NEWLINE
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

    // True if their is a whitespace between the token and the next token
    // i.e * a for token * whitespace would be true as the token "a" has a space
    // between this token
    bool whitespace;
};

struct sizeable_node
{
    size_t size;
    struct node *node;
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
    DATATYPE_FLAG_IS_SIGNED = 0b00000001,
    DATATYPE_FLAG_IS_STATIC = 0b00000010,
    DATATYPE_FLAG_IS_CONST = 0b00000100,
    DATATYPE_FLAG_IS_POINTER = 0b00001000,
    DATATYPE_FLAG_IS_ARRAY = 0b00010000
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

struct array_brackets
{
    // Vector of node brackets.
    struct vector *n_brackets;
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
    NODE_TYPE_STRING,
    NODE_TYPE_VARIABLE,
    NODE_TYPE_FUNCTION,
    NODE_TYPE_BODY,
    NODE_TYPE_STATEMENT_RETURN,
    NODE_TYPE_STATEMENT_IF,    // Used for both IF and ELSE IF statements
    NODE_TYPE_STATEMENT_ELSE,  // This is an ELSE statement of an IF
    NODE_TYPE_STATEMENT_WHILE, // While statements i.e while(1) { }
    NODE_TYPE_STATEMENT_DO_WHILE,
    NODE_TYPE_STATEMENT_FOR,
    NODE_TYPE_STATEMENT_BREAK,
    NODE_TYPE_STATEMENT_CONTINUE,
    NODE_TYPE_STATEMENT_SWITCH,
    NODE_TYPE_STATEMENT_CASE,
    NODE_TYPE_STATEMENT_DEFAULT,
    NODE_TYPE_STATEMENT_GOTO,

    // A label node i.e "testing:"
    NODE_TYPE_LABEL,
    NODE_TYPE_UNARY,
    NODE_TYPE_STRUCT,
    NODE_TYPE_BRACKET // Array brackets i.e [50][20] Two node brackets.
};

enum
{
    // Represents that this node is part of an expressions left or right operands.
    // For example the node that represents decimal 50 would not have this flag set test(50)
    // Since it is the only function argument. However in this case test(50*a) the number node
    // of "50" would have this flag set as its apart of an expression.
    NODE_FLAG_INSIDE_EXPRESSION = 0b00000001,
    // This flag is set if this node is a cloned node
    // cloned nodes can be modified freely and modifying a cloned node
    // does not affect the original node.
    // As cloned nodes do not guarantee to be apart of the tree, it is important the cloner
    // be responsible for the memory.
    NODE_FLAG_CLONED = 0b00000010
};

struct node
{
    int type;
    // Generic flags for the given node
    int flags;

    // What is this node binded too?
    struct node_binded
    {
        // The body node this node is apart of. If theirs no body then its NULL.
        // NULL means no scope is surrounding this node, it must be in the global space
        struct node *owner;

        // The function that this node is apart of
        struct node* function;
    } binded;

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
            struct node *exp;
        } parenthesis;

        // Represents unary expressions i.e "~abc", "-a", "-90"
        struct unary
        {
            // In the case of indirection the "op" variable will only equal to one "*"
            // you can find the depth in the indirection structure within.
            const char *op;
            struct node *operand;

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
            const char *name;
            struct node *body_n;
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

            // The size of the function stack size (unaligned)
            // Essentally this is the sum of the size of all variables inside the body of the given function
            // regardless if the variables are deep in a scope they are included.
            size_t stack_size;
        } func;

        struct body
        {
            // Body nodes have a vector of nodes that represent statements
            // vector<struct node*>
            struct vector *statements;

            // The size of the combined variables in this body.
            // Useful for accessing the bodies of structures and unions
            // where you need to know the size.
            size_t size;

            // True if the variable size had to be increased due to padding in the body
            bool padded;

            // Equal to the largest variable declaration node in the given body
            // NULL if no variable node exists within the body.
            struct node *largest_var_node;

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
                    long lnum;
                    long long llnum;
                    unsigned long ulnum;
                    unsigned long long ullnum;
                };
            } const_val;

            // Aligned offset taking the "padding" into account
            int aoffset;

            // The total bytes before this offset that will be NULL's to allow alignment
            int padding;
            // The total bytes after this variable will be NULL's to allow alignment
            int padding_after;
        } var;

        // Array brackets i.e [50]
        struct bracket
        {
            // The inner expression for the given bracket.
            // i.e [50] inner would be NODE_TYPE_NUMBER that contains a node that represents number 50
            struct node *inner;
        } bracket;

        union statement
        {
            // Represents a return statement.
            struct return_stmt
            {
                // The expresion of the return statement.
                struct node *exp;
            } ret;

            struct if_stmt
            {
                // Condition node i.e if(50*20 > 40)
                struct node *cond_node;

                // Body node i.e { printf("test"); }
                struct node *body_node;
                // The next part of the IF statement i.e the "else" or "else if"
                // NULL If theris nothing more.
                struct node *next;

            } _if;

            struct else_stmt
            {

                // Body node i.e { printf("test"); }
                struct node *body_node;
            } _else;

            // While statements. i.e while(1) {}
            struct while_stmt
            {
                struct node *cond;
                struct node *body;
            } _while;

            struct do_while_stmt
            {
                struct node *body;
                struct node *cond;
            } _do_while;

            struct for_stmt
            {
                struct node *init;
                struct node *cond;
                struct node *loop;
                struct node *body;
            } _for;

            struct _switch_stmt
            {
                struct node *exp;
                struct node *body;
                struct vector *cases;
                bool has_default_case;
            } _switch;

            struct _case_stmt
            {
                struct node *exp;
            } _case;

            struct _goto_stmt
            {
                // The label node for the goto statement
                // i.e goto myspeciallabel
                struct node *label;
            } _goto;

        } stmt;

        struct label
        {
            struct node *name;
        } label;
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
int compile_file(const char *filename, const char *out_filename, int flags);

/**
 * Includes a file to be compiled, returns a new compile process that represents the file
 * to be compiled.
 * 
 * Only lexical analysis, preprocessing and parsing are done for the given file.
 * Code generation is excluded.
 */
struct compile_process *compile_include(const char *filename, struct compile_process *parent_process);

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
struct compile_process *compile_process_create(const char *filename, const char *out_filename, int flags, struct compile_process *parent_process);

/**
 * Destroys the compiler process
 */
void compile_process_destroy(struct compile_process *process);

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

struct scope *scope_alloc();
struct scope *scope_create_root(struct compile_process *process);
struct scope *scope_new(struct compile_process *process, int flags);
void scope_finish(struct compile_process *process);

struct scope *scope_create_root(struct compile_process *process);
void scope_free_root(struct compile_process *process);

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
struct scope *scope_current(struct compile_process *process);

/**
 * Registers a symbol to the symbol table
 */
struct symbol *symresolver_register_symbol(struct compile_process *process, const char *sym_name, int type, void *data);
/**
 * Gets the registered symbol from the symbol table for the given name
 */
struct symbol *symresolver_get_symbol(struct compile_process *process, const char *name);

/**
 * Gets the node from the symbol data
 */
struct node *symresolver_node(struct symbol *sym);

/**
 * Builds a symbol for the given node.
 * Note: The node must be fully parsed and initialized
 */
void symresolver_build_for_node(struct compile_process *process, struct node *node);

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
 * \param last_pos The last offset for the structure offset. I.e the current stack offset.
 * \param flags The flags for this operation
 */
int struct_offset(struct compile_process *compile_proc, const char *struct_name, const char *var_name, struct node **var_node_out, int last_pos, int flags);

/**
 * Returns the node for the structure access expression.
 * 
 * For example if you had the structure "test" and "abc"
 * struct abc
 * {
 *    int z;
 * }
 *
 * struct test
 * {
 *   struct abc a;
 * }
 * 
 * and your expression node was "a.z" and your type_str was "test" you would have 
 * the node for variable "z" returned.
 * 
 * Likewise if only "a" was provided then the "a" variable node in the test structure would be returned.
 */
struct node *struct_for_access(struct resolver_process *process, struct node *node, const char *type_str, int flags, struct struct_access_details *details_out);
/**
 * Finds the first node of the given type.
 * 
 * For example lets imagine the expression "a.b.e.f"
 * if you called this function with NODE_TYPE_IDENTIFIER as the type and you passed in 
 * the right operand of the expression i.e a.E then you would find that the node of "b"
 * would be returned
 */
struct node *first_node_of_type(struct node *node, int type);

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
bool node_in_expression(struct node *node);

bool node_is_root_expression(struct node *node);

/**
 * Returns the entire stack size for the given function, including all sub scopes
 */
size_t function_node_stack_size(struct node* node);

/**
 * Returns true if the given operator is an access operator.
 * 
 * I.e a.b.c
 * 
 * a->b.k
 */
bool is_access_operator(const char *op);

bool is_array_node(struct node *node);

bool is_parentheses_node(struct node *node);

/**
 * Returns true if this node represents an access node expression
 */
bool is_access_node(struct node *node);

/**
 * Returns true if this is an access node and it uses the given operator
 */
bool is_access_node_with_op(struct node *node, const char *op);

/**
 * Just like is_access_operator except it checks on a node basis rather than operator
 */
bool is_access_operator_node(struct node *node);

/**
 * Returns true if this token represents an operator
 */
bool is_operator_token(struct token *token);

bool op_is_indirection(const char *op);

int align_value(int val, int to);

/**
 * Aligns the given value and if its a negative value then it pretends its positive
 * aligns it and then returns the negative result
 */
int align_value_treat_positive(int val, int to);

/**
 * Aligns the offset for the provided offset for the given variable
 */
void variable_align_offset(struct node *var_node, int *stack_offset_out);

/**
 * Sums all the padding for a vector of variable nodes.
 * Non variable nodes are ignored
 */
int compute_sum_padding(struct vector *vec);
/**
 * Sums all the padding for a body of variable nodes, non variable nodes are ignored
 */
int compute_sum_padding_for_body(struct node *node);

/**
 * Calculates padding for the given value. Pads to "to"
 */
int padding(int val, int to);

/**
 * Returns true if the given variable node is a primative variable
 */
bool variable_node_is_primative(struct node *node);

/**
 * Returns the stack size for the node and all parent body nodes.
 * Accept any node. Will iterate through every body until full scope size
 * is summed.
 */
size_t node_sum_scope_size(struct node *node);

struct node *node_from_symbol(struct compile_process *current_process, const char *name);
struct node *node_from_sym(struct symbol *sym);
struct node *struct_node_for_name(struct compile_process *current_process, const char *struct_name);

/**
 * Returns a node that has the given structure for the provided var_node.
 * The var_node must be of type NODE_TYPE_VARIABLE. It must also represent a structure variable type
 * 
 * i.e struct abc test;
 * 
 * Calling this function would return the structure node that represents struct "abc" giving
 * accessing to its entire body and attributes.
 */
struct node *variable_struct_node(struct node *var_node);

/**
 * Returns true if the given node is a variable node that is a structure variable and
 * that structures memory is padded. Otherwise false.
 */
bool variable_struct_padded(struct node *var_node);

/**
 * Returns the largest variable node for the given structure, otherwise NULL.
 */
struct node *variable_struct_largest_variable_node(struct node *var_node);

/**
 * Returns the largest variable declaration inside the provided body.
 * Largest being the variable that takes up the most room. Primitive variables only.
 */
struct node *body_largest_variable_node(struct node *body_node);

/**
 * Array
 * 
 */

struct array_brackets *array_brackets_new();
void array_brackets_free(struct array_brackets *brackets);
void array_brackets_add(struct array_brackets *brackets, struct node *bracket_node);
size_t array_brackets_calculate_size(struct datatype *type, struct array_brackets *brackets);

/**
 * Returns the full variable size for the given var_node. The full size in memory
 * if this is an array variable it will return the array size for the given data type.
 * 
 * I.e short abc[8]; will return 8*2;
 */
size_t variable_size(struct node *var_node);
size_t datatype_size(struct datatype *datatype);

bool is_array_operator(const char *op);
bool is_argument_operator(const char *op);
/**
 * Is the given node an expression whose operator is ","
 */
bool is_argument_node(struct node *node);

/**
 * Returns true if this operator is special.
 * Special operators require instructions that could break the flow of registers
 * examples include multiplication and division.
 * 
 * Special operators need special treatment, they may need to use additioanl registers,
 * unable to rely on just one, Or they may need multiple instructions to compute them correctly
 */
bool is_special_operator(const char *op);
bool is_special_node(struct node *node);

bool is_bitwise_operator(const char *op);

/**
 * Returns true if the given operator is a logical comparision operator
 */
bool is_logical_operator(const char *op);

bool is_logical_node(struct node *node);

/**
 * Returns true if the address can be caclulated at compile time
 */
bool is_compile_computable(struct node *node);

/**
 * Returns true if the given character is one of the provided delims
 * \param c The character to check
 * \param delims A const char string of many delimieters. One byte per delimieter
 */
bool char_is_delim(char c, const char *delims);
/**
 * Computes the array offset for the given expression node or array bracket node.
 * 
 * \param node The node to perform the computation on
 * \param single_element_size THe size of the type of the array. I.e an array of ints would be "4"
 */

int compute_array_offset(struct node *node, size_t single_element_size);
int array_offset(struct datatype *dtype, int index, int index_value);

/**
 * Returns the multipler for a given array index. How much you need to multiply by
 * to get to the offset part.
 */
int array_multiplier(struct datatype *dtype, int index, int index_value);
// Resolver functions

struct resolver_result *resolver_new_result(struct resolver_process *process);
void resolver_result_free(struct resolver_result *result);
bool resolver_result_failed(struct resolver_result *result);
bool resolver_result_ok(struct resolver_result *result);

/**
 * Returns true if this entity has an array multiplier that must
 * be computed at runtime.
 */
bool resolver_entity_has_array_multiplier(struct resolver_entity *entity);

/**
 * Returns true if the resolver entity requires additional
 * runtime code to compute real offset.
 * False if this entity can be computed at compile time
 */
bool resolver_entity_runtime_required(struct resolver_entity *entity);

/**
 * Returns true if the resolver result is completed and no more processing
 * has to be done to find a working path
 */
bool resolver_result_finished(struct resolver_result *result);

struct resolver_entity *resolver_result_entity(struct resolver_result *result);
struct compile_process *resolver_compiler(struct resolver_process *process);
struct resolver_scope *resolver_new_scope(struct resolver_process *resolver, void *private, int flags);
void resolver_finish_scope(struct resolver_process *resolver);
struct resolver_process *resolver_new_process(struct compile_process *compiler, struct resolver_callbacks *callbacks);
struct resolver_entity *resolver_new_entity_for_var_node(struct resolver_process *process, struct node *var_node, void *private);
struct resolver_entity *resolver_register_function(struct resolver_process *process, struct node *func_node, void *private);
struct resolver_entity *resolver_get_variable_in_scope(const char *var_name, struct resolver_scope *scope);
struct resolver_entity *resolver_get_variable(struct resolver_result *result, struct resolver_process *resolver, const char *var_name);
struct resolver_result *resolver_follow(struct resolver_process *resolver, struct node *node);
struct resolver_entity *resolver_result_entity_root(struct resolver_result *result);
struct resolver_entity *resolver_result_entity_next(struct resolver_entity *entity);

/**
 * Attempts to peek through the tree at the given node and looks for a datatype
 * that can be associated with the node entity.
 * 
 * For example if you had an array and you did array[50].
 * 
 * If you passed the array node here the datatype of "array" would be returned.
 * 
 * If you call this function on a function call then the return type of the function call will be returned
 * the deepest possible type will be returned.
 */
struct datatype *resolver_get_datatype(struct node *node);

// Node

struct node *node_clone(struct node *node);
const char *node_var_type_str(struct node *var_node);
const char *node_var_name(struct node *var_node);

/**
 * Returns true if this is a variable node and it is a pointer, or returns true
 * if this is a function node that returns a pointer. Otherwise false
 */
bool is_pointer_node(struct node *node);
// Token

bool token_is_operator(struct token *token, const char *op);
bool token_is_keyword(struct token *token, const char *keyword);
bool token_is_symbol(struct token *token, char sym);
bool token_is_identifier(struct token *token, const char *iden);

// Preprocessor
int preprocessor_run(struct compile_process *compiler);
struct vector *preprocessor_build_value_vector_for_integer(int value);
struct preprocessor_definition *preprocessor_definition_create_native(const char *name, PREPROCESSOR_DEFINITION_NATIVE_CALL_EVALUATE evaluate, PREPROCESSOR_DEFINITION_NATIVE_CALL_VALUE value, struct preprocessor *preprocessor);
struct token *preprocessor_previous_token(struct compile_process *compiler);
struct token *preprocessor_next_token(struct compile_process *compiler);
int preprocessor_line_macro_evaluate(struct preprocessor_definition *definition);
struct vector *preprocessor_line_macro_value(struct preprocessor_definition *definition);
/**
 * This creates the definitions for the preprocessor that should always exist
 */
void preprocessor_create_definitions(struct preprocessor *preprocessor);

/**
 * Creates a new preprocessor instance
 */
struct preprocessor *preprocessor_create(struct compile_process *compiler);

// Expressionable system, parses expressions

enum
{
    EXPRESSIONABLE_GENERIC_TYPE_NUMBER,
    EXPRESSIONABLE_GENERIC_TYPE_IDENTIFIER,
    EXPRESSIONABLE_GENERIC_TYPE_UNARY,
    EXPRESSIONABLE_GENERIC_TYPE_PARENTHESES,
    EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION,
    EXPRESSIONABLE_GENERIC_TYPE_NON_GENERIC
};

// Temporary
#define expressionable_parse_err(str, tmp) \
    FAIL_ERR(str)

struct expressionable;

typedef void *(*EXPRESSIONABLE_HANDLE_NUMBER)(struct expressionable *expressionable);
typedef void *(*EXPRESSIONABLE_HANDLE_IDENTIFIER)(struct expressionable *expressionable);
typedef void (*EXPRESSIONABLE_MAKE_EXPRESSION_NODE)(struct expressionable *expressionable, void *left_node_ptr, void *right_node_ptr, const char *op);
typedef void (*EXPRESSIONABLE_MAKE_PARENTHESES_NODE)(struct expressionable *expressionable, void *node_ptr);
typedef void *(*EXPRESSIONABLE_MAKE_UNARY_NODE)(struct expressionable *expressionable, const char *op, void *right_operand_node_ptr);

typedef int (*EXPRESSIONABLE_GET_NODE_TYPE)(struct expressionable *expressionable, void *node);
typedef void *(*EXPRESSIONABLE_GET_LEFT_NODE)(struct expressionable *expressionable, void *target_node);
typedef void *(*EXPRESSIONABLE_GET_RIGHT_NODE)(struct expressionable *expressionable, void *target_node);
typedef const char *(*EXPRESSIONABLE_GET_NODE_OPERATOR)(struct expressionable *expressionable, void *target_node);
typedef void **(*EXPRESSIONABLE_GET_NODE_ADDRESS)(struct expressionable *expressionable, void *target_node);
typedef void (*EXPPRESIONABLE_SET_EXPRESSION_NODE)(struct expressionable *expressionable, void *node, void *left_node, void *right_node, const char *op);
struct expressionable_config
{
    struct expressionable_callbacks
    {

        EXPRESSIONABLE_HANDLE_NUMBER handle_number_callback;
        EXPRESSIONABLE_HANDLE_IDENTIFIER handle_identifier_callback;

        /**
         * Should create an expressionable node and push it to the stack
         */
        EXPRESSIONABLE_MAKE_EXPRESSION_NODE make_expression_node;
        EXPRESSIONABLE_MAKE_PARENTHESES_NODE make_parentheses_node;
        EXPRESSIONABLE_MAKE_UNARY_NODE make_unary_node;

        /**
         * Function must return the numeric type of this node, as specified in the generic
         * types enumumeration
         */
        EXPRESSIONABLE_GET_NODE_TYPE get_node_type;

        EXPRESSIONABLE_GET_LEFT_NODE get_left_node;
        EXPRESSIONABLE_GET_RIGHT_NODE get_right_node;
        EXPRESSIONABLE_GET_NODE_OPERATOR get_node_operator;
        EXPRESSIONABLE_GET_NODE_ADDRESS get_left_node_address;
        EXPRESSIONABLE_GET_NODE_ADDRESS get_right_node_address;
        EXPPRESIONABLE_SET_EXPRESSION_NODE set_exp_node;

    } callbacks;
};

struct expressionable
{
    struct expressionable_config config;
    struct vector *token_vec;
    struct vector *node_vec_out;
};

struct expressionable *expressionable_create(struct expressionable_config *config, struct vector *token_vector, struct vector *node_vector);
void expressionable_parse(struct expressionable *expressionable);
struct token *expressionable_token_next(struct expressionable *expressionable);
void *expressionable_node_pop(struct expressionable *expressionable);
void expressionable_node_push(struct expressionable *expressionable, void *node_ptr);

/**
 * Resolver default configuration
 */

/**
 * This file is a default configuration for the resolver, this is an abstraction
 * that can be used by every part of this compiler
 */

enum
{
    RESOLVER_DEFAULT_ENTITY_TYPE_STACK,
    RESOLVER_DEFAULT_ENTITY_TYPE_SYMBOL
};

// Resolver scope flags
enum
{
    RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK = 0b00000001
};

enum
{
    RESOLVER_DEFAULT_ENTITY_DATA_TYPE_VARIABLE,
    RESOLVER_DEFAULT_ENTITY_DATA_TYPE_FUNCTION,
};

struct resolver_default_entity_data
{
    // The type of entity data this is, i.e variable, function, structure
    int type;

    // The address that can be addressed in assembly. I.e [ebp-4] [name]
    char address[60];

    // The base address excluding any offset applied to it
    // I.e "ebp" for all stack variables.
    // I.e "variable_name" for global variables.
    char base_address[60];

    // The numeric offset that we must use. This is the numeric offset
    // that is applied to "address" if any.
    int offset;
    int flags;
};

struct resolver_default_scope_data
{
    int flags;
};

struct resolver_default_entity_data *resolver_default_new_entity_data();
struct resolver_default_entity_data *resolver_default_new_entity_data_for_var_node(struct node *var_node, int offset, int flags);
struct resolver_default_entity_data *resolver_default_new_entity_data_for_function(struct node *func_node, int flags);
struct resolver_entity *resolver_default_new_scope_entity(struct resolver_process *resolver, struct node *var_node, int offset, int flags);
struct resolver_entity *resolver_default_register_function(struct resolver_process *resolver, struct node *func_node, int flags);
void resolver_default_new_scope(struct resolver_process *resolver, int flags);
void resolver_default_finish_scope(struct resolver_process *resolver);
struct resolver_process *resolver_default_new_process(struct compile_process *compiler);
struct resolver_default_entity_data *resolver_default_entity_private(struct resolver_entity *entity);
struct resolver_default_scope_data *resolver_default_scope_private(struct resolver_scope *scope);

// Code generator
struct code_generator *codegenerator_new(struct compile_process *process);
#endif