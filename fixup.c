#include "compiler.h"
enum
{
    FIXUP_RESOLVED = 0b00000001
};

struct fixup_system *fixup_sys_new()
{
    struct fixup_system *system = calloc(sizeof(struct fixup_system), 1);
    system->fixups = vector_create(sizeof(struct fixup));
    return system;
}

struct fixup_config *fixup_config(struct fixup *fixup)
{
    return &fixup->config;
}

void fixup_free(struct fixup *fixup)
{
    fixup_config(fixup)->end(fixup);
    free(fixup);
}

void fixup_sys_fixups_free(struct fixup_system *system)
{
    fixup_start_iteration(system);
    struct fixup *fixup = fixup_next(system);
    while (fixup)
    {
        fixup_free(fixup);
        fixup = fixup_next(system);
    }
}

void fixup_sys_finish(struct fixup_system *system)
{
    fixup_sys_fixups_free(system);
    vector_free(system->fixups);
    free(system);
}

int fixup_sys_unresolved_fixups_count(struct fixup_system *system)
{
    size_t c = 0;
    fixup_start_iteration(system);
    struct fixup *fixup = fixup_next(system);
    while (fixup)
    {
        if (fixup->flags & FIXUP_RESOLVED)
        {
            fixup = fixup_next(system);
            continue;
        }

        c++;
        fixup = fixup_next(system);
    }

    return c;
}

void fixup_start_iteration(struct fixup_system *system)
{
    vector_set_peek_pointer(system->fixups, 0);
}

struct fixup *fixup_next(struct fixup_system *system)
{
    return vector_peek_ptr(system->fixups);
}

struct fixup *fixup_register(struct fixup_system *system, struct fixup_config *config)
{
    struct fixup *fixup = calloc(sizeof(struct fixup), 1);
    memcpy(&fixup->config, config, sizeof(struct fixup_config));
    fixup->system = system;
    vector_push(system->fixups, &fixup);
    return fixup;
}

bool fixup_resolve(struct fixup *fixup)
{
    // We must resolve this fixup
    if (fixup_config(fixup)->fix(fixup))
    {
        // A successful fix, lets mark this fixup as resolved
        fixup->flags |= FIXUP_RESOLVED;
        return true;
    }

    return false;
}

void *fixup_private(struct fixup *fixup)
{
    return fixup_config(fixup)->private;
}

bool fixups_resolve(struct fixup_system *system)
{
    fixup_start_iteration(system);
    struct fixup *fixup = fixup_next(system);
    while (fixup)
    {
        if (fixup->flags & FIXUP_RESOLVED)
        {
            continue;
        }

        fixup_resolve(fixup);
        fixup = fixup_next(system);
    }

    return fixup_sys_unresolved_fixups_count(system) == 0;
}