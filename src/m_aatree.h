// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  m_aatree.h
/// \brief AA trees code

#ifndef __M_AATREE__
#define __M_AATREE__

#include "doomtype.h"

// Flags for AA trees.
#define AATREE_ZUSER	1		// Treat values as z_zone-allocated blocks and set their user fields

typedef void (*aatree_iter_t)(INT32 key, void *value);
typedef struct aatree_iterator_s aatree_iterator_t;

aatree_t *M_AATreeAlloc(UINT32 flags);
void M_AATreeFree(aatree_t *aatree);
void M_AATreeSet(aatree_t *aatree, INT32 key, void* value);
void *M_AATreeGet(aatree_t *aatree, INT32 key);
boolean M_AATreeEmpty(aatree_t *aatree);
size_t M_AATreeSize(aatree_t *aatree);
void M_AATreeIterate(aatree_t *aatree, aatree_iter_t callback);

aatree_iterator_t *M_AATreeBegin(aatree_t *aatree);
aatree_iterator_t *M_AATreeRBegin(aatree_t *aatree);
void *M_AATreeIteratorNext(aatree_iterator_t *iterator);
INT32 M_AATreeIteratorKey(aatree_iterator_t *iterator);
void *M_AATreeIteratorValue(aatree_iterator_t *iterator);
void M_AATreeIteratorClose(aatree_iterator_t *iterator);

#endif
