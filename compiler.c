#include "compiler.h"
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

void test(struct node* node)
{
    if (node->type == NODE_TYPE_NUMBER)
    {
        printf("%i", node->inum);
    }
    else if (node->type == NODE_TYPE_EXPRESSION)
    {
        test(node->exp.left);
        printf("%s", node->exp.op);
        test(node->exp.right);

    }
}
int compile_file(const char *filename)
{
    struct compile_process *process = compile_process_create(filename);
    if (!process)
        return COMPILER_FAILED_WITH_ERRORS;

    if (lex(process) != LEXICAL_ANALYSIS_ALL_OK)
        return COMPILER_FAILED_WITH_ERRORS;

    if (parse(process) != PARSE_ALL_OK)
        return COMPILER_FAILED_WITH_ERRORS;

    for (int i = 0; i < vector_count(process->node_vec); i++)
    {
        struct node *ptr;
        ptr = *((struct node **)(vector_at(process->node_vec, i)));
        test(ptr);
    }
    return COMPILER_FILE_COMPILED_OK;
}