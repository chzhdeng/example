#ifndef __REFER_COUNT_H__
#define __REFER_COUNT_H__

#include "logger.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define ast_free free

#define AST_SUCCESS    0
#define    AST_FAIL    -1

#define ASTOBJ_MAGIC    0xa570b123

#define AST_ASSERT(data) do {        \
        if (!(data)) {               \
            assert((data));          \
            return AST_FAIL;         \
        }                            \
    } while (0)

typedef void (*dtor_fn)(void *obj);

struct astobj_region {
    const char        *file;
    int                line;
    unsigned char    data[0] __attribute__((aligned));
};

struct astobj {
    unsigned int    ref_count;
    dtor_fn            dtor_function;
    unsigned int    magic;
    void            *user_data[0];
};

static void *astobj_region_alloc(size_t size, const char *file, int line)
{
    struct astobj_region *region = malloc(size + sizeof(*region));
    if (region == NULL) {
        logger(ERROR, "Malloc memory failed!");
        return NULL;
    }

    region->file = file;
    region->line = line;

    return region->data;
}

static void *__astobj_malloc(size_t obj_size, dtor_fn dtor_function, const char *file, int line)
{
    struct astobj *obj = NULL;
    void *ptr = astobj_region_alloc(obj_size + sizeof(struct astobj), file, line);
    if (ptr == NULL) {
        logger(ERROR, "Malloc memory failed!");
        return NULL;
    }

    memset(ptr, 0, obj_size + sizeof(struct astobj));
    obj = (struct astobj *)ptr;
    obj->ref_count = 1;
    obj->dtor_function = dtor_function;
    obj->magic = ASTOBJ_MAGIC;

    return obj->user_data;
}

#define ASTOBJ_MALLOC(obj_size, dtor_function) __astobj_malloc((obj_size), (dtor_function), __FILE__, __LINE__)

#define ASTOBJ(user_data) ((user_data) - sizeof(struct astobj))

#define ASTOBJ_REGION(data) ((data) - sizeof(struct astobj_region))

static void astobj_region_ref(void *obj, const char *file, int line)
{
    unsigned char *ptr = (unsigned char *)obj;
    struct astobj_region *region = (struct astobj_region *)ASTOBJ_REGION(ptr);
    if (region) {
        ast_free(region);
        region = NULL;
    }
}

static int __ast_ref(void *user_data, int delta, const char *file, int line)
{
    struct astobj *obj = ASTOBJ(user_data);
    AST_ASSERT(obj);

    if (obj->magic != ASTOBJ_MAGIC) {
        logger(ERROR, "Bad magic number 0x%x for object %p!", obj, obj->ref_count);
        return -1;
    }

    __sync_fetch_and_add(&obj->ref_count, delta);
    //logger(DEBUG, "obj->ref_count: %d", obj->ref_count);

    //ref_count > 0 ,dont't handle anything.
    if (0 < obj->ref_count) {
        return -1;
    }

    //ref_count <= 0 ,clean resource of malloc.
    if (obj->dtor_function) {
        logger(DEBUG, "------------delta: %d---------", delta);
        obj->dtor_function(user_data);
    }

    obj->magic = 0;

    astobj_region_ref(obj, file, line);

    return 0;
}

#define    ast_ref(user_data, delta) __ast_ref((user_data), (delta), __FILE__, __LINE__)

#define astobj_cleanup(user_data) ast_ref((user_data), -1)

#endif
