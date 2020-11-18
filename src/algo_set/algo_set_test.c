#include "file_utils.h"
#include "string_utils.h"
#include "time_utils.h"
#include "algo_set.h"
#include <stdio.h>
#include <sys/stat.h>
#include "dirent.h"
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include "fft.h"
#ifdef _WIN32
#endif

//static char *dirPath = "F:\\Tmp\\tiaoya";
//static char *dirPath = "F:\\Tmp\\charging_yes";
//static char *dirPath = "F:\\Tmp\\maliload";
static char *dirPath = "F:\\Tmp\\dingpinkongtiao";

static int init(){

    return 0;
}

int algo_set_test() {

    init();
    DIR *dir = NULL;
    struct dirent *entry;

    int pointNum = 0, fileIndex = 0;
    if ((dir = opendir(dirPath)) == NULL) {
        printf("opendir failed\n");
    } else {
        char csv[128];
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0
                    || !endWith(entry->d_name, ".csv")) //016152951
                continue;
            initTpsonAlgoLib();
            int convertHardcode = 1;
            if (startWith(entry->d_name, "elec_"))
                convertHardcode = 1;//TODO:11月之前的，单相电抓的数据是反的,需要-1

            fileIndex = 0;
            memset(csv, 0, sizeof(csv));
            snprintf(csv, sizeof(csv) - 1, "%s\\%s", dirPath, entry->d_name);
            printf("filepath=%s filetype=%d\n", csv, entry->d_type); //输出文件或者目录的名称
            /**parse csv*/
            float current[128], voltage[128];
            FILE *f = fopen(csv, "rb");
            printf("%s\n", csv);
//            FILE *f = fopen(csvPath, "rb");
            int i = 0;
            while (!feof(f) && !ferror(f)) {
                float cur, vol;
                fscanf(f, "%f,%f", &cur, &vol);
                current[i] = cur * convertHardcode;
                voltage[i] = vol;
                fileIndex++;
                i++;
                if (i >= 128) {
                    feedData(current, 0, voltage, 0, time(NULL), NULL);
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
