/* object-specific code */
#ifndef k_objects_H
#define k_objects_H

/* Hyudoro */
void Obj_HyudoroDeploy(mobj_t *master);
void Obj_HyudoroThink(mobj_t *actor);
void Obj_HyudoroCenterThink(mobj_t *actor);
void Obj_HyudoroCollide(mobj_t *special, mobj_t *toucher);

/* Item Debris */
fixed_t Obj_GetItemDebrisSpeed(mobj_t *collector, fixed_t min_speed);
void Obj_SpawnItemDebrisEffects(mobj_t *collectible, mobj_t *collector);
void Obj_ItemDebrisThink(mobj_t *debris);
fixed_t Obj_ItemDebrisBounce(mobj_t *debris, fixed_t momz);

#endif/*k_objects_H*/
