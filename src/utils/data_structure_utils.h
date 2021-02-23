#ifndef UTILS_DATA_STRUCTURE_H_
#define UTILS_DATA_STRUCTURE_H_

//插入先入先出数组.从数组最后的位置插入，队列满挤掉第一个
void insertFloatArrayToBuff(float buff[], int buffSize, float newData[], int dataLen);
void insertFloatToBuff(float buff[], int buffSize, float newData);
void insertCharToBuff(char buff[], int buffSize, char newData);
void insertPackedDataToBuff(char *buff, int buffSize, char *newData, int dataLen);

#endif
