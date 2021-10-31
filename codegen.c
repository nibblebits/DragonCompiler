#include "compiler.h"
#include "helpers/buffer.h"
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>

static struct compile_process *current_process;
static struct node *current_function;
// Returned when we have no expression state.
static struct expression_state blank_state = {};

#define codegen_err(...) \
    compiler_error(current_process, __VA_ARGS__)

struct history;
void asm_push(const char *ins, ...);
void codegen_gen_exp(struct generator *generator, struct node *node, int flags);
void codegen_entity_address(struct generator *generator, struct resolver_entity *entity, struct generator_entity_address *address_out);
void codegen_end_exp(struct generator *generator);
void codegen_restore_assignment_right_operand(const char *output_register);

struct _x86_generator_private
{
    // The data that has been saved as we transferred control
    // to a resource that doesnt understand our codegenerator internals.
    // Saved so we can access it later.
    struct x86_generator_remembered
    {
        struct history *history;
    } remembered;
} _x86_generator_private;

struct generator x86_codegen = {
    .asm_push = asm_push,
    .gen_exp = codegen_gen_exp,
    .end_exp = codegen_end_exp,
    .entity_address = codegen_entity_address,
    .private = &_x86_generator_private};

struct _x86_generator_private *x86_generator_private(struct generator *gen)
{
    return gen->private;
}

struct codegen_exit_point
{
    // The ID of this exit point.
    int id;
};

/**
 * Entry points represent the start of loop
 */
struct codegen_entry_point
{
    // The ID of this entry point;
    int id;
};

// Represents a history expression
struct history_exp
{
    // The operator for the start of this logical expression
    // I.e 50 || 20 && 90. Logical start operator would be ||
    const char *logical_start_op;
    char logical_end_label[20];
    char logical_end_label_positive[20];
};

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

        struct history_exp exp;
    };
};

enum
{
    CODEGEN_PREDICTION_EAX_WILL_BE_USED = 0b00000001,
    CODEGEN_PREDICTION_EBX_WILL_BE_USED = 0b00000010,
};

enum
{
    RESPONSE_TYPE_FUNCTION_CALL_ARGUMENT_PUSH
};

enum
{
    RESPONSE_FLAG_ACKNOWLEDGED = 0b00000001,
    RESPONSE_FLAG_PUSHED_STRUCTURE = 0b00000010,
};

#define RESPONSE_SET(x) (&(struct response){x})
#define RESPONSE_EMPTY RESPONSE_SET()

struct response_data
{
};

// Operations can pass results back up the stack.
struct response
{
    int flags;
    struct response_data data;
};

void codegen_response_expect()
{
    struct response *res = calloc(sizeof(struct response), 1);
    vector_push(current_process->generator->responses, &res);
}

struct response_data *codegen_response_data(struct response *response)
{
    return &response->data;
}

struct response *codegen_response_pull()
{
    struct response *res = vector_back_ptr_or_null(current_process->generator->responses);
    if (res)
    {
        vector_pop(current_process->generator->responses);
    }

    return res;
}

void codegen_response_acknowledge(struct response *response_in)
{
    struct response *res = vector_back_ptr_or_null(current_process->generator->responses);
    if (res)
    {
        memcpy(res, response_in, sizeof(struct response));
        res->flags |= RESPONSE_FLAG_ACKNOWLEDGED;
    }
}

bool codegen_response_acknowledged(struct response *res)
{
    return res && res->flags & RESPONSE_FLAG_ACKNOWLEDGED;
}

int codegen_label_count()
{
    static int count = 0;
    count++;
    return count;
}

bool codegen_is_exp_root_for_flags(int flags)
{
    return !(flags & EXPRESSION_IS_NOT_ROOT_NODE);
}
bool codegen_is_exp_root(struct history *history)
{
    return codegen_is_exp_root_for_flags(history->flags);
}

const char *codegen_get_label_for_string(const char *str);

/**
 * Registers the given string and returns the label name.
 * If already registered then just returns the label name
 */
const char *codegen_register_string(const char *str)
{

    // Already registered this string before? Why waste memory..
    const char *label = codegen_get_label_for_string(str);
    if (label)
    {
        return label;
    }

    struct string_table_element *str_elem = calloc(sizeof(struct string_table_element), 1);
    int label_id = codegen_label_count();
    sprintf((char *)str_elem->label, "str_%i", label_id);
    str_elem->str = str;
    vector_push(current_process->generator->string_table, &str_elem);
    return str_elem->label;
}

const char *codegen_get_label_for_string(const char *str)
{
    const char *result = NULL;
    vector_set_peek_pointer(current_process->generator->string_table, 0);
    struct string_table_element *current = vector_peek_ptr(current_process->generator->string_table);
    while (current)
    {
        if (S_EQ(current->str, str))
        {
            result = current->label;
        }
        current = vector_peek_ptr(current_process->generator->string_table);
    }

    return result;
}

static struct history *history_down(struct history *history, int flags)
{
    struct history *new_history = calloc(sizeof(struct history), 1);
    memcpy(new_history, history, sizeof(struct history));
    new_history->flags = flags;
    return new_history;
}

static struct history *history_begin(struct history *history_out, int flags)
{
    struct history *new_history = calloc(sizeof(struct history), 1);
    new_history->flags = flags;
    return new_history;
}

struct resolver_default_entity_data *codegen_entity_private(struct resolver_entity *entity)
{
    return resolver_default_entity_private(entity);
}

struct resolver_default_scope_data *codegen_scope_private(struct resolver_scope *scope)
{
    return resolver_default_scope_private(scope);
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
        else if (size == DATA_SIZE_DDWORD)
        {
            // Bit of an issue... If we use RAX
            // we will have trouble with 32 bit processor..
            reg = "rax";
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
        else if (size == DATA_SIZE_DDWORD)
        {
            reg = "rbx";
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
        else if (size == DATA_SIZE_DDWORD)
        {
            reg = "rcx";
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
        else if (size == DATA_SIZE_DDWORD)
        {
            reg = "rdx";
        }
    }
    return reg;
}

/**
 * Finds weather this is a byte operation, word operation or double word operation based on the size provided
 * Returns either "byte", "word", or "dword" 
 * 
 * Note: the reg_to_use pointer should be pointing to the register you intend to use. I.e "eax", "ebx". Only 32 bit registers accepted.
 * \param size The size of the data you are accessing
 * \param reg_to_use pointer to a const char pointer, this will be set to the register you should use for this operation. 32 bit register, 16 bit register or 8 bit register for full register eax, ebx, ecx, edx will be returned 
 */
const char *codegen_byte_word_or_dword_or_ddword(size_t size, const char **reg_to_use)
{
    const char *type = NULL;
    const char *new_register = *reg_to_use;
    if (size == DATA_SIZE_BYTE)
    {
        type = "byte";
        new_register = codegen_sub_register(*reg_to_use, DATA_SIZE_BYTE);
    }
    else if (size == DATA_SIZE_WORD)
    {
        type = "word";
        new_register = codegen_sub_register(*reg_to_use, DATA_SIZE_WORD);
    }
    else if (size == DATA_SIZE_DWORD)
    {
        type = "dword";
        new_register = codegen_sub_register(*reg_to_use, DATA_SIZE_DWORD);
    }
    else if (size == DATA_SIZE_DDWORD)
    {
        type = "ddword";
        new_register = codegen_sub_register(*reg_to_use, DATA_SIZE_DDWORD);
    }

    *reg_to_use = new_register;
    return type;
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

static struct resolver_default_entity_data *codegen_new_entity_data()
{
    return resolver_default_new_entity_data();
}

struct resolver_default_entity_data *codegen_new_entity_data_for_function(struct node *func_node, int flags)
{
    return resolver_default_new_entity_data_for_function(func_node, flags);
}

struct resolver_entity *codegen_new_scope_entity(struct node *var_node, int offset, int flags)
{
    return resolver_default_new_scope_entity(current_process->resolver, var_node, offset, flags);
}

struct resolver_entity *codegen_register_function(struct node *func_node, int flags)
{
    return resolver_default_register_function(current_process->resolver, func_node, flags);
}

void codegen_new_scope(int flags)
{
    resolver_default_new_scope(current_process->resolver, flags);
}

void codegen_finish_scope()
{
    resolver_default_finish_scope(current_process->resolver);
}

/**
 * Returns true if the register must be saved to the stack
 * as this combination of nodes risks corruption of the register, therefore
 * if this function returns true, save the affected register and restore later
 */
static bool codegen_must_save_restore(struct node *node)
{
    return !is_access_node(node->exp.left) &&
           node->exp.left->type == NODE_TYPE_EXPRESSION &&
           node->exp.right->type == NODE_TYPE_EXPRESSION;
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
    return current_process->generator->used_registers & codegen_get_enum_for_register(reg);
}

void register_set_flag(int flag)
{
    current_process->generator->used_registers |= flag;
}

void register_unset_flag(int flag)
{
    current_process->generator->used_registers &= ~flag;
}

void codegen_use_register(const char *reg)
{
    register_set_flag(codegen_get_enum_for_register(reg));
}

void codegen_release_register(const char *reg)
{
    register_unset_flag(codegen_get_enum_for_register(reg));
}

void asm_push_no_nl(const char *ins, ...)
{
    va_list args;
    va_start(args, ins);
    vfprintf(stdout, ins, args);
    va_end(args);

    if (current_process->ofile)
    {
        va_list args;
        va_start(args, ins);
        vfprintf(current_process->ofile, ins, args);
        va_end(args);
    }
}

void asm_push_args(const char *ins, va_list args)
{
    va_list args2;
    va_copy(args2, args);
    vfprintf(stdout, ins, args);
    fprintf(stdout, "\n");
    if (current_process->ofile)
    {

        vfprintf(current_process->ofile, ins, args2);
        fprintf(current_process->ofile, "\n");
    }
}
void asm_push(const char *ins, ...)
{
    va_list args;
    va_start(args, ins);
    asm_push_args(ins, args);
    va_end(args);
}

void asm_push_ins_push(const char *fmt, int stack_entity_type, const char *stack_entity_name, ...)
{
    char tmp_buf[200];
    sprintf(tmp_buf, "push %s", fmt);
    va_list args;
    va_start(args, stack_entity_name);
    asm_push_args(tmp_buf, args);
    va_end(args);

    // Let's add it to the stack frame for compiler referencing
    assert(current_function);
    stackframe_push(current_function, &(struct stack_frame_element){.type = stack_entity_type, .name = stack_entity_name});
}

void asm_push_ins_pop(const char *fmt, int expecting_stack_entity_type, const char *expecting_stack_entity_name, ...)
{
    char tmp_buf[200];
    sprintf(tmp_buf, "pop %s", fmt);
    va_list args;
    va_start(args, expecting_stack_entity_name);
    asm_push_args(tmp_buf, args);
    va_end(args);

    // Let's add it to the stack frame for compiler referencing
    assert(current_function);
    stackframe_pop_expecting(current_function, expecting_stack_entity_type, expecting_stack_entity_name);
}

void asm_push_ebp()
{
    asm_push_ins_push("ebp", STACK_FRAME_ELEMENT_TYPE_SAVED_BP, "function_entry_saved_ebp");
}

void asm_pop_ebp()
{
    asm_push_ins_pop("ebp", STACK_FRAME_ELEMENT_TYPE_SAVED_BP, "function_entry_saved_ebp");
}

void asm_pop_ebp_no_stack_frame_restore()
{
    asm_push("pop ebp");
}

void codegen_stack_sub(size_t stack_size)
{
    if (stack_size != 0)
    {
        stackframe_sub(current_function, STACK_FRAME_ELEMENT_TYPE_UNKNOWN, "stack_subtraction", stack_size);
        asm_push("sub esp, %lld", stack_size);
    }
}

void codegen_stack_add_no_compile_time_stack_frame_restore(size_t stack_size)
{
    if (stack_size != 0)
    {
        asm_push("add esp, %lld", stack_size);
    }
}

void codegen_stack_add(size_t stack_size)
{
    if (stack_size != 0)
    {
        stackframe_add(current_function, stack_size);
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
        sprintf(tmp_buf, "times %lld db ", (unsigned long long)size);
        return tmp_buf;
    }

    stpcpy(tmp_buf, keyword);
    return tmp_buf;
}

static struct node *codegen_node_next()
{
    return vector_peek_ptr(current_process->node_tree_vec);
}

bool codegen_write_string_char_escaped(char c)
{
    const char *c_out = NULL;
    switch (c)
    {
    case '\n':
        c_out = "10";
        break;

    case '\t':
    {
        c_out = "9";
        break;
    }
    };

    if (c_out)
    {
        asm_push_no_nl("%s, ", c_out);
    }
    return c_out != NULL;
}

void codegen_write_string(struct string_table_element *str_elem)
{

    asm_push_no_nl("%s: db ", str_elem->label);

    // We must loop through the string and output each character
    // Some have special features
    size_t len = strlen(str_elem->str);
    for (int i = 0; i < len; i++)
    {
        char c = str_elem->str[i];
        bool handled = codegen_write_string_char_escaped(c);
        if (handled)
        {
            continue;
        }
        asm_push_no_nl("'%c',", c);
    }

    // End this with a null terminator
    asm_push_no_nl(" 0");
    asm_push("");
}

void codegen_write_strings()
{
    vector_set_peek_pointer(current_process->generator->string_table, 0);
    struct string_table_element *current = vector_peek_ptr(current_process->generator->string_table);
    while (current)
    {
        codegen_write_string(current);
        current = vector_peek_ptr(current_process->generator->string_table);
    }
}

/**
 * Generates the read only data section
 */
void codegen_generate_rod()
{
    asm_push("section .rodata");
    codegen_write_strings();
}

void codegen_generate_node(struct node *node);
void codegen_generate_expressionable(struct node *node, struct history *history);
void codegen_generate_new_expressionable(struct node *node, struct history *history)
{
    codegen_generate_expressionable(node, history);
}

void codegen_gen_exp(struct generator *generator, struct node *node, int flags)
{
    codegen_generate_expressionable(node, history_down(x86_generator_private(generator)->remembered.history, flags));
}

void codegen_end_exp(struct generator *generator)
{
}

void codegen_entity_address(struct generator *generator, struct resolver_entity *entity, struct generator_entity_address *address_out)
{
    struct resolver_default_entity_data *data = codegen_entity_private(entity);
    address_out->address = data->address;
    address_out->base_address = data->base_address;
    address_out->is_stack = data->flags & RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK;
    address_out->offset = data->offset;
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

void codegen_register_exit_point(int exit_point_id)
{
    struct code_generator *gen = current_process->generator;
    struct codegen_exit_point *exit_point = calloc(sizeof(struct codegen_exit_point), 1);
    exit_point->id = exit_point_id;
    vector_push(gen->exit_points, &exit_point);
}

struct codegen_exit_point *codegen_current_exit_point()
{
    struct code_generator *gen = current_process->generator;
    return vector_back_ptr_or_null(gen->exit_points);
}

void codegen_begin_exit_point()
{
    int exit_point_id = codegen_label_count();
    codegen_register_exit_point(exit_point_id);
}

void codegen_end_exit_point()
{
    struct code_generator *gen = current_process->generator;
    struct codegen_exit_point *exit_point = codegen_current_exit_point();
    asm_push(".exit_point_%i:", exit_point->id);
    assert(exit_point);
    free(exit_point);
    vector_pop(gen->exit_points);
}

void codegen_goto_exit_point_maintain_stack(struct node *node)
{
    struct code_generator *gen = current_process->generator;
    struct codegen_exit_point *exit_point = codegen_current_exit_point();
    asm_push("jmp .exit_point_%i", exit_point->id);
}

void codegen_goto_exit_point(struct node *current_node)
{
    struct code_generator *gen = current_process->generator;
    struct codegen_exit_point *exit_point = codegen_current_exit_point();
    asm_push("jmp .exit_point_%i", exit_point->id);
}

void codegen_register_entry_point(int entry_point_id)
{
    struct code_generator *gen = current_process->generator;
    struct codegen_entry_point *entry_point = calloc(sizeof(struct codegen_entry_point), 1);
    entry_point->id = entry_point_id;
    vector_push(gen->entry_points, &entry_point);
}

struct codegen_entry_point *codegen_current_entry_point()
{
    struct code_generator *gen = current_process->generator;
    return vector_back_ptr_or_null(gen->entry_points);
}

void codegen_begin_entry_point()
{
    int entry_point_id = codegen_label_count();
    codegen_register_entry_point(entry_point_id);
    asm_push(".entry_point_%i:", entry_point_id);
}

void codegen_end_entry_point()
{
    struct code_generator *gen = current_process->generator;
    struct codegen_entry_point *entry_point = codegen_current_entry_point();
    assert(entry_point);
    free(entry_point);
    vector_pop(gen->entry_points);
}

void codegen_goto_entry_point(struct node *current_node)
{
    struct code_generator *gen = current_process->generator;
    struct codegen_entry_point *entry_point = codegen_current_entry_point();
    asm_push("jmp .entry_point_%i", entry_point->id);
}

void codegen_begin_entry_exit_point()
{
    codegen_begin_entry_point();
    codegen_begin_exit_point();
}

void codegen_end_entry_exit_point()
{
    codegen_end_entry_point();
    codegen_end_exit_point();
}

void codegen_begin_switch_statement()
{
    struct code_generator *generator = current_process->generator;
    struct generator_switch_stmt *switch_stmt_data = &generator->_switch;

    // Let's push the current switch statement to our stack so we can restore it later
    vector_push(switch_stmt_data->switches, &switch_stmt_data->current);
    // Now we can overwrite the current switch with our own.
    memset(&switch_stmt_data->current, 0, sizeof(struct generator_switch_stmt_entity));
    int switch_stmt_id = codegen_label_count();
    asm_push(".switch_stmt_%i:", switch_stmt_id);
    switch_stmt_data->current.id = switch_stmt_id;
}

void codegen_end_switch_statement()
{
    struct code_generator *generator = current_process->generator;
    struct generator_switch_stmt *switch_stmt_data = &generator->_switch;
    asm_push(".switch_stmt_%i_end:", switch_stmt_data->current.id);
    // Let's restore the old switch statement
    memcpy(&switch_stmt_data->current, vector_back(switch_stmt_data->switches), sizeof(struct generator_switch_stmt_entity));
    vector_pop(switch_stmt_data->switches);
}

int codegen_switch_id()
{
    struct code_generator *generator = current_process->generator;
    struct generator_switch_stmt *switch_stmt_data = &generator->_switch;
    return switch_stmt_data->current.id;
}

void codegen_begin_case_statement(int index)
{
    struct code_generator *generator = current_process->generator;
    struct generator_switch_stmt *switch_stmt_data = &generator->_switch;
    asm_push(".switch_stmt_%i_case_%i:", switch_stmt_data->current.id, index);
}

void codegen_end_case_statement()
{
    // Do nothing.
}

void codegen_gen_cmp(const char *value, const char *set_ins)
{
    asm_push("cmp eax, %s", value);
    asm_push("%s al", set_ins);
    asm_push("movzx eax, al");
}

void codegen_gen_math_for_value(const char *reg, const char *value, int flags)
{
    if (flags & EXPRESSION_IS_ADDITION)
    {
        asm_push("add %s, %s", reg, value);
    }
    else if (flags & EXPRESSION_IS_SUBTRACTION)
    {
        asm_push("sub %s, %s", reg, value);
    }
    else if (flags & EXPRESSION_IS_MULTIPLICATION)
    {
        codegen_use_register("ecx");
        asm_push("mov ecx, %s", value);

        // Need a way to know if its signed in the future.. Assumed all signed for now.
        asm_push("imul ecx");
    }
    else if (flags & EXPRESSION_IS_DIVISION)
    {
        codegen_use_register("ecx");
        asm_push("mov ecx, %s", value);
        asm_push("cdq");
        // Assuming signed, check for unsigned in the future, check for float in the future..
        asm_push("idiv ecx");
    }
    else if (flags & EXPRESSION_IS_ABOVE)
    {
        codegen_gen_cmp(value, "setg");
    }
    else if (flags & EXPRESSION_IS_BELOW)
    {
        codegen_gen_cmp(value, "setl");
    }
    else if (flags & EXPRESSION_IS_EQUAL)
    {
        codegen_gen_cmp(value, "sete");
    }
    else if (flags & EXPRESSION_IS_ABOVE_OR_EQUAL)
    {
        codegen_gen_cmp(value, "setge");
    }
    else if (flags & EXPRESSION_IS_BELOW_OR_EQUAL)
    {
        codegen_gen_cmp(value, "setle");
    }
    else if (flags & EXPRESSION_IS_NOT_EQUAL)
    {
        codegen_gen_cmp(value, "setne");
    }
    else if (flags & EXPRESSION_IS_BITSHIFT_LEFT)
    {
        asm_push("sal %s, %s", reg, value);
    }
    else if (flags & EXPRESSION_IS_BITSHIFT_RIGHT)
    {
        asm_push("sar %s, %s", reg, value);
    }
    else if (flags & EXPRESSION_IS_BITWISE_AND)
    {
        asm_push("and %s, %s", reg, value);
    }
    else if (flags & EXPRESSION_IS_BITWISE_OR)
    {
        asm_push("or %s, %s", reg, value);
    }
    else if (flags & EXPRESSION_IS_BITWISE_XOR)
    {
        asm_push("xor %s, %s", reg, value);
    }
}

static void codegen_gen_mov_or_math_for_value(const char *reg, const char *value, const char *datatype, int flags)
{
    if (register_is_used(reg))
    {
        codegen_gen_math_for_value(reg, value, flags);
        return;
    }

    codegen_use_register(reg);
    asm_push("mov %s, %s", reg, value);
}

static void codegen_gen_mov_or_math(const char *reg, struct node *value_node, int flags, struct resolver_entity *entity)
{
    // We possibly need a subregister
    const char *mov_type_keyword = "dword";
    const char *reg_to_use = reg;

    if (flags & EXPRESSION_IS_ASSIGNMENT)
    {
        mov_type_keyword = codegen_byte_word_or_dword_or_ddword(datatype_element_size(&variable_node(entity->node)->var.type), &reg_to_use);
    }
    codegen_gen_mov_or_math_for_value(reg_to_use, codegen_get_fmt_for_value(value_node, entity), mov_type_keyword, flags);
}

static void codegen_gen_mem_access_get_address(struct node *value_node, int flags, struct resolver_entity *entity)
{
    if (flags & EXPRESSION_INDIRECTION)
    {
        codegen_use_register("ebx");
        // We have indirection, therefore we should load the address into EBX.
        // rather than mov instruction
        asm_push("mov ebx, [%s]", codegen_entity_private(entity)->address);
        return;
    }

    asm_push("lea ebx, [%s]", codegen_entity_private(entity)->address);
}

static void codegen_gen_mem_access_first_for_expression(struct node *value_node, int flags, struct resolver_entity *entity)
{
    const char *reg_to_use = "eax";
    // This is the first node therefore, as we are accessing a variable
    // it is very important that we get a subregister to prevent us loading in
    // other bytes that might not be required as part of this expression
    // For example if we have one byte on the stack and we load the full integer into EAX
    // then we will also load the value of the other registers into the expression result
    // not something that we want at all

    reg_to_use = codegen_sub_register("eax", datatype_element_size(&entity->node->var.type));
    assert(reg_to_use);
    if (!S_EQ(reg_to_use, "eax"))
    {
        // Okay we are not using the full 32 bit, lets XOR this thing we need 0s.
        // No corruption that way
        asm_push("xor eax, eax");
    }

    codegen_gen_mov_or_math(reg_to_use, value_node, flags, entity);
}

static void codegen_gen_mem_access_for_continueing_expression(struct node *value_node, int flags, struct resolver_entity *entity)
{
    const char *new_reg_to_use = codegen_sub_register("ecx", datatype_element_size(&entity->node->var.type));
    if (!S_EQ(new_reg_to_use, "ecx"))
    {
        // Okay we are not using the full 32 bit, lets XOR this thing we need 0s.
        // No corruption that way
        //asm_push("xor eax, eax");

        // Okay lets use ECX here and we will need to preform the operation later.
        asm_push("xor ecx, ecx");
        asm_push("mov %s, %s", new_reg_to_use, codegen_get_fmt_for_value(value_node, entity));
        codegen_gen_math_for_value("eax", "ecx", flags);
        return;
    }

    // Is this a full 4 byte value, then their is no need to do anything special
    // generate a normal expression
    codegen_gen_mov_or_math("eax", value_node, flags, entity);
}

static void codegen_gen_mem_access(struct node *value_node, int flags, struct resolver_entity *entity)
{
    if (flags & EXPRESSION_GET_ADDRESS)
    {
        codegen_gen_mem_access_get_address(value_node, flags, entity);
        return;
    }
    // Register is not used? Okay then we need to ensure EAX does not get corrupted
    // with additional data on the stack, lets XOR the eax and use what we care about, not the
    // whole register unless the variable size is 4 bytes.
    if (!register_is_used("eax"))
    {
        codegen_gen_mem_access_first_for_expression(value_node, flags, entity);
        return;
    }

    codegen_gen_mem_access_for_continueing_expression(value_node, flags, entity);
}
/**
 * For literal numbers
 */
void codegen_generate_number_node(struct node *node, struct history *history)
{
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

void codegen_generate_variable_access_for_pointer_array_part(struct resolver_entity *entity, struct history *history)
{
    codegen_generate_expressionable(entity->var_data.array_runtime.index_node, history);
    register_unset_flag(REGISTER_EAX_IS_USED);
    if (resolver_entity_has_array_multiplier(entity))
    {
        asm_push("imul eax, %i", entity->var_data.array_runtime.multiplier);
    }
    asm_push("mov ecx, [ebx]");
    asm_push("add eax, ecx");
}

void codegen_generate_variable_access_for_pointer_array(struct resolver_entity *entity, struct history *history)
{
    asm_push("mov ebx,[%s]", codegen_entity_private(entity)->address);
    codegen_generate_expressionable(entity->var_data.array_runtime.index_node, history);
    register_unset_flag(REGISTER_EAX_IS_USED);
    if (resolver_entity_has_array_multiplier(entity))
    {
        asm_push("imul eax, %i", entity->var_data.array_runtime.multiplier);
    }

    struct resolver_entity *next_entity = resolver_result_entity_next(entity);
    bool had_next_entity = next_entity != NULL;
    while (next_entity)
    {
        codegen_generate_variable_access_for_pointer_array_part(next_entity, history);
        next_entity = resolver_result_entity_next(next_entity);
    }

    if (!had_next_entity)
    {
        asm_push("mov ebx, [ebx+eax]");
        asm_push("mov eax, ebx");
    }
    else
    {
        asm_push("mov eax, [eax]");
    }
}

void codegen_generate_variable_access_for_array_final_non_pointer_calculation(int count, struct resolver_entity *last_entity, struct history *history)
{
    // No count then nothing to do.
    if (count == 0)
        return;

    asm_push("mov eax, 0");
    for (int i = 0; i < count; i++)
    {
        asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "array_pushed_computed_offset");
        asm_push("add eax, ecx");
    }

    if (history->flags & EXPRESSION_GET_ADDRESS)
    {
        asm_push("lea ebx, [%s+eax]", codegen_entity_private(last_entity)->address);
    }
    else
    {
        asm_push("mov eax, [%s+eax]", codegen_entity_private(last_entity)->address);
    }
}

void codegen_generate_structure_push(struct node *node, struct resolver_entity *entity, struct history *history)
{
    asm_push("; STRUCTURE PUSH");
    size_t structure_size = align_value(entity->var_data.dtype.size, DATA_SIZE_DWORD);
    int pushes = structure_size / DATA_SIZE_DWORD;

    for (int i = pushes - 1; i >= 0; i--)
    {
        struct resolver_default_entity_data *private = codegen_entity_private(entity);
        int chunk_offset = private->offset + (i * DATA_SIZE_DWORD);
        asm_push_ins_push("dword [%s%i]", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "structure_part_pushed_to_stack", private->base_address, chunk_offset);
    }
    asm_push("; END STRUCTURE PUSH");

    codegen_response_acknowledge(RESPONSE_SET(.flags = RESPONSE_FLAG_PUSHED_STRUCTURE));
}

void codegen_generate_variable_access_for_entity(struct node *node, struct resolver_entity *entity, struct history *history)
{
    // if (datatype_is_non_pointer_struct(&entity->var_data.dtype))
    // {
    //     codegen_generate_structure_push(node, entity, history);
    //     return;
    // }

    codegen_gen_mem_access(node, history->flags, entity);
}

void codegen_restore_assignment_right_operand(const char *output_register)
{
    struct stack_frame_element *last_stack_push = stackframe_back_expect(current_function, STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "assignment_right_operand");
    if (last_stack_push)
    {
        asm_push_ins_pop(output_register, STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "assignment_right_operand");
    }
}
void codegen_generate_variable_access(struct node *node, struct resolver_entity *entity, struct history *history)
{
    codegen_generate_variable_access_for_entity(node, entity, history_down(history, history->flags));
}

void codegen_generate_assignment_instruction_for_operator(const char *mov_type_keyword, const char *address, const char *reg_to_use, const char *op, bool is_signed)
{
    if (S_EQ(op, "="))
    {
        asm_push("mov %s [%s], %s", mov_type_keyword, address, reg_to_use);
    }
    else if (S_EQ(op, "+="))
    {
        asm_push("add %s [%s], %s", mov_type_keyword, address, reg_to_use);
    }
    else if (S_EQ(op, "*="))
    {
        if (is_signed)
        {
            asm_push("imul %s", reg_to_use);
        }
        else
        {
            asm_push("mul %s", reg_to_use);
        }
        asm_push("mov %s [%s], %s", mov_type_keyword, address, reg_to_use);
    }
    else if (S_EQ(op, "/="))
    {
        if (is_signed)
        {
            asm_push("idiv %s", reg_to_use);
        }
        else
        {
            asm_push("div %s", reg_to_use);
        }

        asm_push("mov %s [%s], %s", mov_type_keyword, address, reg_to_use);
    }
    else if (S_EQ(op, "<<="))
    {
        // not tested.
        asm_push("shl eax, ", reg_to_use);
        asm_push("mov %s [%s], %s", mov_type_keyword, address, reg_to_use);
    }
    else if (S_EQ(op, ">>="))
    {
        // not tested.
        asm_push("shr eax, ", reg_to_use);
        asm_push("mov %s [%s], %s", mov_type_keyword, address, reg_to_use);
    }
}

void codegen_generate_assignment_expression_value_part(struct resolver_result *result, struct resolver_entity *left_entity, struct node *left_node, struct node *right_node, const char *op, struct history *history)
{
    codegen_generate_expressionable(right_node, history_down(history, history->flags));
    register_unset_flag(REGISTER_EAX_IS_USED);
    asm_push_ins_push("eax", STACK_FRAME_ELEMENT_TYPE_SAVED_REGISTER, "right_assignment_operand");

    const char *reg_to_use = "eax";
    const char *mov_type_keyword = codegen_byte_word_or_dword_or_ddword(datatype_size(&variable_node(left_entity->node)->var.type), &reg_to_use);

    codegen_generate_expressionable(left_node, history_down(history, history->flags | EXPRESSION_GET_ADDRESS));
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_SAVED_REGISTER, "right_assignment_operand");
    asm_push("mov [ebx], eax");
    register_unset_flag(REGISTER_EAX_IS_USED);
}

void codegen_generate_entity_access_array_bracket(struct resolver_result *result, struct resolver_entity *entity)
{
    struct history history = {};
    // We have an array bracket that needs to be computed at runtime..
 
    codegen_generate_expressionable(entity->array.array_index_node, history_begin(&history, 0));
    if (entity->flags & RESOLVER_ENTITY_FLAG_JUST_USE_OFFSET)
    {
        // Resolver has spoken.. Just use the offset as its completley computed already...
        asm_push("add ebx, %i", entity->offset);
    }
    else
    {
        asm_push("imul eax, %i", entity->offset);
        asm_push("add ebx, eax");

    }
    register_unset_flag(REGISTER_EAX_IS_USED);
}

void codegen_generate_entity_access_for_variable_or_general(struct resolver_result *result, struct resolver_entity *entity)
{
    if (entity->flags & RESOLVER_ENTITY_FLAG_DO_INDIRECTION)
    {
        asm_push("mov ebx, [ebx]");
    }
    asm_push("add ebx, %i", entity->offset);
}
void codegen_generate_entity_access_for_entity(struct resolver_result *result, struct resolver_entity *entity)
{
    switch (entity->type)
    {
    case RESOLVER_ENTITY_TYPE_ARRAY_BRACKET:
        codegen_generate_entity_access_array_bracket(result, entity);
        break;

    case RESOLVER_ENTITY_TYPE_VARIABLE:
    case RESOLVER_ENTITY_TYPE_GENERAL:
        codegen_generate_entity_access_for_variable_or_general(result, entity);
        break;

    default:
        compiler_error(current_process, "COMPILER BUG...");
    }
}
void codegen_generate_entity_access(struct resolver_result *result, struct resolver_entity *root_assignment_entity)
{
    asm_push("lea ebx, [%s]", result->base.address);
    struct resolver_entity *current = resolver_result_entity_next(root_assignment_entity);
    while (current)
    {
        codegen_generate_entity_access_for_entity(result, current);
        current = resolver_result_entity_next(current);
    }
}
void codegen_generate_assignment_expression(struct node *node, struct history *history)
{
    // Left node = to assign
    // Right node = value

    // Let's create the value
    codegen_generate_expressionable(node->exp.right, history_down(history, 0));
    asm_push_ins_push("eax", STACK_FRAME_ELEMENT_TYPE_SAVED_REGISTER, "assignment_eax_right_value_save");

    register_unset_flag(REGISTER_EAX_IS_USED);
    struct resolver_result *result = resolver_follow(current_process->resolver, node->exp.left);
    assert(!resolver_result_failed(result));

    struct resolver_entity *root_assignment_entity = resolver_result_entity_root(result);
    codegen_generate_entity_access(result, root_assignment_entity);

    const char *reg_to_use = "eax";
    const char *mov_type_keyword =
        codegen_byte_word_or_dword_or_ddword(datatype_element_size(&result->last_entity->dtype), &reg_to_use);

    // Restore the value
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_SAVED_REGISTER, "assignment_eax_right_value_save");
    // All we do now is assign the value..
    asm_push("mov %s [ebx], %s", mov_type_keyword, reg_to_use);
}

void codegen_generate_expressionable_function_arguments(struct node *func_call_args_exp_node, size_t *arguments_size)
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
        flag |= EXPRESSION_IS_DIVISION;
    }
    else if (S_EQ(op, ">"))
    {
        flag |= EXPRESSION_IS_ABOVE;
    }
    else if (S_EQ(op, "<"))
    {
        flag |= EXPRESSION_IS_BELOW;
    }
    else if (S_EQ(op, ">="))
    {
        flag |= EXPRESSION_IS_ABOVE_OR_EQUAL;
    }
    else if (S_EQ(op, "<="))
    {
        flag |= EXPRESSION_IS_BELOW_OR_EQUAL;
    }
    else if (S_EQ(op, "!="))
    {
        flag |= EXPRESSION_IS_NOT_EQUAL;
    }
    else if (S_EQ(op, "&&"))
    {
        flag |= EXPRESSION_LOGICAL_AND;
    }
    else if (S_EQ(op, "<<"))
    {
        flag |= EXPRESSION_IS_BITSHIFT_LEFT;
    }
    else if (S_EQ(op, ">>"))
    {
        flag |= EXPRESSION_IS_BITSHIFT_RIGHT;
    }
    else if (S_EQ(op, "&"))
    {
        flag |= EXPRESSION_IS_BITWISE_AND;
    }
    else if (S_EQ(op, "|"))
    {
        flag |= EXPRESSION_IS_BITWISE_OR;
    }
    else if (S_EQ(op, "^"))
    {
        flag |= EXPRESSION_IS_BITWISE_XOR;
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

void codegen_generate_logical_cmp_and(const char *reg, const char *fail_label)
{
    // THis can be optimized to more appropiate instructions.
    asm_push("cmp %s, 0", reg);
    asm_push("je %s", fail_label);
}

void codegen_generate_logical_cmp_or(const char *reg, const char *equal_label)
{
    // THis can be optimized to more appropiate instructions.
    asm_push("cmp %s, 0", reg);
    asm_push("jg %s", equal_label);
}

void codegen_generate_logical_cmp(const char *op, const char *fail_label, const char *equal_label)
{
    if (S_EQ(op, "&&"))
    {
        codegen_generate_logical_cmp_and("eax", fail_label);
    }
    else if (S_EQ(op, "||"))
    {
        codegen_generate_logical_cmp_or("eax", equal_label);
    }
}

void codegen_generate_end_labels_for_logical_expression(const char *op, const char *end_label, const char *end_label_positive)
{
    if (S_EQ(op, "&&"))
    {
        asm_push("; && END CLAUSE");
        // This result was true, set the EAX register
        asm_push("mov eax, 1");
        asm_push("jmp %s", end_label_positive);
        // Time to end this expression
        asm_push("%s:", end_label);
        // Result was false
        asm_push("xor eax, eax");
        asm_push("%s:", end_label_positive);
    }
    else if (S_EQ(op, "||"))
    {
        asm_push("; || END CLAUSE");
        // Catch all, if we get here with no TRUE result then its false.
        asm_push("jmp %s", end_label);
        asm_push("%s:", end_label_positive);
        // True result
        asm_push("mov eax, 1");
        asm_push("%s:", end_label);
    }
}

void codegen_setup_new_logical_expression(struct history *history, struct node *node)
{
    bool start_of_logical_exp = !(history->flags & EXPRESSION_IN_LOGICAL_EXPRESSION);
    char *end_label = history->exp.logical_end_label;
    char *end_label_positive = history->exp.logical_end_label_positive;

    int label_index = codegen_label_count();
    sprintf(history->exp.logical_end_label, ".endc_%i", label_index);
    sprintf(history->exp.logical_end_label_positive, ".endc_%i_positive", label_index);
    history->exp.logical_start_op = node->exp.op;
    end_label = history->exp.logical_end_label;
    end_label_positive = history->exp.logical_end_label_positive;
    history->flags |= EXPRESSION_IN_LOGICAL_EXPRESSION;
}

void codegen_generate_exp_node_for_logical_arithmetic(struct node *node, struct history *history)
{
    // We have a logical operator i.e && or ||
    bool start_of_logical_exp = !(history->flags & EXPRESSION_IN_LOGICAL_EXPRESSION);
    if (start_of_logical_exp)
    {
        codegen_setup_new_logical_expression(history, node);
    }

    codegen_generate_expressionable(node->exp.left, history);
    codegen_generate_logical_cmp(node->exp.op, history->exp.logical_end_label, history->exp.logical_end_label_positive);
    register_unset_flag(REGISTER_EAX_IS_USED);
    codegen_generate_expressionable(node->exp.right, history);
    register_unset_flag(REGISTER_EAX_IS_USED);
    if (!is_logical_node(node->exp.right))
    {
        codegen_generate_logical_cmp(node->exp.op, history->exp.logical_end_label, history->exp.logical_end_label_positive);
    }

    if (!is_logical_node(node->exp.right))
    {
        codegen_generate_end_labels_for_logical_expression(node->exp.op, history->exp.logical_end_label, history->exp.logical_end_label_positive);
    }
}

void codegen_generate_exp_node_for_arithmetic(struct node *node, struct history *history)
{
    assert(node->type == NODE_TYPE_EXPRESSION);

    int flags = history->flags;

    if (is_logical_operator(node->exp.op))
    {
        codegen_generate_exp_node_for_logical_arithmetic(node, history);
        return;
    }

    struct node *left_node = node->exp.left;
    struct node *right_node = node->exp.right;
    bool must_save_restore = codegen_must_save_restore(node);

    // For now right node priority is false due to bugs.
    bool right_node_priority = node_is_expression_or_parentheses(node->exp.right);

    // When the right node is an expression is takes full priority
    if (right_node_priority)
    {
        codegen_generate_expressionable(node->exp.right, history_down(history, flags));
    }

    // If both left and right node are expressions they will break the assembly flow
    // we must push and restore later on the EAX register
    if (must_save_restore)
    {
        asm_push_ins_push("eax", STACK_FRAME_ELEMENT_TYPE_SAVED_REGISTER, "arithmetic_save_to_stack");
        register_unset_flag(REGISTER_EAX_IS_USED);
    }

    // We need to set the correct flag regarding which operator is being used
    flags |= codegen_set_flag_for_operator(node->exp.op);
    codegen_generate_expressionable(left_node, history_down(history, flags));
    if (!right_node_priority)
    {
        codegen_generate_expressionable(right_node, history_down(history, flags | EXPRESSION_FLAG_RIGHT_NODE));
    }

    if (must_save_restore)
    {
        asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_SAVED_REGISTER, "arithmetic_save_to_stack");
        codegen_gen_math_for_value("ecx", "eax", flags);
    }
}

bool codegen_should_push_function_call_argument(struct response *res)
{
    return !codegen_response_acknowledged(res) || !(res->flags & RESPONSE_FLAG_PUSHED_STRUCTURE);
}

void codegen_generate_function_call_for_native(struct symbol *native_func_sym, struct node *node, struct resolver_entity *entity, struct history *history)
{
    // Okay lets call the function
    struct native_function *func = native_func_sym->data;
    x86_generator_private(&x86_codegen)->remembered.history = history;
    func->callbacks.call(&x86_codegen, node->binded.function, func, entity->func_call_data.arguments);
}

void codegen_generate_function_call(struct node *node, struct resolver_entity *entity, struct history *history)
{
    // Is this function call a native one? If so this is a different game
    struct symbol *native_func_sym = symresolver_get_symbol_for_native_function(current_process, entity->name);
    if (native_func_sym)
    {
        codegen_generate_function_call_for_native(native_func_sym, node, entity, history);
        return;
    }

    // Ok we have a function call entity lets generate it. First we must
    // process all function arguments before we can call the function
    // We must also process them backwards because of the stack
    vector_set_peek_pointer_end(entity->func_call_data.arguments);
    vector_set_flag(entity->func_call_data.arguments, VECTOR_FLAG_PEEK_DECREMENT);

    struct node *argument_node = vector_peek_ptr(entity->func_call_data.arguments);
    while (argument_node)
    {
        codegen_response_expect();
        register_unset_flag(REGISTER_EAX_IS_USED);
        codegen_generate_expressionable(argument_node, history_down(history, codegen_remove_uninheritable_flags(history->flags) | EXPRESSION_IN_FUNCTION_CALL));

        struct response *res = codegen_response_pull();
        if (codegen_should_push_function_call_argument(res))
        {
            // At this point EAX will contain the result of the argument, lets push it to the stack
            // ready for the function call
            asm_push_ins_push("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "pushed_function_argument");
        }

        argument_node = vector_peek_ptr(entity->func_call_data.arguments);
    }

    // Done with the arguments? Great lets initiate the function call
    asm_push("call %s", entity->name);

    // Now to restore the stack
    codegen_stack_add(entity->func_call_data.stack_size);

    register_unset_flag(REGISTER_EAX_IS_USED);
}

void codegen_generate_array_entity(struct node *node, struct resolver_entity *entity, struct history *history)
{
    asm_push_ins_push("ebx", STACK_FRAME_ELEMENT_TYPE_SAVED_REGISTER, "array_entity_ebx_save");
    register_unset_flag(REGISTER_EAX_IS_USED);
    codegen_generate_expressionable(entity->array.array_index_node, history);
    asm_push("mov edx, eax");
    asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_SAVED_REGISTER, "array_entity_ebx_save");
    asm_push("imul edx, %i", entity->array.multiplier * datatype_element_size(&entity->array.dtype));
    asm_push("add ebx, edx");
}

void codegen_generate_for_entity(struct node *node, struct resolver_entity *entity, struct history *history)
{
    switch (entity->type)
    {
    case RESOLVER_ENTITY_TYPE_VARIABLE:
        codegen_generate_variable_access(node, entity, history);
        break;

    case RESOLVER_ENTITY_TYPE_FUNCTION_CALL:
        codegen_generate_function_call(node, entity, history);
        break;

    default:
        FAIL_ERR("Invalid entity to generate, check supported type");
    }
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
    if (resolver_result_ok(result))
    {
        struct resolver_entity *root_assignment_entity = resolver_result_entity_root(result);
        codegen_generate_entity_access(result, root_assignment_entity);

        const char *reg_to_use = "eax";
        const char *mov_type_keyword =
            codegen_byte_word_or_dword_or_ddword(datatype_element_size(&result->last_entity->dtype), &reg_to_use);
        asm_push("mov eax, [ebx]");
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

void codegen_generate_identifier(struct node *node, struct history *history)
{

    struct resolver_result *result = resolver_follow(current_process->resolver, node);
    assert(resolver_result_ok(result));

    struct resolver_entity *entity = resolver_result_entity(result);
    codegen_generate_variable_access(node, entity, history);
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
    codegen_use_register("ebx");
    for (int i = 0; i < node->unary.indirection.depth; i++)
    {
        asm_push("mov ebx, [ebx]");
    }

    codegen_gen_mov_or_math_for_value("eax", "ebx", "dword", history->flags);
    // EBX register now has the address of the variable for indirection
}

void codegen_generate_unary_address(struct node *node, struct history *history)
{
    int flags = history->flags;
    // Generate the operand while passing the indirection flag
    codegen_generate_expressionable(node->unary.operand, history_down(history, flags | EXPRESSION_GET_ADDRESS));
    register_unset_flag(REGISTER_EBX_IS_USED);

    asm_push("mov eax, ebx");
}

void codegen_generate_normal_unary(struct node *node, struct history *history)
{
    bool eax_is_used = register_is_used("eax");
    if (eax_is_used)
    {
        register_unset_flag(REGISTER_EAX_IS_USED);
        asm_push_ins_push("eax", STACK_FRAME_ELEMENT_TYPE_SAVED_REGISTER, "eax_used_saved_value");
    }
    codegen_generate_expressionable(node->unary.operand, history);
    // We have generated the value for the operand
    // Let's now decide what to do with the result based on the operator
    if (S_EQ(node->unary.op, "-"))
    {
        // We have negation operator, so negate.
        asm_push("neg eax");
    }
    else if (S_EQ(node->unary.op, "~"))
    {
        asm_push("not eax");
    }
    else if (S_EQ(node->unary.op, "*"))
    {
        // We are accessing a pointer
        codegen_generate_unary_indirection(node, history);
    }
    if (eax_is_used)
    {
        asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_SAVED_REGISTER, "eax_used_saved_value");
        codegen_gen_math_for_value("ecx", "eax", history->flags);
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
    else if (op_is_address(node->unary.op))
    {
        codegen_generate_unary_address(node, history);
        return;
    }

    codegen_generate_normal_unary(node, history);
}

void codegen_generate_exp_parenthesis_node(struct node *node, struct history *history)
{
    codegen_generate_expressionable(node->parenthesis.exp, history_down(history, codegen_remove_uninheritable_flags(history->flags)));
}

void codegen_generate_string(struct node *node, struct history *history)
{
    const char *label = codegen_register_string(node->sval);
    codegen_gen_mov_or_math_for_value("eax", label, "dword", history->flags);
}

void codegen_generate_tenary(struct node *node, struct history *history)
{
    int true_label_id = codegen_label_count();
    int false_label_id = codegen_label_count();
    int tenary_end_label_id = codegen_label_count();

    // Condition node would have already been generated as tenaries are
    // nested into an expression
    asm_push("cmp eax, 0");
    asm_push("je .tenary_false_%i", false_label_id);
    asm_push(".tenary_true_%i:", true_label_id);

    register_unset_flag(REGISTER_EAX_IS_USED);
    // Now we generate the true condition
    codegen_generate_new_expressionable(node->tenary.true_node, history_down(history, 0));
    asm_push("jmp .tenary_end_%i", tenary_end_label_id);

    asm_push(".tenary_false_%i:", false_label_id);
    // Now the false condition
    register_unset_flag(REGISTER_EAX_IS_USED);
    codegen_generate_new_expressionable(node->tenary.false_node, history_down(history, 0));
    asm_push(".tenary_end_%i:", tenary_end_label_id);
}

void codegen_generate_expressionable(struct node *node, struct history *history)
{
    bool is_root = codegen_is_exp_root(history);
    if (is_root)
    {
        // Set the flag for when we go down the tree, they are not the root we are
        history->flags |= EXPRESSION_IS_NOT_ROOT_NODE;
    }

    switch (node->type)
    {
    case NODE_TYPE_NUMBER:
        codegen_generate_number_node(node, history);
        break;
    case NODE_TYPE_IDENTIFIER:
        codegen_generate_identifier(node, history);
        break;
    case NODE_TYPE_STRING:
        codegen_generate_string(node, history);
        break;

    case NODE_TYPE_EXPRESSION:
        codegen_generate_exp_node(node, history);
        break;
    case NODE_TYPE_EXPRESSION_PARENTHESIS:
        codegen_generate_exp_parenthesis_node(node, history);
        break;

    case NODE_TYPE_UNARY:
        codegen_generate_unary(node, history);
        break;

    case NODE_TYPE_TENARY:
        codegen_generate_tenary(node, history);
        break;
    }
}

void codegen_generate_brand_new_expression(struct node *node, struct history *history)
{
    register_unset_flag(REGISTER_EAX_IS_USED);
    codegen_generate_expressionable(node, history);
    register_unset_flag(REGISTER_EAX_IS_USED);
}

static void codegen_generate_variable_for_array(struct node *node)
{
    if (node->var.val != NULL)
    {
        codegen_err("We don't yet support values for arrays");
        return;
    }

    char tmp_buf[256];
    asm_push("%s: %s 0", node->var.name, asm_keyword_for_size(variable_size(node), tmp_buf));
}
static void codegen_generate_global_variable_for_primitive(struct node *node)
{
    char tmp_buf[256];
    if (node->var.val != NULL)
    {
        if (node->var.val->type == NODE_TYPE_STRING)
        {
            // Ok we have a string here
            const char *label = codegen_register_string(node->var.val->sval);
            asm_push("%s: %s %s", node->var.name, asm_keyword_for_size(variable_size(node), tmp_buf), label, tmp_buf);
        }
        else
        {
            asm_push("%s: %s %lld", node->var.name, asm_keyword_for_size(variable_size(node), tmp_buf), node->var.val->llnum, tmp_buf);
        }
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
    asm_push("%s: %s 0", node->var.name, asm_keyword_for_size(variable_size(node), tmp_buf));
}

static void codegen_generate_global_variable_for_union(struct node *node)
{
    if (node->var.val != NULL)
    {
        codegen_err("We don't yet support values for union");
        return;
    }

    char tmp_buf[256];
    asm_push("%s: %s 0", node->var.name, asm_keyword_for_size(variable_size(node), tmp_buf));
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
    case DATA_TYPE_VOID:
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

    case DATA_TYPE_UNION:
        codegen_generate_global_variable_for_union(node);
        break;

    default:
        codegen_err("Not sure how to generate value for global variable.. Problem!");
    }
    assert(node->type == NODE_TYPE_VARIABLE);
    codegen_new_scope_entity(node, 0, 0);
}

void codegen_generate_global_variable_list(struct node *var_list_node)
{
    assert(var_list_node->type == NODE_TYPE_VARIABLE_LIST);
    vector_set_peek_pointer(var_list_node->var_list.list, 0);
    struct node *var_node = vector_peek_ptr(var_list_node->var_list.list);
    while (var_node)
    {
        codegen_generate_global_variable(var_node);
        var_node = vector_peek_ptr(var_list_node->var_list.list);
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
            stack_size += variable_size(node);
            break;
        case NODE_TYPE_VARIABLE_LIST:
            stack_size += variable_size_for_list(node);
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

void codegen_generate_scope_no_new_scope(struct vector *statements);
void codegen_generate_stack_scope(struct vector *statements, size_t scope_size)
{
    // New body new scope.

    // Resolver scope needs to exist too it will be this normal scopes replacement
    codegen_new_scope(RESOLVER_SCOPE_FLAG_IS_STACK);
    codegen_generate_scope_no_new_scope(statements);
    codegen_finish_scope();
}

void codegen_generate_body(struct node *node)
{
    codegen_generate_stack_scope(node->body.statements, node->body.size);
}
void codegen_generate_scope_variable(struct node *node)
{
    // Register the variable to the scope.
    struct resolver_entity *entity = codegen_new_scope_entity(node, node->var.aoffset, RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);

    // Scope variables have values, lets compute that
    if (node->var.val)
    {
        struct history history;
        codegen_generate_assignment_expression_value_part(entity->result, entity, node, node->var.val, "=", history_begin(&history, 0));
    }
}

void codegen_generate_scope_variable_for_list(struct node *var_list_node)
{
    assert(var_list_node->type == NODE_TYPE_VARIABLE_LIST);
    vector_set_peek_pointer(var_list_node->var_list.list, 0);
    struct node *var_node = vector_peek_ptr(var_list_node->var_list.list);
    while (var_node)
    {
        codegen_generate_scope_variable(var_node);
        var_node = vector_peek_ptr(var_list_node->var_list.list);
    }
}

void codegen_generate_statement_return(struct node *node)
{
    struct history history;

    // Let's generate the expression of the return statement
    codegen_generate_expressionable(node->stmt.ret.exp, history_begin(&history, 0));

    // Let's just check we have an owner for this statement, we should have
    // Bug if we dont.
    assert(node->binded.owner);

    // Generate the stack subtraction.
    codegen_stack_add_no_compile_time_stack_frame_restore(C_ALIGN(function_node_stack_size(node->binded.function)));

    // Now we must leave the function
    asm_pop_ebp_no_stack_frame_restore();
    asm_push("ret");

    // EAX is available now
    register_unset_flag(REGISTER_EAX_IS_USED);
}

void _codegen_generate_if_stmt(struct node *node, int end_label_id);
void codegen_generate_else_stmt(struct node *node)
{
    codegen_generate_body(node->stmt._else.body_node);
}
void codegen_generate_else_or_else_if(struct node *node, int end_label_id)
{
    if (node->type == NODE_TYPE_STATEMENT_IF)
    {
        _codegen_generate_if_stmt(node, end_label_id);
    }
    else if (node->type == NODE_TYPE_STATEMENT_ELSE)
    {
        codegen_generate_else_stmt(node);
    }
    else
    {
        FAIL_ERR("Unexpected node, expecting else or else if. Compiler bug");
    }
}

void _codegen_generate_if_stmt(struct node *node, int end_label_id)
{
    struct history history;
    int if_label_id = codegen_label_count();
    codegen_generate_brand_new_expression(node->stmt._if.cond_node, history_begin(&history, 0));
    asm_push("cmp eax, 0");
    asm_push("je .if_%i", if_label_id);
    // Unset the EAX register flag we are not using it now
    register_unset_flag(REGISTER_EAX_IS_USED);
    codegen_generate_body(node->stmt._if.body_node);
    asm_push("jmp .if_end_%i", end_label_id);
    asm_push(".if_%i:", if_label_id);

    if (node->stmt._if.next)
    {
        codegen_generate_else_or_else_if(node->stmt._if.next, end_label_id);
    }
}

void codegen_generate_if_stmt(struct node *node)
{
    int end_label_id = codegen_label_count();
    _codegen_generate_if_stmt(node, end_label_id);

    asm_push(".if_end_%i:", end_label_id);
}

void codegen_generate_while_stmt(struct node *node)
{
    struct history history;
    codegen_begin_entry_exit_point();
    int while_start_id = codegen_label_count();
    int while_end_id = codegen_label_count();
    asm_push(".while_start_%i:", while_start_id);

    // Generate the expressionable condition
    codegen_generate_brand_new_expression(node->stmt._while.cond, history_begin(&history, 0));
    asm_push("cmp eax, 0");
    asm_push("je .while_end_%i", while_end_id);
    // Okay, let us now generate the body
    codegen_generate_body(node->stmt._while.body);
    asm_push("jmp .while_start_%i", while_start_id);
    asm_push(".while_end_%i:", while_end_id);
    codegen_end_entry_exit_point();
}

void codegen_generate_do_while_stmt(struct node *node)
{
    struct history history;
    codegen_begin_entry_exit_point();
    int do_while_start_id = codegen_label_count();
    asm_push(".do_while_start_%i:", do_while_start_id);
    codegen_generate_body(node->stmt._do_while.body);
    codegen_generate_brand_new_expression(node->stmt._do_while.cond, history_begin(&history, 0));
    asm_push("cmp eax, 0");
    asm_push("jne .do_while_start_%i", do_while_start_id);
    codegen_end_entry_exit_point();
}

void codegen_generate_for_stmt(struct node *node)
{
    struct for_stmt *for_stmt = &node->stmt._for;
    struct history history;
    codegen_begin_entry_exit_point();
    int for_loop_start_id = codegen_label_count();
    int for_loop_end_id = codegen_label_count();
    if (for_stmt->init)
    {
        // We have our FOR loop initialization, lets initialize.
        codegen_generate_brand_new_expression(for_stmt->init, history_begin(&history, 0));
    }

    asm_push(".for_loop%i:", for_loop_start_id);
    if (for_stmt->cond)
    {
        // We have our FOR loop condition, lets condition it.
        codegen_generate_brand_new_expression(for_stmt->cond, history_begin(&history, 0));
        asm_push("cmp eax, 0");
        asm_push("je .for_loop_end%i", for_loop_end_id);
    }

    if (for_stmt->body)
    {
        codegen_generate_body(for_stmt->body);
    }

    if (for_stmt->loop)
    {
        codegen_generate_brand_new_expression(for_stmt->loop, history_begin(&history, 0));
    }
    asm_push("jmp .for_loop%i", for_loop_start_id);
    asm_push(".for_loop_end%i:", for_loop_end_id);
    codegen_end_entry_exit_point();
}

void codegen_generate_switch_case_stmt(struct node *node)
{
    // We don't yet support numeric expressions for cases. I.e 50+20 in a case is 100% valid.
    // We do not support that yet, static numbers only, no expressions supported in a case.

    struct node *case_stmt_exp = node->stmt._case.exp;
    assert(case_stmt_exp->type == NODE_TYPE_NUMBER);
    struct history history;
    codegen_begin_case_statement(case_stmt_exp->llnum);
    asm_push("; CASE %i", case_stmt_exp->llnum);
    codegen_end_case_statement();
}

void codegen_generate_switch_default_stmt(struct node *node)
{
    asm_push("; DEFAULT CASE");
    struct code_generator *generator = current_process->generator;
    struct generator_switch_stmt *switch_stmt_data = &generator->_switch;
    asm_push(".switch_stmt_%i_case_default:", switch_stmt_data->current.id);
}

void codegen_generate_switch_stmt_case_jumps(struct node *node)
{
    vector_set_peek_pointer(node->stmt._switch.cases, 0);
    struct parsed_switch_case *switch_case = vector_peek(node->stmt._switch.cases);
    while (switch_case)
    {
        asm_push("cmp eax, %i", switch_case->index);
        asm_push("je .switch_stmt_%i_case_%i", codegen_switch_id(), switch_case->index);
        switch_case = vector_peek(node->stmt._switch.cases);
    }

    // Do we have a default case in the switch statement?
    // Then lets jump to the default label
    if (node->stmt._switch.has_default_case)
    {
        asm_push("jmp .switch_stmt_%i_case_default", codegen_switch_id());
        return;
    }
    // Not equal to any case? Then go to the exit point, no need to restore
    // any stack of any kind as we have not even gone into the body of the switch
    codegen_goto_exit_point_maintain_stack(node);
}
void codegen_generate_switch_stmt(struct node *node)
{
    // Generate the expression for the switch statement
    struct history history;
    codegen_begin_entry_exit_point();
    codegen_begin_switch_statement();

    codegen_generate_brand_new_expression(node->stmt._switch.exp, history_begin(&history, 0));
    codegen_generate_switch_stmt_case_jumps(node);

    // Let's generate the body
    codegen_generate_body(node->stmt._switch.body);

    codegen_end_switch_statement();
    codegen_end_entry_exit_point();
}

void codegen_generate_break_stmt(struct node *node)
{
    // Okay we have a break, we must jump to the last registered exit point.
    codegen_goto_exit_point(node);
}

void codegen_generate_continue_stmt(struct node *node)
{
    codegen_goto_entry_point(node);
}

void codegen_generate_label(struct node *node)
{
    asm_push("label_%s:", node->label.name->sval);
}

void codegen_generate_goto_stmt(struct node *node)
{
    asm_push("jmp label_%s", node->stmt._goto.label->sval);
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

    case NODE_TYPE_VARIABLE_LIST:
        codegen_generate_scope_variable_for_list(node);
        break;

    case NODE_TYPE_STATEMENT_RETURN:
        codegen_generate_statement_return(node);
        break;

    case NODE_TYPE_STATEMENT_IF:
        codegen_generate_if_stmt(node);
        break;

    case NODE_TYPE_STATEMENT_WHILE:
        codegen_generate_while_stmt(node);
        break;

    case NODE_TYPE_STATEMENT_DO_WHILE:
        codegen_generate_do_while_stmt(node);
        break;

    case NODE_TYPE_STATEMENT_FOR:
        codegen_generate_for_stmt(node);
        break;

    case NODE_TYPE_STATEMENT_SWITCH:
        codegen_generate_switch_stmt(node);
        break;

    case NODE_TYPE_STATEMENT_CASE:
        codegen_generate_switch_case_stmt(node);
        break;

    case NODE_TYPE_STATEMENT_DEFAULT:
        codegen_generate_switch_default_stmt(node);
        break;

    case NODE_TYPE_STATEMENT_BREAK:
        codegen_generate_break_stmt(node);
        break;

    case NODE_TYPE_STATEMENT_CONTINUE:
        codegen_generate_continue_stmt(node);
        break;

    case NODE_TYPE_STATEMENT_GOTO:
        codegen_generate_goto_stmt(node);
        break;

    case NODE_TYPE_LABEL:
        codegen_generate_label(node);
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

void codegen_generate_function_arguments(struct vector *argument_vector)
{
    vector_set_peek_pointer(argument_vector, 0);
    struct node *current = vector_peek_ptr(argument_vector);
    while (current)
    {
        codegen_new_scope_entity(current, current->var.aoffset, RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);
        current = vector_peek_ptr(argument_vector);
    }
}

void codegen_generate_function_prototype(struct node *node)
{
    // We must register this function prototype
    codegen_register_function(node, 0);
    asm_push("extern %s", node->func.name);

    // Since its a prototype no code needs to be generated, just its presence must be registered
    // and it marked as external
}

void codegen_generate_function_with_body(struct node *node)
{
    // We must register this function
    codegen_register_function(node, 0);
    asm_push("global %s", node->func.name);
    asm_push("; %s function", node->func.name);
    asm_push("%s:", node->func.name);

    // We have to create a stack frame ;)
    asm_push_ebp();
    asm_push("mov ebp, esp");
    codegen_stack_sub(C_ALIGN(function_node_stack_size(node)));
    // Generate scope for function arguments
    codegen_new_scope(RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);

    codegen_generate_function_arguments(node->func.argument_vector);

    // Generate the function body
    codegen_generate_body(node->func.body_n);

    // End function argument scope
    codegen_finish_scope();

    codegen_stack_add(C_ALIGN(function_node_stack_size(node)));

    asm_pop_ebp();
    // We expect the compiler stack frame to be empty at this point.
    stackframe_assert_empty(current_function);

    asm_push("ret");
}
void codegen_generate_function(struct node *node)
{
    current_function = node;
    if (function_node_is_prototype(node))
    {
        codegen_generate_function_prototype(node);
        return;
    }

    codegen_generate_function_with_body(node);
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

void codegen_generate_struct(struct node *node)
{
    // We only have to care if we have a variable on this struct
    if (node->flags & NODE_FLAG_HAS_VARIABLE_COMBINED)
    {
        // Generate the structure variable
        codegen_generate_global_variable(node->_struct.var);
    }
}

void codegen_generate_union(struct node *node)
{
    // We only have to care if we have a variable on this struct
    if (node->flags & NODE_FLAG_HAS_VARIABLE_COMBINED)
    {
        // Generate the structure variable
        codegen_generate_global_variable(node->_union.var);
    }
}

void codegen_generate_data_section_part(struct node *node)
{
    if (node->type == NODE_TYPE_VARIABLE)
    {
        codegen_generate_global_variable(node);
    }
    else if (node->type == NODE_TYPE_VARIABLE_LIST)
    {
        codegen_generate_global_variable_list(node);
    }
    else if (node->type == NODE_TYPE_STRUCT)
    {
        codegen_generate_struct(node);
    }
    else if (node->type == NODE_TYPE_UNION)
    {
        codegen_generate_union(node);
    }
}

void codegen_generate_data_section()
{
    asm_push("section .data");
    struct node *node = NULL;
    while ((node = codegen_node_next()) != NULL)
    {
        codegen_generate_data_section_part(node);
    }
}

// Unused for now.
void codegen_generate_start_symbol()
{
    asm_push("global _start");
    asm_push("_start:");
    asm_push("call main");
    asm_push("mov ebx, eax");
    asm_push("mov eax, 1");
    asm_push("int 0x80");
    asm_push("jmp $");
}
/**
 * Starts generating code from the root of the tree, working its way down the leafs
 */
void codegen_generate_root()
{
    asm_push("section .text");

    struct node *node = NULL;
    while ((node = codegen_node_next()) != NULL)
    {
        codegen_generate_root_node(node);
    }
}

int codegen(struct compile_process *process)
{
    current_process = process;
    x86_codegen.compiler = current_process;

    // Create the root scope for this process
    scope_create_root(process);

    vector_set_peek_pointer(process->node_tree_vec, 0);
    // Global variables and down the tree locals... Global scope lets create it.
    codegen_new_scope(0);
    codegen_generate_data_section();
    vector_set_peek_pointer(process->node_tree_vec, 0);
    codegen_generate_root();
    codegen_finish_scope();

    // Finally generate read only data
    codegen_generate_rod();

    return 0;
}

struct code_generator *codegenerator_new(struct compile_process *process)
{
    struct code_generator *generator = calloc(sizeof(struct code_generator), 1);
    generator->states.expr = vector_create(sizeof(struct expression_state *));
    generator->string_table = vector_create(sizeof(struct string_table_element *));
    generator->exit_points = vector_create(sizeof(struct exit_point *));
    generator->entry_points = vector_create(sizeof(struct entry_point *));
    generator->responses = vector_create(sizeof(struct response *));
    generator->_switch.switches = vector_create(sizeof(struct generator_switch_stmt_entity));
    return generator;
}