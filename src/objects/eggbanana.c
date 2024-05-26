#include "../doomdef.h"
#include "../doomstat.h"
#include "../info.h"
#include "../k_kart.h"
#include "../k_objects.h"
#include "../m_random.h"
#include "../p_local.h"
#include "../r_main.h"
#include "../s_sound.h"
#include "../g_game.h"
#include "../z_zone.h"
#include "../k_waypoint.h"
#include "../k_respawn.h"
#include "../k_collide.h"


void Obj_EggBananaThink(mobj_t *mobj)
{
	mobj->friction = ORIG_FRICTION/4;
	if (mobj->momx || mobj->momy)
		P_SpawnGhostMobj(mobj);
	if (P_IsObjectOnGround(mobj) && mobj->health > 1)
	{
		S_StartSound(mobj, mobj->info->activesound);
		mobj->momx = mobj->momy = 0;
		mobj->health = 1;
	}

	if (mobj->threshold > 0)
		mobj->threshold--;
}
