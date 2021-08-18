#include "compiler.h"
#include "misc.h"
#include "helpers/vector.h"
void compiler_error(struct compile_process *compiler, const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);

    fprintf(stderr, " on line %i, col %i\n", compiler->pos.line, compiler->pos.col);

    exit(-1);
}

void test(struct node *node);
void test_vec(struct vector *vec)
{
    for (int i = 0; i < vector_count(vec); i++)
    {
        struct node *node = *((struct node **)(vector_at(vec, i)));
        test(node);
    }
}
void test(struct node *node)
{
    if (node->type == NODE_TYPE_NUMBER)
    {
        printf("%i", node->inum);
    }
    else if (node->type == NODE_TYPE_IDENTIFIER)
    {
        printf("%s", node->sval);
    }
    else if (node->type == NODE_TYPE_EXPRESSION)
    {
        printf("%c", '[');
        test(node->exp.left);
        printf("%s", node->exp.op);
        if (node->exp.right)
            test(node->exp.right);
        printf("%c", ']');
        printf("\n");
    }
    else if (node->type == NODE_TYPE_UNARY)
    {
        printf("%c", '{');
        printf("%c", '*');
        test(node->unary.operand);
        printf("%c", '}');
    }
    else if (node->type == NODE_TYPE_EXPRESSION_PARENTHESIS)
    {
        test(node->parenthesis.exp);
    }
    else if (node->type == NODE_TYPE_FUNCTION)
    {
        if (node->func.argument_vector)
            test_vec(node->func.argument_vector);

        if (node->func.body_n && node->func.body_n->body.statements)
            test_vec(node->func.body_n->body.statements);
    }
    else if (node->type == NODE_TYPE_BRACKET)
    {
        test(node->bracket.inner);
    }
}

/**
 * Includes a file to be compiled, returns a new compile process that represents the file
 * to be compiled.
 * 
 * Only lexical analysis, and preprocessing are done for compiler includes
 * Parsing and code generation is excluded.
 */
struct compile_process *compile_include(const char *filename, struct compile_process *parent_process)
{
    char tmp_filename[512];
    sprintf(tmp_filename, "/usr/include/%s", filename);
    if(file_exists(tmp_filename))
    {
        filename = tmp_filename;
    }

    struct compile_process *process = compile_process_create(filename, NULL, parent_process->flags, parent_process);
    if (!process)
        return NULL;

    if (lex(process) != LEXICAL_ANALYSIS_ALL_OK)
        return NULL;

    if (preprocessor_run(process) != 0)
    {
        return NULL;
    }

    return process;
}

int compile_file(const char *filename, const char* out_filename, int flags)
{
    struct compile_process *process = compile_process_create(filename, out_filename, flags, NULL);
    if (!process)
        return COMPILER_FAILED_WITH_ERRORS;

    if (lex(process) != LEXICAL_ANALYSIS_ALL_OK)
        return COMPILER_FAILED_WITH_ERRORS;

    if (preprocessor_run(process) != 0)
    {
        return COMPILER_FAILED_WITH_ERRORS;
    }

    // Symbol resolution is now done during parsing..
    if (parse(process) != PARSE_ALL_OK)
        return COMPILER_FAILED_WITH_ERRORS;

    for (int i = 0; i < vector_count(process->node_tree_vec); i++)
    {
        struct node *ptr;
        ptr = *((struct node **)(vector_at(process->node_tree_vec, i)));
        test(ptr);
    }
    printf("\n");
    // Do validation here..

    if (codegen(process) != CODEGEN_ALL_OK)
        return COMPILER_FAILED_WITH_ERRORS;

    compile_process_destroy(process);
    return COMPILER_FILE_COMPILED_OK;
}