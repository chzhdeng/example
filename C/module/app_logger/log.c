#include "logger.h"

int main()
{
    logger(NOTICE, "print logger !!!!!");

    char * name = "chzh deng";
    logger(NOTICE, "output name: %s", name);

    return 0;
}
