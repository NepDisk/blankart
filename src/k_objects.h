/* object-specific code */
#ifndef k_objects_H
#define k_objects_H

#include "taglist.h"

/* Loops */
mobj_t *Obj_FindLoopCenter(const mtag_t tag);
void Obj_InitLoopEndpoint(mobj_t *end, mobj_t *anchor);
void Obj_InitLoopCenter(mobj_t *center);
void Obj_LinkLoopAnchor(mobj_t *anchor, mobj_t *center, UINT8 type);
void Obj_LoopEndpointCollide(mobj_t *special, mobj_t *toucher);

#endif/*k_objects_H*/
