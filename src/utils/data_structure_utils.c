
#include <data_structure_utils.h>
#include<string.h>

// 按照顺序插入，最新插入的数据排在buff后面
void insertFloatArrayToBuff(float buff[], int buffSize, float newData[], int dataLen) {

    int end = buffSize - dataLen;
    for (int i = 0; i < end; i++) {
        buff[i] = buff[i + dataLen];
    }

    for (int i = 0; i < dataLen; i++) {
        buff[buffSize - dataLen + i] = newData[i];
    }
}

// 只插入一条，最新插入的数据排在buff后面
 void insertFloatToBuff(float buff[], int buffSize, float newData) {

    int end = buffSize - 1;
    for (int i = 0; i < end; i++) {
        buff[i] = buff[i + 1];
    }
    buff[buffSize - 1] = newData;
}

/**
 * demo: insertFifoCommon((char*) test23, sizeof(test23), (char*) &t1, sizeof(t1))
 * 始终插入到最后一个位置;需要数据本身支持可以判断有效性
 * buffByteSize: 队列占用的字节大小，注意不是队列长度
 */
void insertPackedDataToBuff(char *buff, int buffByteSize, char *newData, int dataLen) {

    if (buffByteSize % dataLen != 0 || dataLen > buffByteSize)
        return;
    memcpy(buff, buff + dataLen, buffByteSize - dataLen);
    memcpy(buff + buffByteSize - dataLen, newData, dataLen);
}
