/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// quakedef.h -- primary header for client

#ifndef __QUAKEDEF_H
#define __QUAKEDEF_H

#define QUAKE_GAME                      // as opposed to utilities

#define VERSION			1.09
#define GLQUAKE_VERSION	1.00
#define LINUX_VERSION	1.30

//define	PARANOID // speed sapping error checking

#ifdef _WIN32
#pragma warning( disable : 4244 4127 4201 4214 4514 4305 4115 4018)
#endif

#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

#ifdef _MSC_VER
#include <assert.h> //johnfitz
#endif

#if defined(_WIN32) && !defined(WINDED)

#if defined(_M_IX86)
#define __i386__      1
#endif

void    VID_LockBuffer (void);
void    VID_UnlockBuffer (void);

#else

#define VID_LockBuffer()
#define VID_UnlockBuffer()

#endif

#if defined __i386__ // && !defined __sun__
#define id386	1
#else
#define id386   0
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1900
#undef id386
#define id386 0
#endif

#if id386
//broken!!!
#define UNALIGNED_OK    1       // set to 0 if unaligned accesses are not supported
#else
#define UNALIGNED_OK    0
#endif

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
#define CACHE_SIZE      32              // used to align key data structures

#define UNUSED(x)       (x = x) // for pesky compiler / lint warnings

#define MINIMUM_MEMORY				0x550000
#define MINIMUM_MEMORY_LEVELPAK		(MINIMUM_MEMORY + 0x100000)

#define MAX_NUM_ARGVS   50

// up / down
#define PITCH   0

// left / right
#define YAW             1

// fall over
#define ROLL    2


#define	MAX_QPATH		64			// max length of a quake game pathname
#define	MAX_OSPATH		256			/* FS: Was 128 */ // max length of a filesystem pathname

#define ON_EPSILON              0.1                     // point on plane side epsilon

#define	MAX_MSGLEN			64000	// max length of a reliable message //johnfitz -- was 8000
#define MAX_MSGLEN_OLD		8000
/* FS: FIXME THIS BREAKS IPX */
#define MAX_DATAGRAM            32000 // max length of unreliable message //johnfitz -- was 1024
#define MAX_DATAGRAM_OLD		1024

#define DATAGRAM_MTU		1400	// johnfitz -- actual limit for unreliable messages to nonlocal clients

//
// per-level limits
//

#define MIN_EDICTS		256			// johnfitz -- lowest allowed value for max_edicts cvar
#define MAX_EDICTS		32000		// johnfitz -- highest allowed value for max_edicts cvar
									// ents past 8192 can't play sounds in the standard protocol
#define MAX_LIGHTSTYLES	64
#define	MAX_MODELS		2048		// johnfitz -- was 256
#define	MAX_SOUNDS		2048		// johnfitz -- was 256

#define SAVEGAME_COMMENT_LENGTH 39

#define MAX_STYLESTRING 64

#define CL_MASTER_ADDR	"maraakate.org" /* FS: Gamespy dead */
#define CL_MASTER_PORT "28900"
#define SV_MASTER_ADDR "maraakate.org" /* FS: Gamespy dead */
#define SV_MASTER_PORT "27900"

//
// stats are integers communicated to the client by the server
//
#define MAX_CL_STATS            32
#define STAT_HEALTH                     0
#define STAT_FRAGS                      1
#define STAT_WEAPON                     2
#define STAT_AMMO                       3
#define STAT_ARMOR                      4
#define STAT_WEAPONFRAME        5
#define STAT_SHELLS                     6
#define STAT_NAILS                      7
#define STAT_ROCKETS            8
#define STAT_CELLS                      9
#define STAT_ACTIVEWEAPON       10
#define STAT_TOTALSECRETS       11
#define STAT_TOTALMONSTERS      12
#define STAT_SECRETS            13              // bumped on client side by svc_foundsecret
#define STAT_MONSTERS           14              // bumped by svc_killedmonster

// stock defines

#define IT_SHOTGUN                              1
#define IT_SUPER_SHOTGUN                2
#define IT_NAILGUN                              4
#define IT_SUPER_NAILGUN                8
#define IT_GRENADE_LAUNCHER             16
#define IT_ROCKET_LAUNCHER              32
#define IT_LIGHTNING                    64
#define IT_SUPER_LIGHTNING      128
#define IT_SHELLS               256
#define IT_NAILS                512
#define IT_ROCKETS              1024
#define IT_CELLS                2048
#define IT_AXE                  4096
#define IT_ARMOR1               8192
#define IT_ARMOR2               16384
#define IT_ARMOR3               32768
#define IT_SUPERHEALTH          65536
#define IT_KEY1                 131072
#define IT_KEY2                 262144
#define IT_INVISIBILITY                 524288
#define IT_INVULNERABILITY              1048576
#define IT_SUIT                                 2097152
#define IT_QUAD                                 4194304
#define IT_SIGIL1               (1<<28)
#define IT_SIGIL2               (1<<29)
#define IT_SIGIL3               (1<<30)
#define IT_SIGIL4               (1<<31)

//===========================================
//rogue changed and added defines

#define RIT_SHELLS              128
#define RIT_NAILS               256
#define RIT_ROCKETS             512
#define RIT_CELLS               1024
#define RIT_AXE                 2048
#define RIT_LAVA_NAILGUN        4096
#define RIT_LAVA_SUPER_NAILGUN  8192
#define RIT_MULTI_GRENADE       16384
#define RIT_MULTI_ROCKET        32768
#define RIT_PLASMA_GUN          65536
#define RIT_ARMOR1              8388608
#define RIT_ARMOR2              16777216
#define RIT_ARMOR3              33554432
#define RIT_LAVA_NAILS          67108864
#define RIT_PLASMA_AMMO         134217728
#define RIT_MULTI_ROCKETS       268435456
#define RIT_SHIELD              536870912
#define RIT_ANTIGRAV            1073741824
#define RIT_SUPERHEALTH         2147483648

//MED 01/04/97 added hipnotic defines
//===========================================
//hipnotic added defines
#define HIT_PROXIMITY_GUN_BIT 16
#define HIT_MJOLNIR_BIT       7
#define HIT_LASER_CANNON_BIT  23
#define HIT_PROXIMITY_GUN   (1<<HIT_PROXIMITY_GUN_BIT)
#define HIT_MJOLNIR         (1<<HIT_MJOLNIR_BIT)
#define HIT_LASER_CANNON    (1<<HIT_LASER_CANNON_BIT)
#define HIT_WETSUIT         (1<<(23+2))
#define HIT_EMPATHY_SHIELDS (1<<(23+3))

//===========================================

#define MAX_SCOREBOARD          16
#define MAX_SCOREBOARDNAME      32

#define SOUND_CHANNELS          8

#include "common.h"
#include "bspfile.h"
#include "vid.h"
#include "sys.h"
#include "zone.h"
#include "mathlib.h"

typedef struct
{
	vec3_t			origin;
	vec3_t			angles;
	unsigned short 	modelindex; //johnfitz -- was int
	unsigned short 	frame; //johnfitz -- was int
	unsigned char 	colormap; //johnfitz -- was int
	unsigned char 	skin; //johnfitz -- was int
	unsigned char	alpha; //johnfitz -- added
	int				effects;
} entity_state_t;


#include "wad.h"
#include "draw.h"
#include "cvar.h"
#include "screen.h"
#include "net.h"
#include "protocol.h"
#include "cmd.h"
#include "sbar.h"
#include "sound.h"
#include "render.h"
#include "client.h"
#include "progs.h"
#include "server.h"

#ifdef GLQUAKE
#include "gl_model.h"
#else
#include "model.h"
#include "d_iface.h"
#endif

#include "input.h"
#include "world.h"
#include "keys.h"
#include "console.h"
#include "view.h"
#include "menu.h"
#include "crc.h"
#include "cdaudio.h"

#ifdef GLQUAKE
#include "glquake.h"
#endif

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifdef _WIN32
#define QINLINE __inline
#else
#define QINLINE inline
#endif

/* FS: Added */
#define bound(a,b,c) ((a) >= (c) ? (a) : \
					(b) < (a) ? (a) : (b) > (c) ? (c) : (b))

//=============================================================================

// the host system specifies the base of the directory tree, the
// command line parms passed to the program, and the amount of memory
// available for the program to use

typedef struct
{
	char	*basedir;
	int		argc;
	char	**argv;
	void	*membase;
	size_t	memsize;
} quakeparms_t;


//=============================================================================



extern qboolean noclip_anglehack;


//
// host
//
extern  quakeparms_t host_parms;

extern	cvar_t	*sys_ticrate;
extern	cvar_t	*sys_nostdout;
extern	cvar_t	*developer;
extern	cvar_t	*timestamp; /* FS: Added */
extern	cvar_t	*con_show_description; /* FS: Added */
extern	cvar_t	*con_show_dev_flags; /* FS: Added */
extern	cvar_t	*max_edicts; //johnfitz
extern	cvar_t	*dedicated; /* FS: Added */

extern  qboolean        host_initialized;               // true if into command execution
extern	qboolean		quakerc_init; /* FS: Intercept config.cfg from quake.rc */
extern  double          host_frametime;
extern  byte            *host_basepal;
extern  byte            *host_colormap;
extern  int                     host_framecount;        // incremented every frame, never reset
extern  double          realtime;                       // not bounded in any way, changed at
										// start of every frame, never reset

void Host_ClearMemory (void);
void Host_ServerFrame (void);
void Host_InitCommands (void);
void Host_Init (quakeparms_t *parms);
void Host_Shutdown(void);
void Host_Error (const char *error, ...);
void Host_EndGame (const char *message, ...);
void Host_Frame (float time);
void Host_Quit_f (void);
void Host_ClientCommands (const char *fmt, ...);
void Host_ShutdownServer (qboolean crash);
void Host_WriteConfiguration (const char *cfgName); /* FS: Added */

extern qboolean	msg_suppress_1;         // suppresses resolution and cache size console output
										//  an fullscreen DIB focus gain/loss
extern int	current_skill;          // skill level for currently loaded level (in case
										//  the user changes the cvar while the level is
										//  running, this reflects the level actually in use)

extern qboolean	sDedicated;

extern size_t	minimum_memory;

//
// chase
//
extern  cvar_t  *chase_active;

void Chase_Init (void);
void Chase_Reset (void);
void Chase_Update (void);

/* FS: From Q2 */
#define SFF_ARCH    0x01
#define SFF_HIDDEN  0x02
#define SFF_RDONLY  0x04
#define SFF_SUBDIR  0x08
#define SFF_SYSTEM  0x10

char *Sys_FindFirst (char *path, unsigned musthave, unsigned canthave);
char *Sys_FindNext (unsigned musthave, unsigned canthave);
void Sys_FindClose (void);

/* FS: FIXME: Move. */
#define	TAG_GAME	765		// clear when unloading the dll
#define	TAG_LEVEL	766		// clear when loading a new level
#define TAG_TEMP	767

#define		MAXPRINTMSG	8192	// Knightmare 3/12/15- was 4096

/* FS */
extern int	max_datagram;
extern int	net_datagramsize;
extern int	max_msglen;

#endif // __QUAKEDEF_H
