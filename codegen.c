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

#define codegen_scope_new() \
    scope_new(current_process, 0)

#define codegen_scope_finish() \
    scope_finish(current_process)

#define codegen_scope_push(value, elem_size) \
    scope_push(current_process, value, elem_size)

#define codegen_scope_last_entity() \
    scope_last_entity(current_process)

#define codegen_scope_current() \
    scope_current(current_process)

struct codegen_scope_entity *codegen_get_scope_variable_for_node(struct node *node, bool *position_known_at_compile_time);
void codegen_scope_entity_to_asm_address(struct codegen_scope_entity *entity, char *out);

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

// Simple bitmask for scope entity rules.
enum
{
    CODEGEN_SCOPE_ENTITY_LOCAL_STACK = 0b00000001,
    CODEGEN_SCOPE_ENTITY_INDIRECTION = 0b00000010
};

struct codegen_scope_entity
{
    // The flags for this scope entity
    int flags;

    // The stack offset this scope entity can be accessedd at.
    // i.e -4, -8 -12
    // If this scope entity has no stack entity as its a global scope
    // then this value should be ignored.
    int stack_offset;

    // A node to a variable declaration.
    struct node *node;

    struct seindirection
    {
        // I.e pointer depth "**a = 50" this would mean we solved the scope entity variable "a"
        // and it was accessed with a depth of two.
        int depth;
    } indirection;
};

enum
{
    CODEGEN_GLOBAL_ENTITY_INDIRECTION = 0b00000001
};

struct codegen_global_entity
{
    // The flags for the global entity
    int flags;

    // The node to the global entity variable node
    struct node *node;

    struct geindirection
    {
        // I.e pointer depth "**a = 50" this would mean we solved the scope entity variable "a"
        // and it was accessed with a depth of two.
        int depth;
    } indirection;

    // The offset relative to the base address of the global entity
    int offset;
};

enum
{
    CODEGEN_ENTITY_TYPE_STACK,
    CODEGEN_ENTITY_TYPE_SYMBOL
};

// Codegen entity types
enum
{
    CODEGEN_ENTITY_FLAG_COMPLETED_ADDRESS = 0b00000001,
    CODEGEN_ENTITY_FLAG_HAS_INDIRECTION = 0b00000010
};

/**
 * Codegen entities are addressable areas of memory known at compile time.
 * For example they can represent scope variables, functions or global variables
 */
struct codegen_entity
{
    int type;
    // The node of the entity
    struct node *node;
    // The address that can be addressed in assembly. I.e [ebp-4] [name]
    char address[60];
    bool is_scope_entity;

    // If the position cannot be known at compile time because it has a run-time only address
    // such as a pointer for example. Then the CODEGEN_ENTITY_COMPLETED_ADDRESS flag will not be set
    int flags;

    union
    {
        struct codegen_scope_entity *scope_entity;
        struct global
        {
            struct codegen_global_entity *entity;
            struct symbol *sym;
        } global;
    };

    struct eindirection
    {
        int depth;
    } indirection;
};

bool codegen_entity_address_known(struct codegen_entity *entity)
{
    return entity->flags & CODEGEN_ENTITY_FLAG_COMPLETED_ADDRESS;
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

struct codegen_scope_entity *codegen_new_scope_entity(struct node *node, int stack_offset, int flags)
{
    struct codegen_scope_entity *entity = calloc(sizeof(struct codegen_scope_entity), 1);
    entity->node = node;
    entity->flags = flags;
    entity->stack_offset = stack_offset;
    return entity;
}

void codegen_free_scope_entity(struct codegen_scope_entity *entity)
{
    free(entity);
}

struct codegen_global_entity *codegen_new_global_entity(struct node *node, int offset, int flags)
{
    struct codegen_global_entity *entity = calloc(sizeof(struct codegen_global_entity), 1);
    entity->node = node;
    entity->flags = flags;
    entity->offset = offset;
    return entity;
}

void codegen_free_global_entity(struct codegen_global_entity *entity)
{
    free(entity);
}

struct codegen_scope_entity *codegen_get_scope_variable(const char *name)
{
    struct scope *current = codegen_scope_current();
    while (current)
    {
        scope_iteration_start(current);
        for (int i = 0; i < vector_count(current->entities); i++)
        {
            struct codegen_scope_entity *entity = scope_iterate_back(current);
            if (!entity)
            {
                continue;
            }

            if (S_EQ(entity->node->var.name, name))
                return entity;
        }

        scope_iteration_end(current);
        current = current->parent;
    }

    return NULL;
}

struct node *codegen_get_entity_node(struct codegen_entity *entity)
{
    struct node *node = NULL;
    switch (entity->type)
    {
    case CODEGEN_ENTITY_TYPE_STACK:
        node = entity->scope_entity->node;
        break;

    case CODEGEN_ENTITY_TYPE_SYMBOL:
        node = entity->global.entity->node;
        break;

    default:
        // TODO: Create a function to do this kind of thing..
        assert(0 == 1 && "Unknown entity");
    }

    return node;
}

struct codegen_global_entity *codegen_get_global_variable_for_node(struct node *node, bool *position_known_at_compile_time, struct symbol **sym_out)
{
    struct codegen_global_entity *entity = NULL;
    switch (node->type)
    {
    case NODE_TYPE_EXPRESSION:
        if (is_access_operator(node->exp.op))
        {
            entity = codegen_get_global_variable_for_node(node->exp.left, position_known_at_compile_time, sym_out);
            if (!entity)
            {
                // Cannot find scope entity? Perhaps its a global variable
                return NULL;
            }

            // Ok entity is the root entity i.e "a.b" (variable a)
            // We have access operator so we must get the next variable.

            // Offset will store the absolute offset from zero for the strucutre access
            int offset = 0;
            struct node *access_node = struct_for_access(current_process, node->exp.right, entity->node->var.type.type_str, &offset, 0);
            entity = codegen_new_global_entity(access_node, offset, 0);
            break;
        }

        entity = codegen_get_global_variable_for_node(node->exp.left, position_known_at_compile_time, sym_out);
        break;

    case NODE_TYPE_EXPRESSION_PARENTHESIS:
        entity = codegen_get_global_variable_for_node(node->parenthesis.exp, position_known_at_compile_time, sym_out);
        break;

    // Repeating myself in both global and stack scopes. Consider making function
    case NODE_TYPE_UNARY:
        entity = codegen_get_global_variable_for_node(node->unary.operand, position_known_at_compile_time, sym_out);
        if (S_EQ(node->unary.op, "*"))
        {
            entity->flags |= CODEGEN_GLOBAL_ENTITY_INDIRECTION;
            entity->indirection.depth = node->unary.indirection.depth;
        }
        break;

    case NODE_TYPE_IDENTIFIER:
    {
        // We shouldn't be creating a new global entity every time we resolve it
        // Best to solve this in the symresolver, when you get the time
        struct symbol *sym = symresolver_get_symbol(current_process, node->sval);
        struct node *var_node = symresolver_node(sym);
        entity = codegen_new_global_entity(var_node, var_node->var.offset, 0);
        if (sym_out && !(*sym_out))
        {
            // We haven't resolved the symbol of the global variable yet?
            // Then let's let the caller know that we are that resolved symbol
            *sym_out = sym;
        }
    }
    break;
    }

    *position_known_at_compile_time = true;

    return entity;
}

void codegen_put_address_for_global_entity(struct codegen_entity *entity_out, struct codegen_global_entity *entity)
{
    assert(entity_out->global.sym);

    if (entity->offset)
    {
        sprintf(entity_out->address, "%s+%i", entity_out->global.sym->name, entity->offset);
        return;
    }

    sprintf(entity_out->address, "%s", entity_out->global.sym->name);
}

int codegen_get_global_entity_for_node(struct node *node, struct codegen_entity *entity_out)
{
    bool position_known_at_compile_time = false;
    struct symbol *sym = NULL;
    struct codegen_global_entity *entity = codegen_get_global_variable_for_node(node, &position_known_at_compile_time, &sym);
    if (!entity)
    {
        return -1;
    }

    // We got the resolved symbol right?
    assert(sym);

    entity_out->type = CODEGEN_ENTITY_TYPE_SYMBOL;
    entity_out->global.sym = sym;
    entity_out->global.entity = entity;
    entity_out->is_scope_entity = false;
    entity_out->node = entity->node;

    if (position_known_at_compile_time)
        entity_out->flags |= CODEGEN_ENTITY_FLAG_COMPLETED_ADDRESS;

    if (entity->flags & CODEGEN_GLOBAL_ENTITY_INDIRECTION)
    {
        entity_out->flags |= CODEGEN_ENTITY_FLAG_HAS_INDIRECTION;
        entity_out->indirection.depth = entity->indirection.depth;
    }

    codegen_put_address_for_global_entity(entity_out, entity);

    // We only deal with node symbols right now.
    assert(sym->type == SYMBOL_TYPE_NODE);

    return 0;
}

int codegen_get_entity_for_node(struct node *node, struct codegen_entity *entity_out)
{
    memset(entity_out, 0, sizeof(struct codegen_entity));

    bool position_known_at_compile_time = false;
    struct codegen_scope_entity *scope_entity = codegen_get_scope_variable_for_node(node, &position_known_at_compile_time);
    if (scope_entity)
    {
        entity_out->type = CODEGEN_ENTITY_TYPE_STACK;
        entity_out->scope_entity = scope_entity;
        entity_out->is_scope_entity = true;

        codegen_scope_entity_to_asm_address(entity_out->scope_entity, entity_out->address);
        entity_out->node = codegen_get_entity_node(entity_out);

        if (position_known_at_compile_time)
            entity_out->flags |= CODEGEN_ENTITY_FLAG_COMPLETED_ADDRESS;

        if (scope_entity->flags & CODEGEN_SCOPE_ENTITY_INDIRECTION)
        {
            entity_out->flags |= CODEGEN_ENTITY_FLAG_HAS_INDIRECTION;
            entity_out->indirection.depth = scope_entity->indirection.depth;
        }

        return 0;
    }

    return codegen_get_global_entity_for_node(node, entity_out);
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

void codegen_new_expression_state()
{
    struct expression_state *state = malloc(sizeof(struct expression_state));
    memset(state, 0, sizeof(struct expression_state));

    vector_push(current_process->generator.states.expr, &state);
}

struct expression_state *codegen_current_exp_state()
{
    struct expression_state *state = vector_back_ptr_or_null(current_process->generator.states.expr);
    if (!state)
    {
        return &blank_state;
    }

    return state;
}

void codegen_end_expression_state()
{
    struct expression_state *state = codegen_current_exp_state();
    // Delete the memory we don't need it anymore
    free(state);
    vector_pop(current_process->generator.states.expr);
}

static bool exp_flag_set(int flag)
{
    return codegen_current_exp_state()->flags & flag;
}

bool codegen_expression_on_right_operand()
{
    return codegen_current_exp_state()->flags & EXPRESSION_FLAG_RIGHT_NODE;
}

void codegen_expression_flags_set_right_operand(bool is_right_operand)
{
    assert(codegen_current_exp_state() != &blank_state);
    codegen_current_exp_state()->flags &= ~EXPRESSION_FLAG_RIGHT_NODE;
    if (is_right_operand)
    {
        codegen_current_exp_state()->flags |= EXPRESSION_FLAG_RIGHT_NODE;
    }
}

void codegen_expression_flags_set_in_function_call_arguments(bool in_function_arguments)
{
    assert(codegen_current_exp_state() != &blank_state);
    if (in_function_arguments)
    {
        codegen_current_exp_state()->flags |= EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS;
        return;
    }

    codegen_current_exp_state()->flags &= ~EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS;
    memset(&codegen_current_exp_state()->fca, 0, sizeof(codegen_current_exp_state()->fca));
}

void codegen_generate_node(struct node *node);
void codegen_generate_expressionable(struct node *node, struct history *history);
void codegen_generate_new_expressionable(struct node *node, struct history *history)
{
    codegen_generate_expressionable(node, history);
}
const char *op;

// Rename this function... terrible name
// Return result should be used immedeitly and not stored
// copy only! Temp result!
static const char *codegen_get_fmt_for_value(struct node *value_node, struct codegen_entity *entity)
{
    assert(value_node->type == NODE_TYPE_NUMBER || value_node->type == NODE_TYPE_IDENTIFIER);

    static char tmp_buf[256];
    if (value_node->type == NODE_TYPE_NUMBER)
    {
        sprintf(tmp_buf, "%lld", value_node->llnum);
        return tmp_buf;
    }

    // SO we are an identifier, in this case we also have an address.
    // The entity address is all we care about.
    sprintf(tmp_buf, "[%s]", entity->address);
    return tmp_buf;
}

static void codegen_gen_math(const char *reg, struct node *value_node, int flags, struct codegen_entity *entity)
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

static void codegen_gen_mov_or_math(const char *reg, struct node *value_node, int flags, struct codegen_entity *entity)
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

/**
 * Iterates through the node until a codegen_scope_entity can be found, otherwise returns NULL.
 * This function does not work for structure pointers as its impossible to complete this action
 * at compile time. If the node ends up at any point pointing to a structure pointer, then this function
 * will return the closest entity it can that can be done at compile time and the position_known_at_compile_time will be set to false signifying some runtime code will be required to complete
 * positioning.
 * 
 * \param position_known_at_compile_time The pointer here is set to true by this function if the exact location of the variable is known. Otherwise its false. 
 */
struct codegen_scope_entity *codegen_get_scope_variable_for_node(struct node *node, bool *position_known_at_compile_time)
{
    assert(node);

    *position_known_at_compile_time = false;

    struct codegen_scope_entity *entity = NULL;
    switch (node->type)
    {
    case NODE_TYPE_EXPRESSION:
        if (is_access_operator(node->exp.op))
        {
            entity = codegen_get_scope_variable_for_node(node->exp.left, position_known_at_compile_time);
            if (!entity)
            {
                // Cannot find scope entity? Perhaps its a global variable
                return NULL;
            }

            // Ok entity is the root entity i.e "a.b" (variable a)
            // We have access operator so we must get the next variable.

            // Offset will store the absolute offset from zero for the strucutre access
            // it acts as if its a global variable, as we are on the scope we need
            // to convert this to a stack address.
            int offset = 0;
            struct node *access_node = struct_for_access(current_process, node->exp.right, entity->node->var.type.type_str, &offset, 0);
            offset += entity->stack_offset;
            entity = codegen_new_scope_entity(access_node, offset, 0);
            break;
        }

        entity = codegen_get_scope_variable_for_node(node->exp.left, position_known_at_compile_time);
        break;

    case NODE_TYPE_EXPRESSION_PARENTHESIS:
        entity = codegen_get_scope_variable_for_node(node->parenthesis.exp, position_known_at_compile_time);
        break;

    case NODE_TYPE_UNARY:
        entity = codegen_get_scope_variable_for_node(node->unary.operand, position_known_at_compile_time);
        if (S_EQ(node->unary.op, "*"))
        {
            entity->flags |= CODEGEN_SCOPE_ENTITY_INDIRECTION;
            entity->indirection.depth = node->unary.indirection.depth;
        }
        break;

    case NODE_TYPE_IDENTIFIER:
        entity = codegen_get_scope_variable(node->sval);
        break;
    }

    *position_known_at_compile_time = true;

    return entity;
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

void codegen_generate_assignment_expression(struct node *node)
{
    struct codegen_entity assignment_operand_entity;

    struct history history;

    // Process the right node first as this is an expression
    codegen_generate_expressionable(node->exp.right, history_begin(&history, 0));
    // Now lets find the stack offset
    assert(codegen_get_entity_for_node(node->exp.left, &assignment_operand_entity) == 0);

    // Mark the EAX register as no longer used.
    register_unset_flag(REGISTER_EAX_IS_USED);

    // Do we have any pointer indirection for this assignment?
    if (assignment_operand_entity.flags & CODEGEN_ENTITY_FLAG_HAS_INDIRECTION)
    {
        int depth = assignment_operand_entity.indirection.depth;
        asm_push("mov ebx, [%s]", assignment_operand_entity.address);

        // We got any more depth?
        for (int i = 1; i < depth; i++)
        {
            asm_push("mov ebx, [ebx]");
        }

        // We finally here? Good write EAX into the address we care about.
        asm_push("mov %s [ebx], eax", codegen_byte_word_or_dword(assignment_operand_entity.node->var.type.size));

        return;
    }

    // Normal variable no pointer access? Then write the move
    asm_push("mov %s [%s], eax", codegen_byte_word_or_dword(assignment_operand_entity.node->var.type.size), assignment_operand_entity.address);
}

void codegen_generate_expressionable_function_arguments(struct codegen_entity *func_entity, struct node *func_call_args_exp_node, size_t *arguments_size)
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

void codegen_generate_function_call_for_exp_node(struct codegen_entity *func_entity, struct node *node)
{
    // Generate expression for left node. EBX should contain the address we care about
    codegen_generate_expressionable(node->exp.left, 0);

    size_t arguments_size = 0;

    // Generate the function arguments i.e (50, 40, 30)
    codegen_generate_expressionable_function_arguments(func_entity, node->exp.right, &arguments_size);

    // Call the function
    asm_push("call [ebx]");

    // EAX register is now used because it contains the return result. Important that
    // we mark it as used to prevent it being overwritten in some sort of expression
    register_set_flag(REGISTER_EAX_IS_USED);

    // We don't need EBX anymore
    register_unset_flag(REGISTER_EBX_IS_USED);

    // We don't ahve to align the arguments size because the deeper parts of the system
    // should have done that already.
    asm_push("add esp, %i", arguments_size);
}

bool codegen_handle_codegen_entity_for_expression(struct codegen_entity *entity_out, struct node *node)
{
    assert(node->type == NODE_TYPE_EXPRESSION);
    bool done = false;
    if (codegen_get_entity_for_node(node, entity_out) == 0)
    {
        switch (entity_out->node->type)
        {
        case NODE_TYPE_FUNCTION:
            codegen_generate_function_call_for_exp_node(entity_out, node);
            done = true;
            break;
        }
    }

    return done;
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
        codegen_generate_assignment_expression(node);
        return;
    }

    struct codegen_entity entity;
    if (codegen_handle_codegen_entity_for_expression(&entity, node))
    {
        // We handled a codegen entity for a given expression.
        // Therefore we have done the job
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

void codegen_scope_entity_to_asm_address(struct codegen_scope_entity *entity, char *out)
{
    codegen_stack_asm_address(entity->stack_offset, out);
}

void codegen_handle_variable_access(struct node *access_node, struct codegen_entity *entity, struct history *history)
{
    int flags = history->flags;

    // Are we instructed to get the address of this entity?
    if (flags & EXPRESSION_GET_ADDRESS)
    {
        register_set_flag(REGISTER_EBX_IS_USED);

        // We have indirection, therefore we should load the address into EBX.
        // rather than mov instruction
        asm_push("lea ebx, [%s]", entity->address);
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

void codegen_handle_function_access(struct codegen_entity *entity, struct history *history)
{
    register_set_flag(REGISTER_EBX_IS_USED);
    asm_push("lea ebx, [%s]", entity->address);
}

void codegen_generate_identifier(struct node *node, struct history *history)
{
    struct codegen_entity entity;
    assert(codegen_get_entity_for_node(node, &entity) == 0);

    // WHat is the type that we are referencing? A variable, a function? WHat is it...
    switch (entity.node->type)
    {
    case NODE_TYPE_VARIABLE:
        codegen_handle_variable_access(node, &entity, history);
        break;

    case NODE_TYPE_FUNCTION:
        codegen_handle_function_access(&entity, history);
        break;

    default:
        // Get a function for this thing..
        assert(1 == 0 && "Compiler bug");
    }
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

    case NODE_TYPE_EXPRESSION_PARENTHESIS:
        codegen_generate_exp_parenthesis_node(node, history);
        break;
    case NODE_TYPE_IDENTIFIER:
        codegen_generate_identifier(node, history);
        break;

    case NODE_TYPE_UNARY:
        codegen_generate_unary(node, history);
        break;
    }
}

static void codegen_generate_global_variable_for_non_floating(struct node *node)
{
    char tmp_buf[256];
    if (node->var.val != NULL)
    {
        asm_push("%s: %s %lld", node->var.name, asm_keyword_for_size(node->var.type.size, tmp_buf), node->var.val->llnum, tmp_buf);
        return;
    }

    asm_push("%s: %s 0", node->var.name, asm_keyword_for_size(node->var.type.size, tmp_buf));
}

static void codegen_generate_global_variable_for_struct(struct node *node)
{
    if (node->var.val != NULL)
    {
        codegen_err("We don't yet support values for structures");
        return;
    }

    char tmp_buf[256];
    asm_push("%s: %s", node->var.name, asm_keyword_for_size(node->var.type.size, tmp_buf));
}

void codegen_generate_global_variable(struct node *node)
{
    asm_push("; %s %s", node->var.type.type_str, node->var.name);
    switch (node->var.type.type)
    {
    case DATA_TYPE_CHAR:
    case DATA_TYPE_SHORT:
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_LONG:
        codegen_generate_global_variable_for_non_floating(node);
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
            stack_size += node->var.type.size;
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

int codegen_stack_offset(struct node *node, int flags)
{
    int offset = node->var.type.size;

    // If the stack is local then we must grow downwards
    // as we want to access our local function arguments in our given scope
    // You would not provide this flag for things such as function arguments whose stack
    // is created by the function caller.
    if (flags & CODEGEN_SCOPE_ENTITY_LOCAL_STACK)
    {
        offset = -offset;
    }

    struct codegen_scope_entity *last_entity = codegen_scope_last_entity();
    if (last_entity)
    {

        // If this entity is not on a local stack but we want to get an offset
        // for an element on a stack then their is an incompatability
        // we should just return the current offset and make no effort to include it
        // in the calculation at all.
        if ((flags & CODEGEN_SCOPE_ENTITY_LOCAL_STACK) && !(last_entity->flags & CODEGEN_SCOPE_ENTITY_LOCAL_STACK))
        {
            return offset;
        }

        // We use += because if the stack_offset is negative then this will do a negative
        // if its positive then it will do a positive. += is the best operator for both cases
        offset += last_entity->stack_offset;
    }

    return offset;
}

void codegen_generate_scope_variable(struct node *node)
{
    struct codegen_scope_entity *entity = codegen_new_scope_entity(node, node->var.aoffset, CODEGEN_SCOPE_ENTITY_LOCAL_STACK);
    
    codegen_scope_push(entity, node->var.type.size);

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
        asm_push("mov [%s], eax", codegen_stack_asm_address(entity->stack_offset, address));
    }

    register_unset_flag(REGISTER_EAX_IS_USED);
}

void codegen_generate_scope_variable_for_first_function_argument(struct node *node)
{
    // The first function argument is also +8 from the base pointer
    // this is because of the base pointer stored on the stack and the return address
    // Naturally once the first function argument has been stored in its scope with the +8 offset
    // any additional generated function arguments will take the previous
    // scope variable into account. So we only need to +8 here for the first argument
    codegen_scope_push(codegen_new_scope_entity(node, C_OFFSET_FROM_FIRST_FUNCTION_ARGUMENT, 0), node->var.type.size);
}

void codegen_generate_scope_variable_for_function_argument(struct node *node)
{
    codegen_scope_push(codegen_new_scope_entity(node, codegen_stack_offset(node, 0), 0), node->var.type.size);
}

void codegen_generate_statement_return(struct node *node)
{
    struct history history;

    // Let's generate the expression of the return statement
    codegen_generate_expressionable(node->stmt.ret.exp, history_begin(&history, 0));

    // Generate the stack subtraction.
    codegen_stack_add(codegen_align(codegen_scope_current()->size));

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

void codegen_generate_scope(struct vector *statements)
{
    // New body new scope.
    codegen_scope_new();
    // We got to compute the stack size we need for our statements
    size_t stack_size = codegen_compute_stack_size(statements);
    codegen_stack_sub(stack_size);
    codegen_generate_scope_no_new_scope(statements);
    codegen_stack_add(stack_size);

    codegen_scope_finish();
}

void codegen_generate_function_body(struct node *node)
{
    codegen_generate_scope(node->body.statements);
}

void codegen_generate_function_argument(struct node *node)
{
    // Check for compiler bug. Arguments must be variables.
    // No if statements allowed in a function argument lol
    assert(node->type == NODE_TYPE_VARIABLE);

    codegen_generate_scope_variable_for_function_argument(node);
}

void codegen_generate_first_function_argument(struct node *node)
{
    assert(node->type == NODE_TYPE_VARIABLE);
    codegen_generate_scope_variable_for_first_function_argument(node);
}

void codegen_generate_function_arguments(struct vector *arguments)
{
    vector_set_peek_pointer(arguments, 0);
    struct node *argument_node = vector_peek_ptr(arguments);

    // First argument must be generated differently..
    if (argument_node)
    {
        codegen_generate_first_function_argument(argument_node);
        argument_node = vector_peek_ptr(arguments);
    }

    // Process the rest of the arguments
    while (argument_node)
    {
        codegen_generate_function_argument(argument_node);
        argument_node = vector_peek_ptr(arguments);
    }
}
void codegen_generate_function(struct node *node)
{
    asm_push("; %s function", node->func.name);
    asm_push("%s:", node->func.name);

    // We have to create a stack frame ;)
    asm_push("push ebp");
    asm_push("mov ebp, esp");

    // Generate scope for functon arguments
    codegen_scope_new();
    codegen_generate_function_arguments(node->func.argument_vector);

    // Generate the function body
    codegen_generate_function_body(node->func.body_n);

    // End function argument scope
    codegen_scope_finish();

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

void codegen_generate_data_section()
{
    asm_push("section .data");
    struct node *node = NULL;
    while ((node = node_next()) != NULL)
    {
        if (node->type == NODE_TYPE_VARIABLE)
        {
            codegen_generate_global_variable(node);
        }
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

int codegen(struct compile_process *process)
{
    current_process = process;
    // Create the root scope for this process
    scope_create_root(process);
    vector_set_peek_pointer(process->node_tree_vec, 0);
    codegen_generate_data_section();
    vector_set_peek_pointer(process->node_tree_vec, 0);
    codegen_generate_root();

    return 0;
}