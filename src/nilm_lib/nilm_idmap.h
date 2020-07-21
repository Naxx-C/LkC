#ifndef __NILM_IDMAP
#define __NILM_IDMAP
#include "nilm_appliance.h"

typedef struct {

    signed char id;
    char name[APP_NAME_LEN];
} IdMap;

void addToIdMap(IdMap* idMaps, char mapsLen, char id, char* name, int nameLen);
void getNameInIdMap(IdMap* idMaps, char mapsLen, char id, char* name);
void removeInIdMap(IdMap* idMaps, char mapsLen, char id, char* name, int nameLen);
#endif
