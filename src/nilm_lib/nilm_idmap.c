#include <stddef.h>
#include <string.h>
#include "nilm_idmap.h"
#include "nilm_appliance.h"

void addToIdMap(IdMap* idMaps, char mapsLen, char id, char* name, int nameLen) {
    if (idMaps == NULL || id >= mapsLen)
        return;

    int index = (int)id;
    memset(idMaps[index].name, 0, sizeof(idMaps[index].name));
    memcpy(idMaps[index].name, name, nameLen);
}

void removeInIdMap(IdMap* idMaps, char mapsLen, char id, char* name, int nameLen) {
    if (idMaps == NULL || id >= mapsLen)
        return;

    int index = (int)id;
    idMaps[index].id = -1;
    memset(idMaps[index].name, 0, sizeof(idMaps[index].name));
}

void getNameInIdMap(IdMap* idMaps, char mapsLen, char id, char* name) {
    if (idMaps == NULL || id >= mapsLen)
        return;

    int index = (int)id;
    memcpy(name, idMaps[index].name, sizeof(idMaps[index].name));
}

char getIdInIdMap(IdMap* idMaps, char mapsLen, char* name) {
    if (idMaps == NULL)
        return -1;
    for (int i = 0; i < mapsLen; i++) {
        if(strncmp(idMaps[i].name,name,strlen(idMaps[i].name)) == 0)
            return i;
    }
    return -1;
}

//TODO
void getAllIdMap(IdMap* idMaps, char mapsLen, char* buff) {

}
