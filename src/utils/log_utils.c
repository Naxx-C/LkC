#include <stdlib.h>

int (*outprintf)(const char*, ...) = NULL;

void registerPrintf(int print(const char*, ...)) {
    outprintf = print;
}
