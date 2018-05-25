#include "logger.h"
#include "refer_count.h"

struct my_test {
    int        num;
    char    *name;
};

/***the function is used for destory struct malloc***/
static void my_test_destructor(void *obj)
{
    logger(NOTICE, "my_test_destructor, obj: %p", obj);
}

int main()
{
    struct my_test *test = ASTOBJ_MALLOC(sizeof(*test), my_test_destructor);
    logger(NOTICE, "test: %p", test);
    test->num = 20;
    test->name = "Hello World";

    ast_ref(test, +1);

    ast_ref(test, -2);

    return 0;
}
