// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  byteptr.h
/// \brief Macros to read/write from/to a UINT8 *,
///        used for packet creation and such

#if defined (__alpha__) || defined (__arm__) || defined (__mips__) || defined (__ia64__) || defined (__clang__)
#define DEALIGNED
#endif

#include "endian.h"

#ifndef SRB2_BIG_ENDIAN
//
// Little-endian machines
//
#ifdef DEALIGNED
#define WRITEUINT8(p,b)     do {   UINT8 *p_tmp = (void *)p; const   UINT8 tv = (  UINT8)(b); memcpy(p, &tv, sizeof(  UINT8)); p_tmp++; p = (void *)p_tmp; } while (0)
#define WRITESINT8(p,b)     do {   SINT8 *p_tmp = (void *)p; const   SINT8 tv = (  UINT8)(b); memcpy(p, &tv, sizeof(  UINT8)); p_tmp++; p = (void *)p_tmp; } while (0)
#define WRITEINT16(p,b)     do {   INT16 *p_tmp = (void *)p; const   INT16 tv = (  INT16)(b); memcpy(p, &tv, sizeof(  INT16)); p_tmp++; p = (void *)p_tmp; } while (0)
#define WRITEUINT16(p,b)    do {  UINT16 *p_tmp = (void *)p; const  UINT16 tv = ( UINT16)(b); memcpy(p, &tv, sizeof( UINT16)); p_tmp++; p = (void *)p_tmp; } while (0)
#define WRITEINT32(p,b)     do {   INT32 *p_tmp = (void *)p; const   INT32 tv = (  INT32)(b); memcpy(p, &tv, sizeof(  INT32)); p_tmp++; p = (void *)p_tmp; } while (0)
#define WRITEUINT32(p,b)    do {  UINT32 *p_tmp = (void *)p; const  UINT32 tv = ( UINT32)(b); memcpy(p, &tv, sizeof( UINT32)); p_tmp++; p = (void *)p_tmp; } while (0)
#define WRITECHAR(p,b)      do {    char *p_tmp = (void *)p; const    char tv = (   char)(b); memcpy(p, &tv, sizeof(   char)); p_tmp++; p = (void *)p_tmp; } while (0)
#define WRITEFIXED(p,b)     do { fixed_t *p_tmp = (void *)p; const fixed_t tv = (fixed_t)(b); memcpy(p, &tv, sizeof(fixed_t)); p_tmp++; p = (void *)p_tmp; } while (0)
#define WRITEANGLE(p,b)     do { angle_t *p_tmp = (void *)p; const angle_t tv = (angle_t)(b); memcpy(p, &tv, sizeof(angle_t)); p_tmp++; p = (void *)p_tmp; } while (0)
#else
#define WRITEUINT8(p,b)     do {   UINT8 *p_tmp = (  UINT8 *)p; *p_tmp = (  UINT8)(b); p_tmp++; p = (void *)p_tmp; } while (0)
#define WRITESINT8(p,b)     do {   SINT8 *p_tmp = (  SINT8 *)p; *p_tmp = (  SINT8)(b); p_tmp++; p = (void *)p_tmp; } while (0)
#define WRITEINT16(p,b)     do {   INT16 *p_tmp = (  INT16 *)p; *p_tmp = (  INT16)(b); p_tmp++; p = (void *)p_tmp; } while (0)
#define WRITEUINT16(p,b)    do {  UINT16 *p_tmp = ( UINT16 *)p; *p_tmp = ( UINT16)(b); p_tmp++; p = (void *)p_tmp; } while (0)
#define WRITEINT32(p,b)     do {   INT32 *p_tmp = (  INT32 *)p; *p_tmp = (  INT32)(b); p_tmp++; p = (void *)p_tmp; } while (0)
#define WRITEUINT32(p,b)    do {  UINT32 *p_tmp = ( UINT32 *)p; *p_tmp = ( UINT32)(b); p_tmp++; p = (void *)p_tmp; } while (0)
#define WRITECHAR(p,b)      do {    char *p_tmp = (   char *)p; *p_tmp = (   char)(b); p_tmp++; p = (void *)p_tmp; } while (0)
#define WRITEFIXED(p,b)     do { fixed_t *p_tmp = (fixed_t *)p; *p_tmp = (fixed_t)(b); p_tmp++; p = (void *)p_tmp; } while (0)
#define WRITEANGLE(p,b)     do { angle_t *p_tmp = (angle_t *)p; *p_tmp = (angle_t)(b); p_tmp++; p = (void *)p_tmp; } while (0)
#endif

// what is this?
#if defined (__GNUC__) && defined (DEALIGNED)
#define READUINT8(p)        ({   UINT8 *p_tmp = (void *)p;   UINT8 b; memcpy(&b, p, sizeof(  UINT8)); p_tmp++; p = (void *)p_tmp; b; })
#define READSINT8(p)        ({   SINT8 *p_tmp = (void *)p;   SINT8 b; memcpy(&b, p, sizeof(  SINT8)); p_tmp++; p = (void *)p_tmp; b; })
#define READINT16(p)        ({   INT16 *p_tmp = (void *)p;   INT16 b; memcpy(&b, p, sizeof(  INT16)); p_tmp++; p = (void *)p_tmp; b; })
#define READUINT16(p)       ({  UINT16 *p_tmp = (void *)p;  UINT16 b; memcpy(&b, p, sizeof( UINT16)); p_tmp++; p = (void *)p_tmp; b; })
#define READINT32(p)        ({   INT32 *p_tmp = (void *)p;   INT32 b; memcpy(&b, p, sizeof(  INT32)); p_tmp++; p = (void *)p_tmp; b; })
#define READUINT32(p)       ({  UINT32 *p_tmp = (void *)p;  UINT32 b; memcpy(&b, p, sizeof( UINT32)); p_tmp++; p = (void *)p_tmp; b; })
#define READCHAR(p)         ({    char *p_tmp = (void *)p;    char b; memcpy(&b, p, sizeof(   char)); p_tmp++; p = (void *)p_tmp; b; })
#define READFIXED(p)        ({ fixed_t *p_tmp = (void *)p; fixed_t b; memcpy(&b, p, sizeof(fixed_t)); p_tmp++; p = (void *)p_tmp; b; })
#define READANGLE(p)        ({ angle_t *p_tmp = (void *)p; angle_t b; memcpy(&b, p, sizeof(angle_t)); p_tmp++; p = (void *)p_tmp; b; })
#else
#define READUINT8(p)        ((UINT8*)(p = (void*)&((UINT8*)p)[1]))[-1]
#define READSINT8(p)        ((SINT8*)(p = (void*)&((SINT8*)p)[1]))[-1]
#define READINT16(p)        ((INT16*)(p = (void*)&((INT16*)p)[1]))[-1]
#define READUINT16(p)       ((UINT16*)(p = (void*)&((UINT16*)p)[1]))[-1]
#define READINT32(p)        ((INT32*)(p = (void*)&((INT32*)p)[1]))[-1]
#define READUINT32(p)       ((UINT32*)(p = (void*)&((UINT32*)p)[1]))[-1]
#define READCHAR(p)         ((char*)(p = (void*)&((char*)p)[1]))[-1]
#define READFIXED(p)        ((fixed_t*)(p = (void*)&((fixed_t*)p)[1]))[-1]
#define READANGLE(p)        ((angle_t*)(p = (void*)&((angle_t*)p)[1]))[-1]
#endif

#else //SRB2_BIG_ENDIAN
//
// definitions for big-endian machines with alignment constraints.
//
// Write a value to a little-endian, unaligned destination.
//
FUNCINLINE static ATTRINLINE void writeshort(void *ptr, INT32 val)
{
	SINT8 *cp = ptr;
	cp[0] = val; val >>= 8;
	cp[1] = val;
}

FUNCINLINE static ATTRINLINE void writelong(void *ptr, INT32 val)
{
	SINT8 *cp = ptr;
	cp[0] = val; val >>= 8;
	cp[1] = val; val >>= 8;
	cp[2] = val; val >>= 8;
	cp[3] = val;
}

#define WRITEUINT8(p,b)     do {  UINT8 *p_tmp = (  UINT8 *)p; *p_tmp       = (  UINT8)(b) ; p_tmp++; p = (void *)p_tmp;} while (0)
#define WRITESINT8(p,b)     do {  SINT8 *p_tmp = (  SINT8 *)p; *p_tmp       = (  SINT8)(b) ; p_tmp++; p = (void *)p_tmp;} while (0)
#define WRITEINT16(p,b)     do {  INT16 *p_tmp = (  INT16 *)p; writeshort (p, (  INT16)(b)); p_tmp++; p = (void *)p_tmp;} while (0)
#define WRITEUINT16(p,b)    do { UINT16 *p_tmp = ( UINT16 *)p; writeshort (p, ( UINT16)(b)); p_tmp++; p = (void *)p_tmp;} while (0)
#define WRITEINT32(p,b)     do {  INT32 *p_tmp = (  INT32 *)p; writelong  (p, (  INT32)(b)); p_tmp++; p = (void *)p_tmp;} while (0)
#define WRITEUINT32(p,b)    do { UINT32 *p_tmp = ( UINT32 *)p; writelong  (p, ( UINT32)(b)); p_tmp++; p = (void *)p_tmp;} while (0)
#define WRITECHAR(p,b)      do {   char *p_tmp = (   char *)p; *p_tmp       = (   char)(b) ; p_tmp++; p = (void *)p_tmp;} while (0)
#define WRITEFIXED(p,b)     do {fixed_t *p_tmp = (fixed_t *)p; writelong  (p, (fixed_t)(b)); p_tmp++; p = (void *)p_tmp;} while (0)
#define WRITEANGLE(p,b)     do {angle_t *p_tmp = (angle_t *)p; writelong  (p, (angle_t)(b)); p_tmp++; p = (void *)p_tmp;} while (0)

// Read a signed quantity from little-endian, unaligned data.
//
FUNCINLINE static ATTRINLINE INT16 readshort(void *ptr)
{
	SINT8 *cp  = ptr;
	UINT8 *ucp = ptr;
	return (cp[1] << 8) | ucp[0];
}

FUNCINLINE static ATTRINLINE UINT16 readushort(void *ptr)
{
	UINT8 *ucp = ptr;
	return (ucp[1] << 8) | ucp[0];
}

FUNCINLINE static ATTRINLINE INT32 readlong(void *ptr)
{
	SINT8 *cp = ptr;
	UINT8 *ucp = ptr;
	return (cp[3] << 24) | (ucp[2] << 16) | (ucp[1] << 8) | ucp[0];
}

FUNCINLINE static ATTRINLINE UINT32 readulong(void *ptr)
{
	UINT8 *ucp = ptr;
	return (ucp[3] << 24) | (ucp[2] << 16) | (ucp[1] << 8) | ucp[0];
}

#define READUINT8(p)        ((UINT8*)(p = (void*)&((UINT8*)p)[1]))[-1]
#define READSINT8(p)        ((SINT8*)(p = (void*)&((SINT8*)p)[1]))[-1]
#define READINT16(p)        readshort(&((INT16*)(p = (void*)&((INT16*)p)[1]))[-1])
#define READUINT16(p)       readushort(&((UINT16*)(p = (void*)&((UINT16*)p)[1]))[-1])
#define READINT32(p)        readlong(&((INT32*)(p = (void*)&((INT32*)p)[1]))[-1])
#define READUINT32(p)       readulong(&((UINT32*)(p = (void*)&((UINT32*)p)[1]))
#define READCHAR(p)         ((char*)(p = (void*)&((char*)p)[1]))[-1]
#define READFIXED(p)        readlong(&((fixed_t*)(p = (void*)&((fixed_t*)p)[1]))[-1])
#define READANGLE(p)        readulong(&((angle_t*)(p = (void*)&((angle_t*)p)[1]))[-1])
#endif //SRB2_BIG_ENDIAN

#undef DEALIGNED

#define WRITESTRINGN(p, s, n) do {                          \
	size_t tmp_i;                                           \
                                                            \
	for (tmp_i = 0; tmp_i < n && s[tmp_i] != '\0'; tmp_i++) \
		WRITECHAR(p, s[tmp_i]);                             \
                                                            \
	if (tmp_i < n)                                          \
		WRITECHAR(p, '\0');                                 \
} while (0)

#define WRITESTRINGL(p, s, n) do {                              \
	size_t tmp_i;                                               \
                                                                \
	for (tmp_i = 0; tmp_i < n - 1 && s[tmp_i] != '\0'; tmp_i++) \
		WRITECHAR(p, s[tmp_i]);                                 \
                                                                \
	WRITECHAR(p, '\0');                                         \
} while (0)

#define WRITESTRING(p, s) do {                 \
	size_t tmp_i;                              \
                                               \
	for (tmp_i = 0; s[tmp_i] != '\0'; tmp_i++) \
		WRITECHAR(p, s[tmp_i]);                \
                                               \
	WRITECHAR(p, '\0');                        \
} while (0)

#define WRITEMEM(p, s, n) do { \
	memcpy(p, s, n);           \
	p += n;                    \
} while (0)

#define SKIPSTRING(p) while (READCHAR(p) != '\0')

#define SKIPSTRINGN(p, n) do {               \
	size_t tmp_i = 0;                        \
                                             \
	while (tmp_i < n && READCHAR(p) != '\0') \
		tmp_i++;                             \
} while (0)

#define SKIPSTRINGL(p, n) SKIPSTRINGN(p, n)

#define READSTRINGN(p, s, n) do {                         \
	size_t tmp_i = 0;                                     \
                                                          \
	while (tmp_i < n && (s[tmp_i] = READCHAR(p)) != '\0') \
		tmp_i++;                                          \
                                                          \
	s[tmp_i] = '\0';                                      \
} while (0)

#define READSTRINGL(p, s, n) do {                             \
	size_t tmp_i = 0;                                         \
                                                              \
	while (tmp_i < n - 1 && (s[tmp_i] = READCHAR(p)) != '\0') \
		tmp_i++;                                              \
                                                              \
	s[tmp_i] = '\0';                                          \
} while (0)

#define READSTRING(p, s) do {                \
	size_t tmp_i = 0;                        \
                                             \
	while ((s[tmp_i] = READCHAR(p)) != '\0') \
		tmp_i++;                             \
                                             \
	s[tmp_i] = '\0';                         \
} while (0)

#define READMEM(p, s, n) do { \
	memcpy(s, p, n);          \
	p += n;                   \
} while (0)
