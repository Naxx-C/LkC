#include "file_utils.h"
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#define BUFFER_SIZE 1024

//char fileContent[100000];
//memset(fileContent,0,sizeof(fileContent));
//readFile(src,fileContent,sizeof(fileContent));
//printf("%s\n",fileContent);
void readFile(char *path, char *outBuff, int bufSize) {
   int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open failed");
        return;
    }
    read(fd, outBuff, bufSize);
    close(fd);
}

void readFileTest(char *path, char *outBuff, int bufSize) {
    FILE *fp = NULL;
    //读写方式打开，如果文件不存在，打开失败
    fp = fopen(path, "r+");
    if (fp == NULL) {
        perror("my_fgets fopen");
        return;
    }
    while (!feof(fp)) //文件没有结束
    {
        char *p = fgets(outBuff, bufSize, fp);
        if (p != NULL) {
            printf("buf = %s\n", outBuff);
        }
    }
}

void copyFile(char *src, char *dst, char *outErrInfo) {
    int srcfd, dstFd;
    int bytes_read = 0, bytes_write;
    char buffer[BUFFER_SIZE];
    char *ptr;
    if ((srcfd = open(src, O_RDONLY)) == -1) {
        snprintf(outErrInfo, 128, "open src failed\n");
    }
    if ((dstFd = open(dst, O_WRONLY | O_CREAT)) == -1) {
        snprintf(outErrInfo, 128, "create dst failed\n");
        printf("errno=%d\n",errno);
    }
    while ((bytes_read = read(srcfd, buffer, BUFFER_SIZE)) > 0) {
        if ((bytes_read == -1) && (errno != EINTR)) {
            snprintf(outErrInfo, 128, "\n -- read error ! \n");
            break;
        } else if (bytes_read > 0) {
            ptr = buffer;
            while ((bytes_write = write(dstFd, ptr, bytes_read)) > 0) {
                if ((bytes_write == -1) && (errno != EINTR)) {
                    snprintf(outErrInfo, 128, "\n -- write error ! \n");
                    break;
                } else if (bytes_write == bytes_read)
                    break;
                else if (bytes_write > 0) {
                    ptr += bytes_write;
                    bytes_read -= bytes_write;
                }
            }
            if (bytes_write == -1) {
                snprintf(outErrInfo, 128, "\n -- write error ! \n");
                break;
            }
        }
    }
    close(srcfd);
    close(dstFd);
    return;
}
