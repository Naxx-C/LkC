#include <math.h>
#include <string.h>

#define PI 3.14159265f
float getCosineSimilarity(float a[], float b[], int start, int len) {
    if (a == NULL || b == NULL) {
        return -1;
    }

    float numerator = 0;
    float denominatorA = 0;
    float denominatorB = 0;
    int end = start + len;
    for (int i = start; i < end; i++) {
        numerator += a[i] * b[i];
        denominatorA += a[i] * a[i];
        denominatorB += b[i] * b[i];
    }

    float cosine = (numerator / (sqrt(denominatorA) * sqrt(denominatorB)));

    float cosineSimilarity = (float) (acos(cosine) * 180 / PI);
    return cosineSimilarity;
}

float getAverageValue(float *data, int len) {
    float sum = 0;
    for (int i = 0; i < len; i++)
        sum += data[i];

    return sum / len;
}


float getThreshAverage(float *data, int len, float thresh) {

    float sum = 0;
    int counter = 0;
    for (int i = 0; i < len; i++) {
        if (data[i] > thresh) {
            sum += data[i];
            counter++;
        }
    }
    if (counter == 0)
        return 0;
    return sum / counter;
}
