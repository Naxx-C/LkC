#include "string_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
//"0123" 4 "23" 2
int endWith(char *src, char *sub) {

    int srcLen=strlen(src);
    int subLen = strlen(sub);
    if (src == NULL || sub == NULL || srcLen < subLen)
        return 0;
    return strncmp(src + srcLen - subLen, sub, subLen) == 0;
}

void subString(char *src, int start,int len,char* sub) {

    if (src == NULL)
        return;
    strncpy(sub, src + start, len);
}

int startWith(char *src, char *sub) {

    int srcLen=strlen(src);
    int subLen = strlen(sub);
    if (src == NULL || sub == NULL || srcLen < subLen)
        return 0;
    return strncmp(src, sub, subLen) == 0;
}
