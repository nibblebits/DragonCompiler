#include "compiler.h"
#include "helpers/buffer.h"
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>

static struct compile_process *current_process;

// Returned when we have no expression state.
static struct expression_state blank_state = {};

#define codegen_err(...) \
    compiler_error(current_process, __VA_ARGS__)


/**
 * Specifies current history for the given operation/expression
 */
struct history
{
    // Flags for this history.
    int flags;

    // This union has shared memory, rely on the flags to ensure
    // the memory you are accessing is what your looking for.
    union
    {
        // History for the current function call (if any)
        struct function_call
        {
            struct arguments
            {
                int total_args;
                // Stack size for the given function arguments. i.e test(50, 40, 30, 20)
                // would be 4*4 =16 bytes
                size_t size;
            } arguments;
        } function_call;
    };
};

static struct history *history_down(struct history *history, int flags)
{
    history->flags = flags;
    return history;
}

static struct history *history_begin(struct history *history_out, int flags)
{
    memset(history_out, 0, sizeof(struct history));
    history_out->flags = flags;
    return history_out;
}

enum
{
    CODEGEN_ENTITY_TYPE_STACK,
    CODEGEN_ENTITY_TYPE_SYMBOL
};

// Codegen scope flags
enum
{
    CODEGEN_SCOPE_FLAG_IS_LOCAL_STACK = 0b00000001
};

struct codegen_scope_data
{
    int flags;
};

// Codegen entity flags
enum
{
    CODEGEN_ENTITY_FLAG_IS_LOCAL_STACK = 0b00000001
};

struct codegen_entity_data
{
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

struct codegen_entity_data *codegen_entity_private(struct resolver_entity *entity)
{
    return entity->private;
}

struct codegen_scope_data *codegen_scope_private(struct resolver_scope *scope)
{
    return scope->private;
}

size_t codegen_align(size_t size)
{
    if (size % C_STACK_ALIGNMENT)
        size += C_STACK_ALIGNMENT - (size % C_STACK_ALIGNMENT);

    return size;
}

/**
 * Aligns the size to a dword so long as the size is above 4 bytes
 */
size_t codegen_align_to_dword(size_t size)
{
    if (size < DATA_SIZE_DWORD)
        return size;

    if (size % DATA_SIZE_DWORD)
    {
        size += DATA_SIZE_DWORD - (size % DATA_SIZE_DWORD);
    }

    return size;
}

static int codegen_remove_uninheritable_flags(int flags)
{
    return flags & ~EXPRESSION_UNINHERITABLE_FLAGS;
}

char *codegen_stack_asm_address(int stack_offset, char *out)
{
    if (stack_offset < 0)
    {
        sprintf(out, "ebp%i", stack_offset);
        return out;
    }

    sprintf(out, "ebp+%i", stack_offset);
    return out;
}

void codegen_global_asm_address(struct node *var_node, int offset, char *address_out)
{
    if (offset == 0)
    {
        sprintf(address_out, "%s", var_node->var.name);
        return;
    }

    sprintf(address_out, "%s+%i", var_node->var.name, offset);
}

struct codegen_entity_data *codegen_new_entity_data(struct node *var_node, int offset, int flags)
{
    struct codegen_entity_data *entity_data = calloc(sizeof(struct codegen_entity_data), 1);
    entity_data->offset = offset;
    entity_data->flags = flags;

    if (flags & CODEGEN_ENTITY_FLAG_IS_LOCAL_STACK)
    {
        codegen_stack_asm_address(offset, entity_data->address);
        sprintf(entity_data->base_address, "ebp");
    }
    else
    {
        codegen_global_asm_address(var_node, offset, entity_data->address);
        sprintf(entity_data->base_address, "%s", var_node->var.name);
    }

    return entity_data;
}

void codegen_entity_build_address_from_base(struct codegen_entity_data *data_out, struct codegen_entity_data *data_second)
{
    memset(data_out->address, 0x00, sizeof(data_out->address));
    sprintf(data_out->address, "%s+%i", data_out->base_address, data_out->offset + data_second->offset);
}

struct resolver_entity *codegen_new_scope_entity(struct node *var_node, int offset, int flags)
{
    struct codegen_entity_data *entity_data = codegen_new_entity_data(var_node, offset, flags);
    return resolver_new_entity_for_var_node(current_process->resolver, var_node, entity_data);
}

void codegen_new_scope(int flags)
{
    struct codegen_scope_data *scope_data = calloc(sizeof(struct codegen_scope_data), 1);
    scope_data->flags |= flags;
    resolver_new_scope(current_process->resolver, scope_data);
}

void codegen_finish_scope()
{
    resolver_finish_scope(current_process->resolver);
}

int codegen_get_enum_for_register(const char *reg)
{
    int _enum = -1;
    if (S_EQ(reg, "eax"))
    {
        _enum = REGISTER_EAX_IS_USED;
    }
    else if (S_EQ(reg, "ebx"))
    {
        _enum = REGISTER_EBX_IS_USED;
    }
    else if (S_EQ(reg, "ecx"))
    {
        _enum = REGISTER_ECX_IS_USED;
    }
    else if (S_EQ(reg, "edx"))
    {
        _enum = REGISTER_EDX_IS_USED;
    }

    return _enum;
}

static bool register_is_used(const char *reg)
{
    return current_process->generator.used_registers & codegen_get_enum_for_register(reg);
}

void register_set_flag(int flag)
{
    current_process->generator.used_registers |= flag;
}

void register_unset_flag(int flag)
{
    current_process->generator.used_registers &= ~flag;
}

void codegen_use_register(const char *reg)
{
    register_set_flag(codegen_get_enum_for_register(reg));
}

void codegen_release_register(const char *reg)
{
    register_unset_flag(codegen_get_enum_for_register(reg));
}

void asm_push(const char *ins, ...)
{
    va_list args;
    va_start(args, ins);
    vfprintf(stdout, ins, args);
    fprintf(stdout, "\n");
    va_end(args);
}

void codegen_stack_sub(size_t stack_size)
{
    if (stack_size != 0)
    {
        asm_push("sub esp, %lld", stack_size);
    }
}

void codegen_stack_add(size_t stack_size)
{
    if (stack_size != 0)
    {
        asm_push("add esp, %lld", stack_size);
    }
}

/**
 * Fills tmp_buf with the assembly keyword for the given size.
 * Returns tmp_buf
 */
static const char *asm_keyword_for_size(size_t size, char *tmp_buf)
{
    const char *keyword = NULL;
    switch (size)
    {
    case DATA_SIZE_BYTE:
        keyword = "db";
        break;
    case DATA_SIZE_WORD:
        keyword = "dw";
        break;
    case DATA_SIZE_DWORD:
        keyword = "dd";
        break;
    case DATA_SIZE_DDWORD:
        keyword = "dq";
        break;

    default:
        // We have a structure or unknown type? Then lets just reserve enough bytes.
        sprintf(tmp_buf, "times %lld db 0", (unsigned long long)size);
        return tmp_buf;
    }

    stpcpy(tmp_buf, keyword);
    return tmp_buf;
}

static struct node *node_next()
{
    return vector_peek_ptr(current_process->node_tree_vec);
}

void codegen_generate_node(struct node *node);
void codegen_generate_expressionable(struct node *node, struct history *history);
void codegen_generate_new_expressionable(struct node *node, struct history *history)
{
    codegen_generate_expressionable(node, history);
}

// Rename this function... terrible name
// Return result should be used immedeitly and not stored
// copy only! Temp result!
static const char *codegen_get_fmt_for_value(struct node *value_node, struct resolver_entity *entity)
{
    static char tmp_buf[256];
    if (value_node->type == NODE_TYPE_NUMBER)
    {
        sprintf(tmp_buf, "%lld", value_node->llnum);
        return tmp_buf;
    }

    // SO we are an identifier, in this case we also have an address.
    // The entity address is all we care about.
    sprintf(tmp_buf, "[%s]", codegen_entity_private(entity)->address);
    return tmp_buf;
}

static void codegen_gen_math(const char *reg, struct node *value_node, int flags, struct resolver_entity *entity)
{
    if (flags & EXPRESSION_IS_ADDITION)
    {
        asm_push("add %s, %s", reg, codegen_get_fmt_for_value(value_node, entity));
    }
    else if (flags & EXPRESSION_IS_SUBTRACTION)
    {
        asm_push("sub %s, %s", reg, codegen_get_fmt_for_value(value_node, entity));
    }
    else if (flags & EXPRESSION_IS_MULTIPLICATION)
    {
        codegen_use_register("ecx");
        asm_push("mov ecx, %s", codegen_get_fmt_for_value(value_node, entity));

        // Need a way to know if its signed in the future.. Assumed all signed for now.
        asm_push("imul ecx");
    }
    else if (flags & EXPRESSIPON_IS_DIVISION)
    {
        codegen_use_register("ecx");
        asm_push("mov ecx, %s", codegen_get_fmt_for_value(value_node, entity));
        // Assuming signed, check for unsigned in the future, check for float in the future..
        asm_push("idiv ecx");
    }
}

static void codegen_gen_mov_or_math(const char *reg, struct node *value_node, int flags, struct resolver_entity *entity)
{
    if (register_is_used(reg))
    {
        codegen_gen_math(reg, value_node, flags, entity);
        return;
    }

    codegen_use_register(reg);
    asm_push("mov %s, %s", reg, codegen_get_fmt_for_value(value_node, entity));
}
/**
 * For literal numbers
 */
void codegen_generate_number_node(struct node *node, struct history *history)
{
    // If this is a function call argument then we can just push the result straight to the stack
    if (history->flags & EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS)
    {
        // This node represents a function call argument
        // let's just push the number
        asm_push("push %lld", node->llnum);

        // Stack would be changed by four bytes with this push
        // Perhaps better way to do this.. function of some kind..
        history->function_call.arguments.size += 4;

        return;
    }

    const char *reg_to_use = "eax";
    codegen_gen_mov_or_math(reg_to_use, node, history->flags, 0);
}

/**
 * Hopefully we can find a better way of doing this.
 * Currently its the best ive got, to check for mul_or_div for certain operations
 */
static bool is_node_mul_or_div(struct node *node)
{
    return S_EQ(node->exp.op, "*") || S_EQ(node->exp.op, "/");
}

static bool is_node_assignment(struct node *node)
{
    return S_EQ(node->exp.op, "=") ||
           S_EQ(node->exp.op, "+=") ||
           S_EQ(node->exp.op, "-=") ||
           S_EQ(node->exp.op, "*=") ||
           S_EQ(node->exp.op, "/=");
}

static bool is_node_array_access(struct node *node)
{
    return node->type == NODE_TYPE_EXPRESSION && is_array_operator(node->exp.op);
}

/**
 * Finds the correct sub register to use for the original register provided.
 * 
 * I.e if the size is one byte and you provide eax as the original register then al will be returned
 * 
 * \attention The original register must be a 32-bit wide general purpose register i.e eax, ecx, edx, or ebx
 */
const char *codegen_sub_register(const char *original_register, size_t size)
{
    const char *reg = NULL;
    if (S_EQ(original_register, "eax"))
    {
        if (size == DATA_SIZE_BYTE)
        {
            reg = "al";
        }
        else if (size == DATA_SIZE_WORD)
        {
            reg = "ax";
        }
        else if (size == DATA_SIZE_DWORD)
        {
            reg = "eax";
        }
    }
    else if (S_EQ(original_register, "ebx"))
    {
        if (size == DATA_SIZE_BYTE)
        {
            reg = "bl";
        }
        else if (size == DATA_SIZE_WORD)
        {
            reg = "bx";
        }
        else if (size == DATA_SIZE_DWORD)
        {
            reg = "ebx";
        }
    }
    else if (S_EQ(original_register, "ecx"))
    {
        if (size == DATA_SIZE_BYTE)
        {
            reg = "cl";
        }
        else if (size == DATA_SIZE_WORD)
        {
            reg = "cx";
        }
        else if (size == DATA_SIZE_DWORD)
        {
            reg = "ecx";
        }
    }
    else if (S_EQ(original_register, "edx"))
    {
        if (size == DATA_SIZE_BYTE)
        {
            reg = "dl";
        }
        else if (size == DATA_SIZE_WORD)
        {
            reg = "dx";
        }
        else if (size == DATA_SIZE_DWORD)
        {
            reg = "edx";
        }
    }
    return reg;
}

/**
 * Finds weather this is a byte operation, word operation or double word operation based on the size provided
 * Returns either "byte", "word", or "dword" 
 */
const char *codegen_byte_word_or_dword(size_t size)
{
    const char *type = NULL;

    if (size == DATA_SIZE_BYTE)
    {
        type = "byte";
    }
    else if (size == DATA_SIZE_WORD)
    {
        type = "word";
    }
    else if (size == DATA_SIZE_DWORD)
    {
        type = "dword";
    }

    return type;
}

void codegen_generate_variable_access(struct node *node, struct resolver_result *result, struct history *history)
{
    struct resolver_entity *entity = resolver_result_entity_root(result);
    codegen_gen_mov_or_math("eax", node, history->flags, entity);
    entity = resolver_result_entity_next(entity);
    while (entity)
    {
       asm_push("mov eax, [eax+%i]", codegen_entity_private(entity)->offset);
        entity = resolver_result_entity_next(entity);
    }

}

void codegen_generate_assignment_expression(struct node *node, struct history *history)
{
    // Left node = to assign
    // Right node = value

    struct resolver_result *result = resolver_follow(current_process->resolver, node->exp.left);
    assert(!resolver_result_failed(result));

    struct resolver_entity *left_entity = resolver_result_entity_root(result);

    codegen_generate_expressionable(node->exp.right, history);

    register_unset_flag(REGISTER_EAX_IS_USED);
    asm_push("mov %s [%s], eax", codegen_byte_word_or_dword(left_entity->node->var.type.size), codegen_entity_private(left_entity)->address);
}

void codegen_generate_expressionable_function_arguments(struct resolver_entity *resolver_entity, struct node *func_call_args_exp_node, size_t *arguments_size)
{
    *arguments_size = 0;
    assert(func_call_args_exp_node->type == NODE_TYPE_EXPRESSION_PARENTHESIS);

    struct history history;

    // Code generate the function arguments
    codegen_generate_expressionable(func_call_args_exp_node, history_begin(&history, EXPRESSION_IN_FUNCTION_CALL | EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS));

    // Let's set the arguments size based on the history we have gathered.
    *arguments_size = history.function_call.arguments.size;
}

void codegen_generate_pop(const char *reg, size_t times)
{
    for (size_t i = 0; i < times; i++)
    {
        asm_push("pop %s", reg);
    }
}

int codegen_set_flag_for_operator(const char *op)
{
    int flag = 0;

    if (S_EQ(op, "+"))
    {
        flag |= EXPRESSION_IS_ADDITION;
    }
    else if (S_EQ(op, "-"))
    {
        flag |= EXPRESSION_IS_SUBTRACTION;
    }
    else if (S_EQ(op, "*"))
    {
        flag |= EXPRESSION_IS_MULTIPLICATION;
    }
    else if (S_EQ(op, "/"))
    {
        flag |= EXPRESSIPON_IS_DIVISION;
    }

    return flag;
}

int get_additional_flags(int current_flags, struct node *node)
{
    if (node->type != NODE_TYPE_EXPRESSION)
    {
        return 0;
    }

    int additional_flags = 0;
    bool maintain_function_call_argument_flag = (current_flags & EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS) && S_EQ(node->exp.op, ",");
    if (maintain_function_call_argument_flag)
    {
        additional_flags |= EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS;
    }

    return additional_flags;
}

void codegen_generate_exp_node_for_arithmetic(struct node *node, struct history *history)
{
    assert(node->type == NODE_TYPE_EXPRESSION);

    int flags = history->flags;

    // We need to set the correct flag regarding which operator is being used
    flags |= codegen_set_flag_for_operator(node->exp.op);

    codegen_generate_expressionable(node->exp.left, history_down(history, flags));
    codegen_generate_expressionable(node->exp.right, history_down(history, flags | EXPRESSION_FLAG_RIGHT_NODE));
}

void _codegen_generate_exp_node(struct node *node, struct history *history)
{
    if (is_node_assignment(node))
    {
        codegen_generate_assignment_expression(node, history);
        return;
    }

    // Can we locate a variable for the given expression?
    struct resolver_result *result = resolver_follow(current_process->resolver, node);
    struct resolver_entity *entity = NULL;
    if (resolver_result_ok(result))
    {
        entity = resolver_result_entity(result);
        codegen_generate_variable_access(node, result, history);
        return;
    }

    // Additional flags might need to be passed down to the other nodes even if they are naturally uninheritable
    // Examples include a function call with multiple arguments (50, 40, 30). In this case we have three arguments
    // so if this expression has a comma as the operator then one of the additional flags will be EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS
    // which specifies that we are in a function call argument, because we are.
    // If we have test(50, 40) then the expression (50, 40) has both left and right operands
    // being a function call argument. So the flag will be set for both of these by specifying the
    // additional_flags
    int additional_flags = get_additional_flags(history->flags, node);

    // Still not done? Then its probably an arithmetic expression of some kind.
    // I.e a+b+50
    codegen_generate_exp_node_for_arithmetic(node, history_down(history, codegen_remove_uninheritable_flags(history->flags) | additional_flags));
}

void codegen_generate_exp_node(struct node *node, struct history *history)
{
    // Reserve current flags as they can be changed by lower in hirarchy
    int flags = history->flags;

    // Generate the expression and all child expressions
    _codegen_generate_exp_node(node, history);

    // If we are in function call arguments then we must push the result to the stack
    // If we have comma then we got multiple arguments so no need to create a PUSH as it was
    // done earlier.

    // Hmm could be improve perhaps..
    if (flags & EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS && !S_EQ(node->exp.op, ","))
    {
        asm_push("PUSH eax");
    }
}

/**
 * Picks EBX or EDX depending which one is free, if none are free then this is a compiler bug.
 * Once a register is picked it is marked as used. It is the callers responsibility
 * to unuse it when its done.
 */
const char *codegen_choose_ebx_or_edx()
{
    assert(!(register_is_used("edx") && register_is_used("ebx")));
    const char *reg = "";
    if (!register_is_used("ebx"))
    {
        reg = "ebx";
        register_set_flag(REGISTER_EBX_IS_USED);
    }
    else if (!register_is_used("edx"))
    {
        reg = "edx";
        register_set_flag(REGISTER_EDX_IS_USED);
    }

    return reg;
}

void codegen_handle_variable_access(struct node *access_node, struct resolver_entity *entity, struct history *history)
{
    int flags = history->flags;

    // Are we instructed to get the address of this entity?
    if (flags & EXPRESSION_GET_ADDRESS)
    {
        register_set_flag(REGISTER_EBX_IS_USED);

        // We have indirection, therefore we should load the address into EBX.
        // rather than mov instruction
        asm_push("lea ebx, [%s]", codegen_entity_private(entity)->address);
        // We are done for now
        return;
    }

    // Normal variable access?
    // Generate move or math.
    codegen_gen_mov_or_math("eax", access_node, flags, entity);

    if (flags & EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS)
    {
        // We have a function call argument, therefore we must push the argument to the stck
        // so that it can be passed to the function we are calling
        asm_push("PUSH eax");

        // We are done with the EAX register.
        register_unset_flag(REGISTER_EAX_IS_USED);

        // Think of a function name for this.. cant think of one..
        // another time
        history->function_call.arguments.size += 4;
    }
}

void codegen_generate_identifier(struct node *node, struct history *history)
{
    struct resolver_result *result = resolver_follow(current_process->resolver, node);
    assert(resolver_result_ok(result));
    
    struct resolver_entity *entity = resolver_result_entity(result);
    codegen_generate_variable_access(node, result, history);
}

static bool is_comma_operator(struct node *node)
{
    return S_EQ(node->exp.op, ",");
}

void codegen_generate_unary_indirection(struct node *node, struct history *history)
{
    int flags = history->flags;
    // Generate the operand while passing the indirection flag
    codegen_generate_expressionable(node->unary.operand, history_down(history, flags | EXPRESSION_GET_ADDRESS | EXPRESSION_INDIRECTION));

    // EBX register now has the address of the variable for indirection
}

void codegen_generate_indirection_for_unary(struct node *node, struct history *history)
{
    // Firstly generate the expressionable of the unary
    codegen_generate_expressionable(node, history);
    int depth = node->unary.indirection.depth;
    for (int i = 0; i < depth; i++)
    {
    }
}

void codegen_generate_normal_unary(struct node *node, struct history *history)
{
    codegen_generate_expressionable(node->unary.operand, history);
    // We have generated the value for the operand
    // Let's now decide what to do with the result based on the operator
    if (S_EQ(node->unary.op, "-"))
    {
        // We have negation operator, so negate.
        asm_push("neg eax");
    }
    else if (S_EQ(node->unary.op, "*"))
    {
        // We are accessing a pointer
        codegen_generate_indirection_for_unary(node, history);
    }
}

void codegen_generate_unary(struct node *node, struct history *history)
{
    int flags = history->flags;

    // Indirection unary should be handled differently to most unaries
    if (op_is_indirection(node->unary.op))
    {
        codegen_generate_unary_indirection(node, history);
        return;
    }

    codegen_generate_normal_unary(node, history);
}

void codegen_generate_exp_parenthesis_node(struct node *node, struct history *history)
{
    codegen_generate_expressionable(node->parenthesis.exp, history);
}

void codegen_generate_expressionable(struct node *node, struct history *history)
{
    switch (node->type)
    {
    case NODE_TYPE_NUMBER:
        codegen_generate_number_node(node, history);
        break;

    case NODE_TYPE_EXPRESSION:
        codegen_generate_exp_node(node, history);
        break;
    case NODE_TYPE_IDENTIFIER:
        codegen_generate_identifier(node, history);
        break;
    case NODE_TYPE_EXPRESSION_PARENTHESIS:
        codegen_generate_exp_parenthesis_node(node, history);
        break;

    case NODE_TYPE_UNARY:
        codegen_generate_unary(node, history);
        break;
    }
}

static void codegen_generate_variable_for_array(struct node *node)
{
    if (node->var.val != NULL)
    {
        codegen_err("We don't yet support values for arrays");
        return;
    }

    char tmp_buf[256];
    asm_push("%s: %s ", node->var.name, asm_keyword_for_size(variable_size(node), tmp_buf));
}
static void codegen_generate_global_variable_for_primitive(struct node *node)
{
    char tmp_buf[256];
    if (node->var.val != NULL)
    {
        asm_push("%s: %s %lld", node->var.name, asm_keyword_for_size(variable_size(node), tmp_buf), node->var.val->llnum, tmp_buf);
        return;
    }

    asm_push("%s: %s 0", node->var.name, asm_keyword_for_size(variable_size(node), tmp_buf));
}

static void codegen_generate_global_variable_for_struct(struct node *node)
{
    if (node->var.val != NULL)
    {
        codegen_err("We don't yet support values for structures");
        return;
    }

    char tmp_buf[256];
    asm_push("%s: %s", node->var.name, asm_keyword_for_size(variable_size(node), tmp_buf));
}

void codegen_generate_global_variable(struct node *node)
{
    asm_push("; %s %s", node->var.type.type_str, node->var.name);
    if (node->var.type.flags & DATATYPE_FLAG_IS_ARRAY)
    {
        codegen_generate_variable_for_array(node);
        codegen_new_scope_entity(node, 0, 0);
        return;
    }

    switch (node->var.type.type)
    {
    case DATA_TYPE_CHAR:
    case DATA_TYPE_SHORT:
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_LONG:
        codegen_generate_global_variable_for_primitive(node);
        break;
    case DATA_TYPE_DOUBLE:
        codegen_err("Doubles are not yet supported for global variable values.");

        break;
    case DATA_TYPE_FLOAT:
        codegen_err("Floats are not yet supported for global variable values.");
        break;

    case DATA_TYPE_STRUCT:
        codegen_generate_global_variable_for_struct(node);
        break;
    default:
        codegen_err("Not sure how to generate value for global variable.. Problem!");
    }
    assert(node->type == NODE_TYPE_VARIABLE);
    codegen_new_scope_entity(node, 0, 0);
}

size_t codegen_compute_stack_size(struct vector *vec)
{
    // Empty vector then theirs no stack size.
    if (vector_count(vec) == 0)
        return 0;

    size_t stack_size = 0;
    vector_set_peek_pointer(vec, 0);
    struct node *node = vector_peek_ptr(vec);
    while (node)
    {
        switch (node->type)
        {
        case NODE_TYPE_VARIABLE:
            stack_size += variable_size(node);
            break;

        default:
            // We ignore all other nodes, we don't care for them. they wont
            // help us compute the stack size...
            break;
        };
        node = vector_peek_ptr(vec);
    }

    // Stack size must be 16 byte aligned as per C specification
    return C_ALIGN(stack_size);
}

void codegen_generate_scope_variable(struct node *node)
{
    // Register the variable to the scope.
    struct resolver_entity *entity = codegen_new_scope_entity(node, node->var.aoffset, CODEGEN_ENTITY_FLAG_IS_LOCAL_STACK);

    // Scope variables have values, lets compute that
    if (node->var.val)
    {
        struct history history;
        // Process the right node first as this is an expression
        codegen_generate_expressionable(node->var.val, history_begin(&history, 0));

        // Mark the EAX register as no longer used.
        register_unset_flag(REGISTER_EAX_IS_USED);

        char address[256];
        // Write the move. Only intergers supported at the moment as you can see
        // this will be improved.
        asm_push("mov [%s], eax", codegen_stack_asm_address(codegen_entity_private(entity)->offset, address));
    }

    register_unset_flag(REGISTER_EAX_IS_USED);
}

void codegen_generate_statement_return(struct node *node)
{
    struct history history;

    // Let's generate the expression of the return statement
    codegen_generate_expressionable(node->stmt.ret.exp, history_begin(&history, 0));

    // Generate the stack subtraction.
    // codegen_stack_add(codegen_align(codegen_scope_current()->size));

    // Now we must leave the function
    asm_push("pop ebp");
    asm_push("ret");

    // EAX is available now
    register_unset_flag(REGISTER_EAX_IS_USED);
}

void codegen_generate_statement(struct node *node)
{
    struct history history;
    switch (node->type)
    {

    case NODE_TYPE_EXPRESSION:
        codegen_generate_exp_node(node, history_begin(&history, 0));
        break;

    case NODE_TYPE_UNARY:
        codegen_generate_unary(node, history_begin(&history, 0));
        break;

    case NODE_TYPE_VARIABLE:
        codegen_generate_scope_variable(node);
        break;

    case NODE_TYPE_STATEMENT_RETURN:
        codegen_generate_statement_return(node);
        break;
    }
}

void codegen_generate_scope_no_new_scope(struct vector *statements)
{
    vector_set_peek_pointer(statements, 0);
    struct node *statement_node = vector_peek_ptr(statements);
    while (statement_node)
    {
        codegen_generate_statement(statement_node);
        statement_node = vector_peek_ptr(statements);
    }
}

void codegen_generate_stack_scope(struct vector *statements)
{
    // New body new scope.

    // Resolver scope needs to exist too it will be this normal scopes replacement
    codegen_new_scope(CODEGEN_SCOPE_FLAG_IS_LOCAL_STACK);

    // We got to compute the stack size we need for our statements
    size_t stack_size = codegen_compute_stack_size(statements);
    codegen_stack_sub(stack_size);
    codegen_generate_scope_no_new_scope(statements);
    codegen_stack_add(stack_size);
    codegen_finish_scope();
}

void codegen_generate_function_body(struct node *node)
{
    codegen_generate_stack_scope(node->body.statements);
}

void codegen_generate_function(struct node *node)
{
    asm_push("; %s function", node->func.name);
    asm_push("%s:", node->func.name);

    // We have to create a stack frame ;)
    asm_push("push ebp");
    asm_push("mov ebp, esp");

    // Generate scope for functon arguments
    codegen_new_scope(0);
    //  codegen_generate_function_arguments(node->func.argument_vector);

    // Generate the function body
    codegen_generate_function_body(node->func.body_n);

    // End function argument scope
    codegen_finish_scope();

    asm_push("pop ebp");
    asm_push("ret");
}

void codegen_generate_root_node(struct node *node)
{
    switch (node->type)
    {
    case NODE_TYPE_VARIABLE:
        // Was processed earlier.. for data section
        break;

    case NODE_TYPE_FUNCTION:
        codegen_generate_function(node);
        break;
    }
}

void codegen_generate_data_section_part(struct node *node)
{
    if (node->type == NODE_TYPE_VARIABLE)
    {
        codegen_generate_global_variable(node);
    }
}

void codegen_generate_data_section()
{
    asm_push("section .data");
    struct node *node = NULL;
    while ((node = node_next()) != NULL)
    {
        codegen_generate_data_section_part(node);
    }
}
/**
 * Starts generating code from the root of the tree, working its way down the leafs
 */
void codegen_generate_root()
{
    asm_push("section .text");
    struct node *node = NULL;
    while ((node = node_next()) != NULL)
    {
        codegen_generate_root_node(node);
    }
}

/**
 * This function is called when a structure is accessed and that causes scope entities
 * to be created. This function is responsible for retunring the private data that must
 * be assigned to this resolver_entity
 * 
 * Only called for the first entity structure that needs to be created.
 * 
 * I.e "a.b.c" will only be called for "b". "c will call another function
 */
void *codegen_new_struct_entity(struct resolver_result* result, struct node *var_node, int offset, int flags)
{
    int entity_flags = 0;
    struct codegen_entity_data *result_entity = codegen_new_entity_data(result->last_struct_entity->node, offset, entity_flags);
    return result_entity;
}

void* codegen_merge_struct_entity(struct resolver_result* result, struct resolver_entity* left_entity, struct resolver_entity* right_entity)
{
    int left_offset = codegen_entity_private(left_entity)->offset;
    int right_offset = codegen_entity_private(right_entity)->offset;
    return codegen_new_entity_data(result->first_entity_const->node, left_offset+right_offset, 0);
}

void *codegen_new_array_entity(struct resolver_result* result, struct resolver_entity *array_entity, int index_val, int index)
{
    int index_offset = array_offset(&array_entity->dtype, index, index_val);
    int final_offset = codegen_entity_private(array_entity)->offset + index_offset;
    return codegen_new_entity_data(array_entity->node, final_offset, 0);

}

void codegen_delete_entity(struct resolver_entity *entity)
{
    free(entity->private);
}

void codegen_delete_scope(struct resolver_scope *scope)
{
    free(scope->private);
}

int codegen(struct compile_process *process)
{
    current_process = process;
    // Create the root scope for this process
    scope_create_root(process);

    vector_set_peek_pointer(process->node_tree_vec, 0);
    process->resolver = resolver_new_process(process, &(struct resolver_callbacks){.new_struct_entity = codegen_new_struct_entity, .merge_struct_entity=codegen_merge_struct_entity, .new_array_entity = codegen_new_array_entity, .delete_entity = codegen_delete_entity, .delete_scope = codegen_delete_scope});
    // Global variables and down the tree locals... Global scope lets create it.
    codegen_new_scope(0);
    codegen_generate_data_section();
    vector_set_peek_pointer(process->node_tree_vec, 0);
    codegen_generate_root();
    codegen_finish_scope();

    return 0;
}