#include "file_utils.h"
#include "string_utils.h"
#include "time_utils.h"
#include <stdio.h>
#include <sys/stat.h>
#include "dirent.h"
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "nilm_algo.h"
#include "fft.h"
static char *dirPath = "F:\\Tmp";

static float Cleaner1[8] = { 3.400, 2.600, 1.400, 0.700, 0.500, 0.550, 4.300, 382.300};
static float Cleaner2[8] = {6.700, 1.000, 0.300, 0.100, 0.100, 0.990, 7.100, 1028.400};
static float Cleaner3[8] = {3.900, 2.900, 1.400, 0.700, 0.400, 0.530, 3.800, 418.500};
static float Cleaner4[8] = {7.800, 1.400, 0.300, 0.100, 0.100, 0.980, 5.600, 1198.000};

int nilm_main() {

    int ret = nilmInit();
    if (ret > 1) {
        printf("some error\n");
        return -1;
    }
    DIR *dir = NULL;
    struct dirent *entry;

    int pointNum = 0, fileIndex = 0;
    if ((dir = opendir(dirPath)) == NULL) {
        printf("opendir failed\n");
    } else {
        char csv[128];
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0
                    || !endWith(entry->d_name, ".csv") || !startWith(entry->d_name, "0")) ///current dir OR parrent dir
                continue;
            fileIndex = 0;
            memset(csv, 0, sizeof(csv));
            snprintf(csv, sizeof(csv) - 1, "%s\\%s", dirPath, entry->d_name);
//            printf("filepath=%s filetype=%d\n", csv, entry->d_type);        //输出文件或者目录的名称
            /**parse csv*/
            float current[128], voltage[128];
            FILE *f = fopen(csv, "rb");
            printf("%s\n",csv);
//            FILE *f = fopen(csvPath, "rb");
            int i = 0;
            while (!feof(f) && !ferror(f)) {
                float cur, vol;
                fscanf(f, "%f,%f", &cur, &vol);
                current[i] = cur;
                voltage[i] = vol;
                fileIndex++;
                i++;
                if (i >= 128) {
                    float fft[128],oddFft[5];
                    do_fft(current, fft);

                    for(int j=0;j<5;j++){
                        oddFft[j] = fft[j * 2 + 1];
                    }
                    nilmAnalyze(current, voltage, 128, getCurTime(), -1, -1, -1, oddFft);
                    i = 0;
                }
                pointNum++;
            }
            fclose(f);
        }

    }
    if (dir != NULL)
        closedir(dir);

    return 0;
}
