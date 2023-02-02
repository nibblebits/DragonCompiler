#include "compiler.h"
#include "helpers/buffer.h"
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>

#define STRUCTURE_PUSH_START_POSITION_ONE 1

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
void asm_push_ins_with_datatype(struct datatype* dtype, const char* fmt, ...);



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
    .ret = asm_push_ins_with_datatype,
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
    RESPONSE_FLAG_RESOLVED_ENTITY = 0b00000100,
    RESPONSE_FLAG_UNARY_GET_ADDRESS = 0b00001000
};

#define RESPONSE_SET(x) (&(struct response){x})
#define RESPONSE_EMPTY RESPONSE_SET()

struct response_data
{
    union
    {
        struct resolver_entity *resolved_entity;
    };
};

// Operations can pass results back up the stack.
struct response
{
    int flags;
    struct response_data data;
};

enum
{
    CODEGEN_ENTITY_RULE_IS_STRUCT_OR_UNION_NON_POINTER = 0b00000001,
    CODEGEN_ENTITY_RULE_IS_FUNCTION_CALL = 0b00000010,
    CODEGEN_ENTITY_RULE_IS_GET_ADDRESS = 0b00000100,
    CODEGEN_ENTITY_RULE_WILL_PEEK_AT_EBX = 0b00001000
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
        res->flags |= response_in->flags;
        if (response_in->data.resolved_entity)
        {
            res->data.resolved_entity = response_in->data.resolved_entity;
        }
        res->flags |= RESPONSE_FLAG_ACKNOWLEDGED;
    }
}

bool codegen_response_acknowledged(struct response *res)
{
    return res && res->flags & RESPONSE_FLAG_ACKNOWLEDGED;
}

bool codegen_response_has_entity(struct response *res)
{
    return codegen_response_acknowledged(res) && res->flags & RESPONSE_FLAG_RESOLVED_ENTITY && res->data.resolved_entity;
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
 * @brief Returns additional rules about the entity that only the code generator needs to understand
 *
 * @param last_entity
 * @param history
 * @return int
 */
int codegen_entity_rules(struct resolver_entity *last_entity, struct history *history)
{
    int rule_flags = 0;
    if (!last_entity)
    {
        // What we we going to do with NULL..
        return 0;
    }

    // Is the variable resolved a non pointer structure
    // if so then it must be pushed as we are passing by value

    if (datatype_is_struct_or_union_non_pointer(&last_entity->dtype))
    {
        rule_flags |= CODEGEN_ENTITY_RULE_IS_STRUCT_OR_UNION_NON_POINTER;
    }
    if (last_entity->type == RESOLVER_ENTITY_TYPE_FUNCTION_CALL)
    {
        rule_flags |= CODEGEN_ENTITY_RULE_IS_FUNCTION_CALL;
    }
    else if (history->flags & EXPRESSION_GET_ADDRESS)
    {
        rule_flags |= CODEGEN_ENTITY_RULE_IS_GET_ADDRESS;
    }
    else if (last_entity->type == RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS)
    {
        rule_flags |= CODEGEN_ENTITY_RULE_IS_GET_ADDRESS;
    }
    else
    {
        rule_flags |= CODEGEN_ENTITY_RULE_WILL_PEEK_AT_EBX;
    }
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

void codegen_data_section_add(const char *data, ...)
{
    va_list args;
    va_start(args, data);
    char *new_data = malloc(256);
    vsprintf(new_data, data, args);
    vector_push(current_process->generator->custom_data_section, &new_data);
}

void asm_push(const char *ins, ...)
{
    va_list args;
    va_start(args, ins);
    asm_push_args(ins, args);
    va_end(args);
}

void asm_push_ins_push_with_flags(const char *fmt, int stack_entity_type, const char *stack_entity_name, int flags, ...)
{
    char tmp_buf[200];
    sprintf(tmp_buf, "push %s", fmt);
    va_list args;
    va_start(args, flags);
    asm_push_args(tmp_buf, args);
    va_end(args);

    // Let's add it to the stack frame for compiler referencing
    assert(current_function);
    stackframe_push(current_function, &(struct stack_frame_element){.flags = flags, .type = stack_entity_type, .name = stack_entity_name});
}

void asm_push_ins_with_datatype(struct datatype* dtype, const char* fmt, ...)
{
    char tmp_buf[200];
    sprintf(tmp_buf, "push %s", fmt);
    va_list args;
    va_start(args, fmt);
    asm_push_args(tmp_buf, args);
    va_end(args);

    stackframe_push(current_function, &(struct stack_frame_element){.type = STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, .name = "result_value", .flags = STACK_FRAME_ELEMENT_FLAG_HAS_DATATYPE, .data.dtype = *dtype});
}

void asm_push_ins_push_with_data(const char *fmt, int stack_entity_type, const char *stack_entity_name, int flags, struct stack_frame_data *data, ...)
{
    char tmp_buf[200];
    sprintf(tmp_buf, "push %s", fmt);
    va_list args;
    va_start(args, data);
    asm_push_args(tmp_buf, args);
    va_end(args);

    flags |= STACK_FRAME_ELEMENT_FLAG_HAS_DATATYPE;
    // Let's add it to the stack frame for compiler referencing
    assert(current_function);
    stackframe_push(current_function, &(struct stack_frame_element){.type = stack_entity_type, .name = stack_entity_name, .flags = flags, .data = *data});
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

struct stack_frame_element *asm_stack_back()
{
    return stackframe_back(current_function);
}

struct stack_frame_element *asm_stack_peek()
{
    return stackframe_peek(current_function);
}

void asm_stack_peek_start()
{
    stackframe_peek_start(current_function);
}

/**
 * @brief Gets the datatype from the last pushed element to the stack. True is returned
 * if the last element on the stack has a datatype, if not then false.
 * \param dtype_out The datatype we should set with the one on the stack
 */
bool asm_datatype_back(struct datatype *dtype_out)
{
    struct stack_frame_element *last_stack_frame_element = asm_stack_back();
    if (!last_stack_frame_element)
        return false;

    if (!(last_stack_frame_element->flags & STACK_FRAME_ELEMENT_FLAG_HAS_DATATYPE))
    {
        return false;
    }

    *dtype_out = last_stack_frame_element->data.dtype;
    return true;
}

int asm_push_ins_pop(const char *fmt, int expecting_stack_entity_type, const char *expecting_stack_entity_name, ...)
{
    char tmp_buf[200];
    sprintf(tmp_buf, "pop %s", fmt);
    va_list args;
    va_start(args, expecting_stack_entity_name);
    asm_push_args(tmp_buf, args);
    va_end(args);

    // Let's add it to the stack frame for compiler referencing
    assert(current_function);
    struct stack_frame_element *element = stackframe_back(current_function);
    int flags = element->flags;
    stackframe_pop_expecting(current_function, expecting_stack_entity_type, expecting_stack_entity_name);
    return flags;
}

int asm_push_ins_pop_or_ignore(const char *fmt, int expecting_stack_entity_type, const char *expecting_stack_entity_name, ...)
{
    if (!stackframe_back_expect(current_function, expecting_stack_entity_type, expecting_stack_entity_name))
    {
        return STACK_FRAME_ELEMENT_FLAG_ELEMENT_NOT_FOUND;
    }

    char tmp_buf[200];
    sprintf(tmp_buf, "pop %s", fmt);
    va_list args;
    va_start(args, expecting_stack_entity_name);
    asm_push_args(tmp_buf, args);
    va_end(args);

    struct stack_frame_element *element = stackframe_back(current_function);
    int flags = element->flags;
    stackframe_pop_expecting(current_function, expecting_stack_entity_type, expecting_stack_entity_name);
    return flags;
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

void codegen_stack_sub_with_name(size_t stack_size, const char *name)
{
    if (stack_size != 0)
    {
        stackframe_sub(current_function, STACK_FRAME_ELEMENT_TYPE_UNKNOWN, name, stack_size);
        asm_push("sub esp, %lld", stack_size);
    }
}

void codegen_stack_sub(size_t stack_size)
{
    codegen_stack_sub_with_name(stack_size, "stack_subtraction");
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

void codegen_reduce_register(const char *reg, size_t size, bool is_signed)
{
    if (size != DATA_SIZE_DWORD && size > 0)
    {
        const char *ins = "movsx";
        if (!is_signed)
        {
            ins = "movzx";
        }

        asm_push("%s eax, %s", ins, codegen_sub_register("eax", size));
    }
}

void codegen_plus_or_minus_string_for_value(char *out, int val, size_t len)
{
    memset(out, 0, len);
    if (val < 0)
    {
        sprintf(out, "%i", val);
    }
    else
    {
        sprintf(out, "+%i", val);
    }
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
    struct history history;
    codegen_generate_expressionable(node, history_begin(&history, flags));
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

bool codegen_can_gen_math(int flags)
{
    return flags & EXPRESSION_GEN_MATHABLE;
}

void codegen_gen_math_for_value(const char *reg, const char *value, int flags, bool is_signed)
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
        asm_push("mov ecx, %s", value);

        // Need a way to know if its signed in the future.. Assumed all signed for now.
        if (is_signed)
        {
            asm_push("imul ecx");
        }
        else
        {
            asm_push("mul ecx");
        }
    }
    else if (flags & EXPRESSION_IS_DIVISION)
    {
        asm_push("mov ecx, %s", value);
        asm_push("cdq");
        if (is_signed)
        {
            asm_push("idiv ecx");
        }
        else
        {
            asm_push("div ecx");
        }
    }
    else if (flags & EXPRESSION_IS_MODULAS)
    {
        asm_push("mov ecx, %s", value);
        asm_push("cdq");
        // Assuming signed, check for unsigned in the future, check for float in the future..
        if (is_signed)
        {
            asm_push("idiv ecx");
        }
        else
        {
            asm_push("div ecx");
        }
        // Remainder stored in EDX
        asm_push("mov eax, edx");
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
        value = codegen_sub_register(value, DATA_SIZE_BYTE);
        asm_push("sal %s, %s", reg, value);
    }
    else if (flags & EXPRESSION_IS_BITSHIFT_RIGHT)
    {
        value = codegen_sub_register(value, DATA_SIZE_BYTE);
        if (is_signed)
        {
            asm_push("sar %s, %s", reg, value);
        }
        else
        {
            asm_push("shr %s, %s", reg, value);
        }
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

/**
 * @brief Generates a structure to value operation. Pushing an entire structures memory
 * to the stack. Useful for passing structures to functions....
 *
 * @param entity The entity of the structure variable to be pushed to the stack
 * @param history Expressionable history
 * @param start_pos The start position for the strucutre push. Useful for ignoring pushes of the start of the structure, in cases where this may be handled else where....
 */
void codegen_generate_structure_push(struct resolver_entity *entity, struct history *history, int start_pos)
{
    asm_push("; STRUCTURE PUSH");
    size_t structure_size = align_value(entity->dtype.size, DATA_SIZE_DWORD);
    int pushes = structure_size / DATA_SIZE_DWORD;

    for (int i = pushes - 1; i >= start_pos; i--)
    {
        char fmt[10];
        int chunk_offset = (i * DATA_SIZE_DWORD);
        codegen_plus_or_minus_string_for_value(fmt, chunk_offset, sizeof(fmt));
        asm_push_ins_push_with_data("dword [%s%s]", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = entity->dtype}, "ebx", fmt);
    }
    asm_push("; END STRUCTURE PUSH");

    codegen_response_acknowledge(RESPONSE_SET(.flags = RESPONSE_FLAG_PUSHED_STRUCTURE));
}

void codegen_generate_move_struct(struct datatype *dtype, const char *base_address, off_t offset)
{
    size_t structure_size = align_value(datatype_size(dtype), DATA_SIZE_DWORD);
    int pops = structure_size / DATA_SIZE_DWORD;
    for (int i = 0; i < pops; i++)
    {
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        char fmt[10];
        int chunk_offset = offset + (i * DATA_SIZE_DWORD);
        codegen_plus_or_minus_string_for_value(fmt, chunk_offset, sizeof(fmt));
        asm_push("mov [%s%s], eax", base_address, fmt);
    }
}

void codegen_generate_structure_push_or_return(struct resolver_entity *entity, struct history *history, int start_pos)
{
    codegen_generate_structure_push(entity, history, start_pos);
}

static void codegen_gen_mov_for_value(const char *reg, const char *value, const char *datatype, int flags)
{
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
    codegen_gen_mov_for_value(reg_to_use, codegen_get_fmt_for_value(value_node, entity), mov_type_keyword, flags);
}

static void codegen_gen_mem_access_get_address(struct node *value_node, int flags, struct resolver_entity *entity)
{
    asm_push("lea ebx, [%s]", codegen_entity_private(entity)->address);
    asm_push_ins_push_with_flags("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", STACK_FRAME_ELEMENT_FLAG_IS_PUSHED_ADDRESS);
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

static void codegen_gen_mem_access(struct node *value_node, int flags, struct resolver_entity *entity)
{
    if (flags & EXPRESSION_GET_ADDRESS)
    {
        codegen_gen_mem_access_get_address(value_node, flags, entity);
        return;
    }

    // If the value cannot be pushed directly to the stack
    // we must first strip it down in the EAX register then push that instead..

    if (datatype_is_struct_or_union_non_pointer(&entity->dtype))
    {
        struct history history = {};
        history.flags = flags;
        codegen_gen_mem_access_get_address(value_node, 0, entity);
        asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        codegen_generate_structure_push_or_return(entity, &history, 0);
    }
    else if (datatype_element_size(&entity->dtype) != DATA_SIZE_DWORD)
    {
        asm_push("mov eax, [%s]", codegen_entity_private(entity)->address);
        codegen_reduce_register("eax", datatype_element_size(&entity->dtype), entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
        asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = entity->dtype});
    }
    else
    {
        // This can be pushed straight to the stack? i.e 4 bytes in size..
        // Then don't waste instructions
        asm_push_ins_push_with_data("dword [%s]", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = entity->dtype}, codegen_entity_private(entity)->address);
    }
}
/**
 * For literal numbers
 */
void codegen_generate_number_node(struct node *node, struct history *history)
{
    const char *reg_to_use = "eax";
    asm_push_ins_push_with_data("dword %i", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", STACK_FRAME_ELEMENT_FLAG_IS_NUMERICAL, &(struct stack_frame_data){.dtype = datatype_for_numeric()}, node->llnum);
}

/**
 * Hopefully we can find a better way of doing this.
 * Currently its the best ive got, to check for mul_or_div for certain operations
 */
static bool is_node_mul_or_div(struct node *node)
{
    return S_EQ(node->exp.op, "*") || S_EQ(node->exp.op, "/");
}
static bool is_node_array_access(struct node *node)
{
    return node->type == NODE_TYPE_EXPRESSION && is_array_operator(node->exp.op);
}

void codegen_generate_variable_access_for_entity(struct node *node, struct resolver_entity *entity, struct history *history)
{
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
    else if (S_EQ(op, "-="))
    {
        asm_push("sub %s [%s], %s", mov_type_keyword, address, reg_to_use);
    }
    else if (S_EQ(op, "*="))
    {
        asm_push("mov ecx, eax");
        asm_push("mov eax, [%s]", address);
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
        asm_push("mov ecx, eax");
        asm_push("mov eax, [%s]", address);
        asm_push("cdq");
        if (is_signed)
        {
            asm_push("idiv ecx");
        }
        else
        {
            asm_push("div ecx");
        }

        asm_push("mov %s [%s], %s", mov_type_keyword, address, reg_to_use);
    }
    else if (S_EQ(op, "<<="))
    {
        asm_push("mov ecx, %s", reg_to_use);
        asm_push("sal %s [%s], cl", mov_type_keyword, address);
    }
    else if (S_EQ(op, ">>="))
    {
        asm_push("mov ecx, %s", reg_to_use);
        if (is_signed)
        {
            asm_push("sar %s [%s], cl", mov_type_keyword, address);
        }
        else
        {
            asm_push("shr %s [%s], cl", mov_type_keyword, address);
        }
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
void codegen_generate_entity_access_array_bracket_pointer(struct resolver_result *result, struct resolver_entity *entity)
{
    struct history history = {};

    // Restore EBX
    asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

    // Now we have accessed the pointer we may add on the offset
    codegen_generate_expressionable(entity->array.array_index_node, history_begin(&history, 0));
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    // We must multiply the index by the element size
    if (datatype_element_size(&entity->dtype) > DATA_SIZE_BYTE)
    {
        asm_push("imul eax, %i", datatype_size_for_array_access(&entity->dtype));
    }
    asm_push("add ebx, eax");

    // Save EBX
    asm_push_ins_push_with_data("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = entity->dtype});
}
void codegen_generate_entity_access_array_bracket(struct resolver_result *result, struct resolver_entity *entity)
{
    struct history history = {};
    // We have an array bracket that needs to be computed at runtime..
    if (entity->flags & RESOLVER_ENTITY_FLAG_IS_POINTER_ARRAY_ENTITY)
    {
        codegen_generate_entity_access_array_bracket_pointer(result, entity);
        return;
    }

    // Restore EBX
    asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

    // Normal array access
    codegen_generate_expressionable(entity->array.array_index_node, history_begin(&history, 0));
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

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

    // Save EBX
    asm_push_ins_push_with_data("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = entity->dtype});
}

void codegen_generate_entity_access_for_variable_or_general(struct resolver_result *result, struct resolver_entity *entity)
{
    // Restore EBX
    asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("; Entity=%i", entity->type);
    if (entity->flags & RESOLVER_ENTITY_FLAG_DO_INDIRECTION)
    {
        asm_push("mov ebx, [ebx]");
    }
    asm_push("add ebx, %i", entity->offset);

    // Save EBX
    asm_push_ins_push_with_data("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = entity->dtype});
}

void codegen_generate_entity_access_for_function_call(struct resolver_result *result, struct resolver_entity *entity)
{

    vector_set_flag(entity->func_call_data.arguments, VECTOR_FLAG_PEEK_DECREMENT);
    vector_set_peek_pointer_end(entity->func_call_data.arguments);

    struct node *node = vector_peek_ptr(entity->func_call_data.arguments);
    int function_call_label_id = codegen_label_count();
    // Function address
    codegen_data_section_add("function_call_%i: dd 0", function_call_label_id);

    asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("mov dword [function_call_%i], ebx", function_call_label_id);

    // Is this a structure return type?
    if (datatype_is_struct_or_union_non_pointer(&entity->dtype))
    {
        struct history history = {};

        asm_push("; SUBTRACT ROOM FOR RETURNED STRUCTURE/UNION DATATYPE ");
        // Make room for the returned structure
        codegen_stack_sub_with_name(align_value(datatype_size(&entity->dtype), DATA_SIZE_DWORD), "result_value");

        // Now we must pass a pointer to the data we just created
        // which will be the current stack pointer
        asm_push_ins_push("esp", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }
    // If we have a structure return type then we need to push the address of the left operand.
    while (node)
    {
        struct history history;
        codegen_generate_expressionable(node, history_begin(&history, EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS));
        node = vector_peek_ptr(entity->func_call_data.arguments);
    }

    // Call the function, address is in EBX
    asm_push("call [function_call_%i]", function_call_label_id);
    size_t stack_size = entity->func_call_data.stack_size;
    if (datatype_is_struct_or_union_non_pointer(&entity->dtype))
    {
        // We returned a datatype? Then we need to account for
        // "push esp" where we passed the pointer to the structure data
        stack_size += DATA_SIZE_DWORD;
    }
    codegen_stack_add(stack_size);

    // We have to put EAX back to the stack for receivers of this function
    if (datatype_is_struct_or_union_non_pointer(&entity->dtype))
    {
        // This is a structure/union return type, therefore push all to the stack
        struct history history = {};
        asm_push("mov ebx, eax");
        codegen_generate_structure_push(entity, &history, 0);
    }
    else
    {
        asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = entity->dtype});
    }
    struct resolver_entity *next_entity = resolver_result_entity_next(entity);
    if (next_entity && datatype_is_struct_or_union(&entity->dtype))
    {
        // POp off EAX again
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

        // We have a next entity and the return type is a structure or union
        // therefore we should move the return value EAX Into EBX
        asm_push("mov ebx, eax");
        asm_push_ins_push("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }
}

void codegen_generate_entity_access(struct resolver_result *result, struct resolver_entity *root_assignment_entity, struct node *top_most_node, struct history *history);

void codegen_generate_entity_access_for_unsupported(struct resolver_result *result, struct resolver_entity *entity)
{
    struct history history = {};
    codegen_generate_expressionable(entity->node, &history);
    // We have now resolved the unsupported entity it will be on the stack.
}

void codegen_apply_unary_access(int amount)
{
    for (int i = 0; i < amount; i++)
    {
        asm_push("mov ebx, [ebx]");
    }
}

void codegen_generate_entity_access_for_unary_get_address(struct resolver_result *result, struct resolver_entity *entity)
{
    // We don't care about resolving the address as we already assume
    // that it is in the EBX register at this point in time...

    // Pop off the RPARAM as we dont need it right now
    asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("; PUSH ADDRESS &");

    // Let's push the datatype
    asm_push_ins_push_with_data("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = entity->dtype});
}

void codegen_generate_entity_access_for_unary_indirection(struct resolver_result *result, struct resolver_entity *entity, struct history *history)
{
    asm_push("; INDIRECTION");

    struct datatype operand_datatype;
    assert(asm_datatype_back(&operand_datatype));

    // If we have a result value waiting for us we will pop it, otherwise assume EBX is set already..
    int flags = asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

    int gen_entity_rules = codegen_entity_rules(result->last_entity, history);
    int depth = entity->indirection.depth;
    codegen_apply_unary_access(depth);

    // We must push the computed EBX back to the stack
    asm_push_ins_push_with_data("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", STACK_FRAME_ELEMENT_FLAG_IS_PUSHED_ADDRESS, &(struct stack_frame_data){.dtype = operand_datatype});
}

void codegen_generate_entity_access_for_unary_indirection_for_assignment_left_operand(struct resolver_result *result, struct resolver_entity *entity, struct history *history)
{
    asm_push("; INDIRECTION");
    // If we have a result value waiting for us we will pop it, otherwise assume EBX is set already..
    int flags = asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

    int gen_entity_rules = codegen_entity_rules(result->last_entity, history);
    // One less depth because we care about getting the address to set
    // not the value.
    int depth = entity->indirection.depth - 1;
    codegen_apply_unary_access(depth);

    // We must push the computed EBX back to the stack
    asm_push_ins_push_with_flags("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", STACK_FRAME_ELEMENT_FLAG_IS_PUSHED_ADDRESS);
}

void codegen_generate_entity_access_for_cast(struct resolver_result *result, struct resolver_entity *entity)
{
    asm_push("; CAST");
}
void codegen_generate_entity_access_for_entity(struct resolver_result *result, struct resolver_entity *entity, struct history *history)
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

    case RESOLVER_ENTITY_TYPE_FUNCTION_CALL:
        codegen_generate_entity_access_for_function_call(result, entity);
        break;

    case RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION:
        codegen_generate_entity_access_for_unary_indirection(result, entity, history);
        break;

    case RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS:
        codegen_generate_entity_access_for_unary_get_address(result, entity);
        break;

    case RESOLVER_ENTITY_TYPE_UNSUPPORTED:
        // Resolver didnt support this node, we need to break the problem down further.
        codegen_generate_entity_access_for_unsupported(result, entity);
        break;

    case RESOLVER_ENTITY_TYPE_CAST:
        codegen_generate_entity_access_for_cast(result, entity);
        break;
    default:
        compiler_error(current_process, "COMPILER BUG...");
    }
}

void codegen_generate_entity_access_for_entity_for_assignment_left_operand(struct resolver_result *result, struct resolver_entity *entity, struct history *history)
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

    case RESOLVER_ENTITY_TYPE_FUNCTION_CALL:
        codegen_generate_entity_access_for_function_call(result, entity);
        break;

    case RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION:
        codegen_generate_entity_access_for_unary_indirection_for_assignment_left_operand(result, entity, history);
        break;

    case RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS:
        codegen_generate_entity_access_for_unary_get_address(result, entity);
        break;

    case RESOLVER_ENTITY_TYPE_UNSUPPORTED:
        // Resolver didnt support this node, we need to break the problem down further.
        codegen_generate_entity_access_for_unsupported(result, entity);
        break;

    case RESOLVER_ENTITY_TYPE_CAST:
        codegen_generate_entity_access_for_cast(result, entity);
        break;
    default:
        compiler_error(current_process, "COMPILER BUG...");
    }
}

void codegen_generate_additional_for_last_entity(struct resolver_entity *last_entity, struct history *history, bool single_entity_access)
{
    if (!last_entity)
    {
        // What we we going to do with NULL..
        return;
    }

    // Is the variable resolved a non pointer structure
    // if so then it must be pushed as we are passing by value

    if (datatype_is_struct_or_union_non_pointer(&last_entity->dtype))
    {
        // We want a start position of 1 because at some point up the call stack
        // we move [ebx] into EAX...
        codegen_generate_structure_push(last_entity, history, STRUCTURE_PUSH_START_POSITION_ONE);
    }
    if (last_entity->type == RESOLVER_ENTITY_TYPE_FUNCTION_CALL)
    {
        // Only if we have a function call that is not on its own i.e test(50) rather than x = test(50)
        // Do we push the EAX to the stack
        // Hack (fix it)
        if (!(history->flags & IS_ALONE_STATEMENT))
        {
            // Push return value to the stack
            asm_push_ins_push("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        }
    }
    else if (history->flags & EXPRESSION_GET_ADDRESS)
    {
        // We are getting the address? well it would have been resolved
        // Its in EBX push it
        //        asm_push_ins_push("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }
    else if (last_entity->type == RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS)
    {
        // Do nothing.. handled earlier..
    }
    else
    {

        // We would have had the EBX register pushed, and since we are not getting an address
        // we should also go and get the value
        const char *reg_to_use = "eax";
        const char *mov_type_keyword =
            codegen_byte_word_or_dword_or_ddword(datatype_element_size(&last_entity->dtype), &reg_to_use);

        //   asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        // asm_push("mov eax, [ebx]");

        if (datatype_is_primitive(&last_entity->dtype) && datatype_element_size(&last_entity->dtype) != DATA_SIZE_DWORD)
        {
            codegen_reduce_register("eax", datatype_element_size(&last_entity->dtype), last_entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
        }
        asm_push_ins_push("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }
}

void codegen_generate_code_for_result_base(struct resolver_result *result, struct resolver_entity *entity, struct history *history)
{
    // Do we have to load the address of EBX or are we going to push the value directly.
    // Assignments need this to happen and would have set the EXPRESSION_GET_ADDRESS flag

    if (history->flags & EXPRESSION_GET_ADDRESS ||
        result->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX)
    {
        asm_push("lea ebx, [%s]", result->base.address);
        asm_push_ins_push_with_data("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = entity->dtype});
    }
    else if (result->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE)
    {
        asm_push_ins_push_with_data("dword [%s]", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = entity->dtype}, result->base.address);
    }
    else
    {
        compiler_error(current_process, "COMPILER BUG NOT SURE HOW TO HANDLE ROOT ENTITY!");
    }
}
void codegen_generate_entity_access_for_single(struct resolver_result *result, struct resolver_entity *entity, struct history *history)
{
    asm_push("; SINGLE ENTITY ACCESS");
    codegen_generate_code_for_result_base(result, entity, history);
    codegen_generate_additional_for_last_entity(entity, history, true);
    codegen_response_acknowledge((&(struct response){.flags = RESPONSE_FLAG_RESOLVED_ENTITY, .data.resolved_entity = entity}));
}

void codegen_generate_unsupported_entity(struct resolver_result *result, struct resolver_entity *entity, struct history *history)
{
    struct resolver_entity *current = entity;
    while (current)
    {
        codegen_generate_entity_access_for_entity(result, current, history);
        current = resolver_result_entity_next(current);
    }

    codegen_generate_additional_for_last_entity(result->last_entity, history, false);
    codegen_response_acknowledge((&(struct response){.flags = RESPONSE_FLAG_RESOLVED_ENTITY, .data.resolved_entity = result->last_entity}));
}

void codegen_generate_entity_access_for_entity_new(struct resolver_result *result, struct resolver_entity *current, struct history *history)
{
}

void codegen_generate_entity_access_start(struct resolver_result *result, struct resolver_entity *root_assignment_entity, struct history *history)
{
    if (root_assignment_entity->type == RESOLVER_ENTITY_TYPE_UNSUPPORTED)
    {
        // Unsupported entity then generate it
        codegen_generate_expressionable(root_assignment_entity->node, history);
    }
    else if (result->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE)
    {
        asm_push_ins_push_with_data("dword [%s]", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = root_assignment_entity->dtype}, result->base.address);
    }
    else if (result->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX)
    {
        if (root_assignment_entity->next && root_assignment_entity->next->flags & RESOLVER_ENTITY_FLAG_IS_POINTER_ARRAY_ENTITY)
        {
            asm_push("mov ebx, [%s]", result->base.address);
        }
        else
        {
            asm_push("lea ebx, [%s]", result->base.address);
        }
        asm_push_ins_push_with_data("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = root_assignment_entity->dtype});
    }
}
void codegen_generate_entity_access_for_assignment_left_operand(struct resolver_result *result, struct resolver_entity *root_assignment_entity, struct node *top_most_node, struct history *history)
{
    codegen_generate_entity_access_start(result, root_assignment_entity, history);
    struct resolver_entity *current = resolver_result_entity_next(root_assignment_entity);
    while (current)
    {
        codegen_generate_entity_access_for_entity_for_assignment_left_operand(result, current, history);
        current = resolver_result_entity_next(current);
    }
    struct resolver_entity *last_entity = result->last_entity;
    codegen_response_acknowledge((&(struct response){.flags = RESPONSE_FLAG_RESOLVED_ENTITY, .data.resolved_entity = last_entity}));
}

void codegen_generate_entity_access(struct resolver_result *result, struct resolver_entity *root_assignment_entity, struct node *top_most_node, struct history *history)
{
    if (root_assignment_entity->type == RESOLVER_ENTITY_TYPE_NATIVE_FUNCTION)
    {
        //Is this a native function we are calling?
        struct native_function* native_func = native_function_get(current_process, root_assignment_entity->name);
        if (native_func)
        {
            asm_push("; NATIVE FUNCTION %s", root_assignment_entity->name);
            // Since we have a native function the next entity should be the function call to that function
            struct resolver_entity* func_call_entity = resolver_result_entity_next(root_assignment_entity);
            assert(func_call_entity && func_call_entity->type == RESOLVER_ENTITY_TYPE_FUNCTION_CALL);

            native_func->callbacks.call(&x86_codegen, native_func, func_call_entity->func_call_data.arguments);

            return;
        }
    }
    codegen_generate_entity_access_start(result, root_assignment_entity, history);
    struct resolver_entity *current = resolver_result_entity_next(root_assignment_entity);
    while (current)
    {
        codegen_generate_entity_access_for_entity(result, current, history);
        current = resolver_result_entity_next(current);
    }
    struct resolver_entity *last_entity = result->last_entity;
    codegen_response_acknowledge((&(struct response){.flags = RESPONSE_FLAG_RESOLVED_ENTITY, .data.resolved_entity = last_entity}));
}

void codegen_generate_assignment_expression_move_value(struct resolver_entity *left_entity)
{
    const char *reg_to_use = "eax";
    const char *mov_type_keyword = codegen_byte_word_or_dword_or_ddword(datatype_element_size(&left_entity->dtype), &reg_to_use);

    // Pop off the destination address
    asm_push_ins_pop("edx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

    // // Pop off the value
    // asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    // // // All we do now is assign the value..
    // asm_push("mov %s [edx], %s", mov_type_keyword, reg_to_use);
}

void codegen_generate_assignment_expression_move_struct(struct resolver_entity *entity)
{
    asm_push("; STRUCT MOVE TO TARGET");
    asm_push_ins_pop("edx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    codegen_generate_move_struct(&entity->dtype, "edx", 0);
}

void codegen_generate_assignment_part(struct node *node, const char *op, struct history *history)
{
    // Pop the value of the right operand
    struct datatype right_operand_dtype;

    struct resolver_result *result = resolver_follow(current_process->resolver, node);
    assert(resolver_result_ok(result));

    struct resolver_entity *root_assignment_entity = resolver_result_entity_root(result);
    const char *reg_to_use = "eax";
    const char *mov_type = codegen_byte_word_or_dword_or_ddword(datatype_element_size(&result->last_entity->dtype), &reg_to_use);

    struct resolver_entity *next_entity = resolver_result_entity_next(root_assignment_entity);
    if (!next_entity)
    {
        if (datatype_is_struct_or_union_non_pointer(&result->last_entity->dtype))
        {
            codegen_generate_move_struct(&result->last_entity->dtype, result->base.address, 0);
        }
        else
        {
            asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

            // No further entities then set the value..
            codegen_generate_assignment_instruction_for_operator(mov_type, result->base.address, reg_to_use, op, result->last_entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
        }
    }
    else
    {
        // We have further processing neeeded.
        codegen_generate_entity_access_for_assignment_left_operand(result, root_assignment_entity, node, history);
        // Last stack element will contain the address to assign
        asm_push_ins_pop("edx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

        // Pop off the EAX register set by right operand
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

        // Make the move
        codegen_generate_assignment_instruction_for_operator(mov_type, "edx", reg_to_use, op, result->last_entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
    }

    codegen_response_acknowledge((&(struct response){.flags = RESPONSE_FLAG_RESOLVED_ENTITY, .data.resolved_entity = result->last_entity}));
}
void codegen_generate_assignment_expression(struct node *node, struct history *history)
{
    // Left node = to assign
    // Right node = value
    codegen_generate_expressionable(node->exp.right, history_down(history, EXPRESSION_IS_ASSIGNMENT | IS_RIGHT_OPERAND_OF_ASSIGNMENT));

    codegen_generate_assignment_part(node->exp.left, node->exp.op, history);

    // codegen_generate_assignment_expression_move_value(left_entity);
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
    else if (S_EQ(op, "%"))
    {
        flag |= EXPRESSION_IS_MODULAS;
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
    else if (S_EQ(op, "=="))
    {
        flag |= EXPRESSION_IS_EQUAL;
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

    codegen_generate_expressionable(node->exp.left, history_down(history, history->flags | EXPRESSION_IN_LOGICAL_EXPRESSION));
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    codegen_generate_logical_cmp(node->exp.op, history->exp.logical_end_label, history->exp.logical_end_label_positive);
    register_unset_flag(REGISTER_EAX_IS_USED);
    codegen_generate_expressionable(node->exp.right, history_down(history, history->flags | EXPRESSION_IN_LOGICAL_EXPRESSION));
    register_unset_flag(REGISTER_EAX_IS_USED);
    if (!is_logical_node(node->exp.right))
    {
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        codegen_generate_logical_cmp(node->exp.op, history->exp.logical_end_label, history->exp.logical_end_label_positive);
        codegen_generate_end_labels_for_logical_expression(node->exp.op, history->exp.logical_end_label, history->exp.logical_end_label_positive);
        asm_push_ins_push("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }
}

void codegen_handle_expression_with_pointer(struct resolver_entity *left_entity, struct node *right_node, struct history *history)
{
    // Left entity has already been handled at this point...

    // Generate the right node then multiply by 4 .. this is a pointer access after all
    // Save eax
    asm_push_ins_push("eax", STACK_FRAME_ELEMENT_TYPE_SAVED_REGISTER, "eax_saved_handle_exp_with_pointer");
    register_unset_flag(REGISTER_EAX_IS_USED);
    codegen_generate_expressionable(right_node, history);
    asm_push("imul eax, %i", datatype_element_size(&left_entity->dtype));
    asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_SAVED_REGISTER, "eax_saved_handle_exp_with_pointer");

    // Okay now to add
    asm_push("add eax, ecx");
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

    // We need to set the correct flag regarding which operator is being used
    int op_flags = codegen_set_flag_for_operator(node->exp.op);
    codegen_generate_expressionable(left_node, history_down(history, flags));
    codegen_generate_expressionable(right_node, history_down(history, flags));

    // What datatype are we dealing with
    // Default as numeric datatype.
    struct datatype last_dtype = datatype_for_numeric();
    asm_datatype_back(&last_dtype);
    if (codegen_can_gen_math(op_flags))
    {
        // Pop off right value
        struct datatype right_dtype = datatype_for_numeric();
        asm_datatype_back(&right_dtype);
        asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

        if (last_dtype.flags & DATATYPE_FLAG_IS_LITERAL)
        {
            // We are looking for the real datatype here
            // i.e a+5 a would be the type we care about not integer 5.
            asm_datatype_back(&last_dtype);
        }
        // Pop off left value
        struct datatype left_dtype = datatype_for_numeric();
        asm_datatype_back(&left_dtype);
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

        struct datatype *pointer_datatype = datatype_thats_a_pointer(&left_dtype, &right_dtype);

        if (pointer_datatype && datatype_size(datatype_pointer_reduce(pointer_datatype, 1)) > DATA_SIZE_BYTE)
        {
            // We have a pointer in this expression which means we need to multiply the value
            // that is not a pointer by the size of the pointer datatype .
            const char *reg = "ecx";
            if (pointer_datatype == &right_dtype)
            {
                reg = "eax";
            }

            asm_push("imul %s, %i", reg, datatype_size(datatype_pointer_reduce(pointer_datatype, 1)));
        }
        // Add together, subtract, multiply ect...
        codegen_gen_math_for_value("eax", "ecx", op_flags, last_dtype.flags & DATATYPE_FLAG_IS_SIGNED);
    }

    // Caller always expects a response from us..
    asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = last_dtype});
}

bool codegen_should_push_function_call_argument(struct response *res)
{
    return !codegen_response_acknowledged(res) || !(res->flags & RESPONSE_FLAG_PUSHED_STRUCTURE);
}


bool codegen_resolve_node_return_result(struct node *node, struct history *history, struct resolver_result **result_out)
{
    struct resolver_result *result = resolver_follow(current_process->resolver, node);
    if (resolver_result_ok(result))
    {
        struct resolver_entity *root_assignment_entity = resolver_result_entity_root(result);
        codegen_generate_entity_access(result, root_assignment_entity, node, history);

        if (result_out)
        {
            *result_out = result;
        }
        codegen_response_acknowledge((&(struct response){.flags = RESPONSE_FLAG_RESOLVED_ENTITY, .data.resolved_entity = result->last_entity}));

        return true;
    }

    return false;
}
bool codegen_resolve_node(struct node *node, struct history *history)
{
    return codegen_resolve_node_return_result(node, history, NULL);
}

bool codegen_resolve_node_for_value(struct node *node, struct history *history)
{
    struct resolver_result *result = NULL;
    if (!codegen_resolve_node_return_result(node, history, &result))
    {
        return false;
    }

    struct datatype dtype;
    assert(asm_datatype_back(&dtype));

    if (result->flags & RESOLVER_RESULT_FLAG_DOES_GET_ADDRESS)
    {
        // Since we are getting the address we already have it in EAX theres nothing
        // for us to do.
    }
    else if (result->last_entity->type == RESOLVER_ENTITY_TYPE_FUNCTION_CALL &&
             datatype_is_struct_or_union_non_pointer(&result->last_entity->dtype))
    {
    }
    else if (datatype_is_struct_or_union_non_pointer(&dtype))
    {
        codegen_generate_structure_push(result->last_entity, history, 0);
    }
    else if (!(dtype.flags & DATATYPE_FLAG_IS_POINTER))
    {

        // If the last entity is not a pointer then it must be accessed as a value.
        // therefore it must be reduced in size to its actual size.
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

        // As this is a value we may have to dive a bit deeper to resolve the final computed address.
        if (result->flags & RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE)
        {

            // Yeah we have not pushed the address or value already..
            // Let's do a final peek
            asm_push("mov eax, [eax]");
        }

        // The register must be broken down into the correct size
        codegen_reduce_register("eax", datatype_element_size(&dtype), dtype.flags & DATATYPE_FLAG_IS_SIGNED);
        asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = dtype});
    }

    // Now push the result back
    return true;
}

void _codegen_generate_exp_node(struct node *node, struct history *history)
{
    if (is_node_assignment(node))
    {
        codegen_generate_assignment_expression(node, history);
        return;
    }

    // Can we locate a variable for the given expression?
    if (codegen_resolve_node_for_value(node, history))
    {
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
    codegen_response_acknowledge((&(struct response){.flags = RESPONSE_FLAG_RESOLVED_ENTITY, .data.resolved_entity = entity}));
}

static bool is_comma_operator(struct node *node)
{
    return S_EQ(node->exp.op, ",");
}

void codegen_generate_unary_indirection(struct node *node, struct history *history)
{
    const char *reg_to_use = "ebx";
    int flags = history->flags;

    codegen_response_expect();

    // Generate the operand while passing the indirection flag
    codegen_generate_expressionable(node->unary.operand, history_down(history, flags | EXPRESSION_GET_ADDRESS | EXPRESSION_INDIRECTION));
    struct response *res = codegen_response_pull();
    assert(codegen_response_has_entity(res));

    struct datatype operand_datatype;
    assert(asm_datatype_back(&operand_datatype));

    // Lets pop off the value
    asm_push_ins_pop(reg_to_use, STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

    int depth = node->unary.indirection.depth;
    int real_depth = depth;
    // If we want the value not the address then we must depth+1 because
    // expressionable above will give us the address not the value
    if (!(history->flags & EXPRESSION_GET_ADDRESS))
    {
        depth++;
    }

    for (int i = 0; i < depth; i++)
    {
        asm_push("mov %s, [%s]", reg_to_use, reg_to_use);
    }

    // Do we need to truncate this value?
    if (real_depth == res->data.resolved_entity->dtype.pointer_depth)
    {
        // Seems like it as the depth equals total pointer depth
        // so in this senario we will be pointing directly on the datatype size..
        codegen_reduce_register(reg_to_use, datatype_size_no_ptr(&operand_datatype), operand_datatype.flags & DATATYPE_FLAG_IS_SIGNED);
    }

    asm_push_ins_push_with_data(reg_to_use, STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = operand_datatype});
    // Acknowledge it again incase someone else is waiting for a response..
    codegen_response_acknowledge((&(struct response){.flags = RESPONSE_FLAG_RESOLVED_ENTITY, .data.resolved_entity = res->data.resolved_entity}));
}

void codegen_generate_unary_address(struct node *node, struct history *history)
{
    int flags = history->flags;
    // Generate the operand while passing the indirection flag
    codegen_generate_expressionable(node->unary.operand, history_down(history, flags | EXPRESSION_GET_ADDRESS));

    codegen_response_acknowledge(&((struct response){.flags = RESPONSE_FLAG_UNARY_GET_ADDRESS}));
}

void codegen_generate_normal_unary(struct node *node, struct history *history)
{
    codegen_generate_expressionable(node->unary.operand, history);

    struct datatype last_dtype;
    assert(asm_datatype_back(&last_dtype));

    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

    // We have generated the value for the operand
    // Let's now decide what to do with the result based on the operator
    if (S_EQ(node->unary.op, "-"))
    {
        // We have negation operator, so negate.
        asm_push("neg eax");
        asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = last_dtype});
    }
    else if (S_EQ(node->unary.op, "~"))
    {
        asm_push("not eax");
        asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = last_dtype});
    }
    else if (S_EQ(node->unary.op, "*"))
    {
        // We are accessing a pointer
        codegen_generate_unary_indirection(node, history);
    }
    else if (S_EQ(node->unary.op, "++"))
    {
        if (node->unary.flags & UNARY_FLAG_IS_RIGHT_OPERANDED_UNARY)
        {
            // Save the value as this is a "x++" i.e we should return x before incrementing
            asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = last_dtype});
            asm_push("inc eax");
            asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = last_dtype});
            codegen_generate_assignment_part(node->unary.operand, "=", history);
            // No need to pop EAX back off that we did above the "inc" as the receiver expects to pop from the stack
            // anyway..
        }
        else
        {
            asm_push("inc eax");
            asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = last_dtype});
            codegen_generate_assignment_part(node->unary.operand, "=", history);
            asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = last_dtype});
        }
    }
    else if (S_EQ(node->unary.op, "--"))
    {
        if (node->unary.flags & UNARY_FLAG_IS_RIGHT_OPERANDED_UNARY)
        {
            // Save the value as this is a "x--" i.e we should return x before decerementing
            asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = last_dtype});
            asm_push("dec eax");
            asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = last_dtype});
            codegen_generate_assignment_part(node->unary.operand, "=", history);
            // No need to pop EAX back off that we did above the "inc" as the receiver expects to pop from the stack
            // anyway..
        }
        else
        {
            asm_push("dec eax");
            asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = last_dtype});
            codegen_generate_assignment_part(node->unary.operand, "=", history);
            asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = last_dtype});
        }
    }
    else if (S_EQ(node->unary.op, "!"))
    {
        // We have a logical not so preform it
        asm_push("cmp eax, 0");
        asm_push("sete al");
        asm_push("movzx eax, al");
        asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = last_dtype});
    }
}

void codegen_generate_unary(struct node *node, struct history *history)
{
    int flags = history->flags;

    // Can we resolve a unary..
    if (codegen_resolve_node_for_value(node, history))
    {
        return;
    }

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
    codegen_gen_mov_for_value("eax", label, "dword", history->flags);
    asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = datatype_for_string()});
}

void codegen_generate_cast(struct node *node, struct history *history)
{
    // Can we resolve the current node ?
    if (!codegen_resolve_node_for_value(node, history))
    {
        // Nope.. lets generate an expressionable on the operand.
        codegen_generate_expressionable(node->cast.operand, history);
    }

    // We must reduce EAX
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    codegen_reduce_register("eax", datatype_size(&node->cast.dtype), node->cast.dtype.flags & DATATYPE_FLAG_IS_SIGNED);
    asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, &(struct stack_frame_data){.dtype = node->cast.dtype});
}

void codegen_generate_tenary(struct node *node, struct history *history)
{
    int true_label_id = codegen_label_count();
    int false_label_id = codegen_label_count();
    int tenary_end_label_id = codegen_label_count();

    struct datatype last_dtype;
    assert(asm_datatype_back(&last_dtype));

    // Pop off the result for the tenary
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

    // Condition node would have already been generated as tenaries are
    // nested into an expression
    asm_push("cmp eax, 0");
    asm_push("je .tenary_false_%i", false_label_id);
    asm_push(".tenary_true_%i:", true_label_id);

    register_unset_flag(REGISTER_EAX_IS_USED);
    // Now we generate the true condition
    codegen_generate_new_expressionable(node->tenary.true_node, history_down(history, 0));
    asm_push_ins_pop_or_ignore("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

    asm_push("jmp .tenary_end_%i", tenary_end_label_id);

    asm_push(".tenary_false_%i:", false_label_id);
    // Now the false condition
    register_unset_flag(REGISTER_EAX_IS_USED);
    codegen_generate_new_expressionable(node->tenary.false_node, history_down(history, 0));
    asm_push_ins_pop_or_ignore("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
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

    case NODE_TYPE_CAST:
        codegen_generate_cast(node, history);
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

void codegen_generate_scope_no_new_scope(struct vector *statements, struct history *history);
void codegen_generate_stack_scope(struct vector *statements, size_t scope_size, struct history *history)
{
    // New body new scope.

    // Resolver scope needs to exist too it will be this normal scopes replacement
    codegen_new_scope(RESOLVER_SCOPE_FLAG_IS_STACK);
    codegen_generate_scope_no_new_scope(statements, history);
    codegen_finish_scope();
}

void codegen_generate_body(struct node *node, struct history *history)
{
    codegen_generate_stack_scope(node->body.statements, node->body.size, history);
}
void codegen_generate_scope_variable(struct node *node)
{
    // Register the variable to the scope.
    struct resolver_entity *entity = codegen_new_scope_entity(node, node->var.aoffset, RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);

    // Scope variables have values, lets compute that
    if (node->var.val)
    {
        struct history history;
        codegen_generate_expressionable(node->var.val, history_down(&history, EXPRESSION_IS_ASSIGNMENT | IS_RIGHT_OPERAND_OF_ASSIGNMENT));
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        const char *reg_to_use = "eax";
        const char *mov_type = codegen_byte_word_or_dword_or_ddword(datatype_element_size(&entity->dtype), &reg_to_use);
        codegen_generate_assignment_instruction_for_operator(mov_type, codegen_entity_private(entity)->address, reg_to_use, "=", entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
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

void codegen_generate_statement_return_exp(struct node *node)
{
    struct history history;
    codegen_response_expect();
    // Let's generate the expression of the return statement
    codegen_generate_expressionable(node->stmt.ret.exp, history_begin(&history, IS_STATEMENT_RETURN));

    struct datatype dtype;
    assert(asm_datatype_back(&dtype));

    if (datatype_is_struct_or_union_non_pointer(&dtype))
    {
        // Returning a structure from a function? Things must be done differently
        // Firslty lets access the structure pointer to return.
        // It will be EBP+8 i.e first argument..
        asm_push("mov edx, [ebp+8]");
        // EBX EDX contains the address to the structure.
        // Now we must make a move
        codegen_generate_move_struct(&dtype, "edx", 0);
        // Eax should also contain a pointer to this structure
        asm_push("mov eax, [ebp+8]");
        return;
    }

    // Restore return value from stack
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
}

void codegen_generate_statement_return(struct node *node)
{
    if (node->stmt.ret.exp)
    {
        codegen_generate_statement_return_exp(node);
    }
    // Generate the stack subtraction.
    codegen_stack_add_no_compile_time_stack_frame_restore(C_ALIGN(function_node_stack_size(node->binded.function)));

    // Now we must leave the function
    asm_pop_ebp_no_stack_frame_restore();
    asm_push("ret");
}

void _codegen_generate_if_stmt(struct node *node, int end_label_id);
void codegen_generate_else_stmt(struct node *node)
{
    struct history history;
    codegen_generate_body(node->stmt._else.body_node, history_begin(&history, 0));
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
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("cmp eax, 0");
    asm_push("je .if_%i", if_label_id);
    // Unset the EAX register flag we are not using it now
    register_unset_flag(REGISTER_EAX_IS_USED);
    codegen_generate_body(node->stmt._if.body_node, history_begin(&history, IS_ALONE_STATEMENT));
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
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

    asm_push("cmp eax, 0");
    asm_push("je .while_end_%i", while_end_id);
    // Okay, let us now generate the body
    codegen_generate_body(node->stmt._while.body, history_begin(&history, IS_ALONE_STATEMENT));
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
    codegen_generate_body(node->stmt._do_while.body, history_begin(&history, IS_ALONE_STATEMENT));
    codegen_generate_brand_new_expression(node->stmt._do_while.cond, history_begin(&history, 0));
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

    asm_push("cmp eax, 0");
    asm_push("jne .do_while_start_%i", do_while_start_id);
    codegen_end_entry_exit_point();
}

void codegen_generate_for_stmt(struct node *node)
{
    struct for_stmt *for_stmt = &node->stmt._for;
    int for_loop_start_id = codegen_label_count();
    int for_loop_end_id = codegen_label_count();
    struct history history;

    if (for_stmt->init)
    {
        // We have our FOR loop initialization, lets initialize.
        codegen_generate_brand_new_expression(for_stmt->init, history_begin(&history, 0));
        asm_push_ins_pop_or_ignore("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }

    asm_push("jmp .for_loop%i", for_loop_start_id);
    codegen_begin_entry_exit_point();
    if (for_stmt->loop)
    {
        codegen_generate_brand_new_expression(for_stmt->loop, history_begin(&history, 0));
        asm_push_ins_pop_or_ignore("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }
    asm_push(".for_loop%i:", for_loop_start_id);
    if (for_stmt->cond)
    {
        // We have our FOR loop condition, lets condition it.
        codegen_generate_brand_new_expression(for_stmt->cond, history_begin(&history, 0));
        asm_push_ins_pop_or_ignore("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

        asm_push("cmp eax, 0");
        asm_push("je .for_loop_end%i", for_loop_end_id);
    }

    if (for_stmt->body)
    {
        codegen_generate_body(for_stmt->body, history_begin(&history, IS_ALONE_STATEMENT));
    }

    if (for_stmt->loop)
    {
        codegen_generate_brand_new_expression(for_stmt->loop, history_begin(&history, 0));
        asm_push_ins_pop_or_ignore("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
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
    asm_push_ins_pop_or_ignore("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

    codegen_generate_switch_stmt_case_jumps(node);

    // Let's generate the body
    codegen_generate_body(node->stmt._switch.body, history_begin(&history, IS_ALONE_STATEMENT));

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

void codegen_discard_unused_stack()
{
    asm_stack_peek_start();
    struct stack_frame_element *element = asm_stack_peek();
    size_t stack_adjustment = 0;
    while (element)
    {
        if (!S_EQ(element->name, "result_value"))
            break;

        stack_adjustment += DATA_SIZE_DWORD;
        element = asm_stack_peek();
    }

    codegen_stack_add(stack_adjustment);
}
void codegen_generate_statement(struct node *node, struct history *history)
{
    switch (node->type)
    {

    case NODE_TYPE_EXPRESSION:
        codegen_generate_exp_node(node, history_begin(history, history->flags));
        break;

    case NODE_TYPE_UNARY:
        codegen_generate_unary(node, history_begin(history, history->flags));
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

    codegen_discard_unused_stack();
}

void codegen_generate_scope_no_new_scope(struct vector *statements, struct history *history)
{
    vector_set_peek_pointer(statements, 0);
    struct node *statement_node = vector_peek_ptr(statements);
    while (statement_node)
    {
        codegen_generate_statement(statement_node, history);
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

    codegen_generate_function_arguments(function_node_argument_vec(node));

    struct history history;

    // Generate the function body
    codegen_generate_body(node->func.body_n, history_begin(&history, IS_ALONE_STATEMENT));

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

void codegen_generate_data_section_add_ons()
{
    asm_push("section .data");
    vector_set_peek_pointer(current_process->generator->custom_data_section, 0);
    const char *str = vector_peek_ptr(current_process->generator->custom_data_section);
    while (str)
    {
        asm_push(str);
        str = vector_peek_ptr(current_process->generator->custom_data_section);
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

    codegen_generate_data_section_add_ons();

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
    generator->custom_data_section = vector_create(sizeof(const char *));
    return generator;
}