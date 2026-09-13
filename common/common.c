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
// common.c -- misc functions used in client and server

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef __DJGPP__
#include <limits.h>
#include <errno.h>
#include <libc/file.h>
#endif

#ifdef SERVERONLY
#include "qwsvdef.h"
#else
#include "quakedef.h"
#endif

#define MAX_NUM_ARGVS	50
#define NUM_SAFE_ARGVS  8

usercmd_t nullcmd; // guarenteed to be zero

static char	*largv[MAX_NUM_ARGVS + NUM_SAFE_ARGVS + 1];
static char	*argvdummy = " ";

static char	*safeargvs[NUM_SAFE_ARGVS] = {"-stdvid", "-nolan", "-nosound", "-nocdaudio", "-nojoy", "-nomouse", "-dibonly", "-safevga"}; /* FS: Added -safevga for 320x200 */

cvar_t	*registered;

/* sending cmdline upon CCREQ_RULE_INFO is evil */
cvar_t	*cmdline;

/* FS: For Nehahra */
cvar_t	*cutscene;
cvar_t	*nehx00;
cvar_t	*nehx01;
cvar_t	*nehx02;
cvar_t	*nehx03;
cvar_t	*nehx04;
cvar_t	*nehx05;
cvar_t	*nehx06;
cvar_t	*nehx07;
cvar_t	*nehx08;
cvar_t	*nehx09;
cvar_t	*nehx10;
cvar_t	*nehx11;
cvar_t	*nehx12;
cvar_t	*nehx13;
cvar_t	*nehx14;
cvar_t	*nehx15;
cvar_t	*nehx16;
cvar_t	*nehx17;
cvar_t	*nehx18;
cvar_t	*nehx19;

cvar_t	*sv_allow_errorcmd; /* FS */

int com_nummissionpacks; //johnfitz

qboolean        com_modified;   // set true if using non-id files

int             static_registered = 1;  // only for startup check, then set

qboolean		msg_suppress_1 = 0;

void COM_InitFilesystem (void);
void COM_Path_f (void);
void COM_Dir_f (void); /* FS: From Quake 2 */
void COM_Error_f (void); /* FS: From Quake 2 */

// if a packfile directory differs from this, it is assumed to be hacked
#define PAK0_COUNT		339	/* id1/pak0.pak - v1.0x */
#define PAK0_CRC_V100		13900	/* id1/pak0.pak - v1.00 */
#define PAK0_CRC_V101		62751	/* id1/pak0.pak - v1.01 */
#define PAK0_CRC_V106		32981	/* id1/pak0.pak - v1.06 */
#ifdef QUAKE1
#define PAK0_CRC	(PAK0_CRC_V106)
#else
#define	PAK0_CRC		52883
#endif
#define PAK0_COUNT_V091		308	/* id1/pak0.pak - v0.91/0.92, not supported */
#define PAK0_CRC_V091		28804	/* id1/pak0.pak - v0.91/0.92, not supported */

char	com_token[1024];
int		com_argc;
char	**com_argv;

#define CMDLINE_LENGTH	256 //johnfitz -- mirrored in cmd.c
char	com_cmdline[CMDLINE_LENGTH];

qboolean	standard_quake = true, rogue, hipnotic;
qboolean	nehahra, extended_mod, warpspasm; /* FS: For Nehahra and Warpspasm */

char	gamedirfile[MAX_OSPATH];

// this graphic needs to be in the pak file to use registered features
static const unsigned short pop[] =
{
 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
,0x0000,0x0000,0x6600,0x0000,0x0000,0x0000,0x6600,0x0000
,0x0000,0x0066,0x0000,0x0000,0x0000,0x0000,0x0067,0x0000
,0x0000,0x6665,0x0000,0x0000,0x0000,0x0000,0x0065,0x6600
,0x0063,0x6561,0x0000,0x0000,0x0000,0x0000,0x0061,0x6563
,0x0064,0x6561,0x0000,0x0000,0x0000,0x0000,0x0061,0x6564
,0x0064,0x6564,0x0000,0x6469,0x6969,0x6400,0x0064,0x6564
,0x0063,0x6568,0x6200,0x0064,0x6864,0x0000,0x6268,0x6563
,0x0000,0x6567,0x6963,0x0064,0x6764,0x0063,0x6967,0x6500
,0x0000,0x6266,0x6769,0x6a68,0x6768,0x6a69,0x6766,0x6200
,0x0000,0x0062,0x6566,0x6666,0x6666,0x6666,0x6562,0x0000
,0x0000,0x0000,0x0062,0x6364,0x6664,0x6362,0x0000,0x0000
,0x0000,0x0000,0x0000,0x0062,0x6662,0x0000,0x0000,0x0000
,0x0000,0x0000,0x0000,0x0061,0x6661,0x0000,0x0000,0x0000
,0x0000,0x0000,0x0000,0x0000,0x6500,0x0000,0x0000,0x0000
,0x0000,0x0000,0x0000,0x0000,0x6400,0x0000,0x0000,0x0000
};

/*

All of Quake's data access is through a hierchal file system, but the contents of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all game directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.  This can be overridden with the "-basedir" command line parm to allow code debugging
in a different directory.  The base directory is only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be saved to.  This can be overridden with the "-game" command line parameter.  The game directory can
never be changed while quake is executing.  This is a precacution against having a malicious server instruct clients to write files over areas they shouldn't.

The "cache directory" is only used during development to save network bandwidth, especially over ISDN / T1 lines.  If there is a cache directory
specified, when a file is found by the normal search path, it will be mirrored
into the cache directory, then opened there.

*/

//============================================================================


// ClearLink is used for new headnodes
void ClearLink (link_t *l)
{
	l->prev = l->next = l;
}

void RemoveLink (link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void InsertLinkBefore (link_t *l, link_t *before)
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}
void InsertLinkAfter (link_t *l, link_t *after)
{
	l->next = after->next;
	l->prev = after;
	l->prev->next = l;
	l->next->prev = l;
}

/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/
size_t Q_strlen (const char *str)
{
	return strlen(str);
}

int Q_strcmp (const char *s1, const char *s2)
{
	return strcmp(s1, s2);
}

int Q_strncmp (const char *s1, const char *s2, size_t count)
{
	return strncmp(s1, s2, count);
}

int Q_strncasecmp (const char *s1, const char *s2, size_t n)
{
#ifdef _WIN32
	return _strnicmp(s1, s2, n);
#else
	return strncasecmp(s1, s2, n);
#endif
}

int Q_strcasecmp (const char *s1, const char *s2)
{
	return Q_strncasecmp (s1, s2, 99999);
}

/* FS: From OpenBSD */
size_t Q_strlcpy (char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0) {
		while (--n != 0) {
			if ((*d++ = *s++) == '\0')
				break;
		}
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}

/* FS: From OpenBSD */
size_t Q_strlcat (char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (n-- != 0 && *d != '\0')
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return(dlen + strlen(s));
	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return(dlen + (s - src));	/* count does not include NUL */
}

int Q_toupper (int c) /* FS: Added */
{
	if (c>='a' && c<='z')
		c-=('a'-'A');
	return(c);
}

int Q_tolower (int c) /* FS: Added */
{
	if (c>='A' && c<='Z')
		c+=('a'-'A');
	return(c);
}

qboolean Q_StrIsNullOrEmpty (const char *str) /* FS */
{
	size_t len, i, badchars;

	if (!str || str[0] == '\0')
		return true;

	len = Q_strlen(str);
	if (!len)
		return true;

	for (i = 0, badchars = 0; i <= len; i++)
	{
		if (str[i] == ' ')
			badchars++;
	}

	if (badchars == len)
		return true;

	return false;
}

/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/

qboolean	bigendien;

short	(*BigShort) (short l);
short	(*LittleShort) (short l);
int		(*BigLong) (int l);
int		(*LittleLong) (int l);
float	(*BigFloat) (float l);
float	(*LittleFloat) (float l);

short   ShortSwap (short l)
{
	byte    b1,b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}

short	ShortNoSwap (short l)
{
	return l;
}

int    LongSwap (int l)
{
	byte    b1,b2,b3,b4;

	b1 = l&255;
	b2 = (l>>8)&255;
	b3 = (l>>16)&255;
	b4 = (l>>24)&255;

	return ((int)b1<<24) + ((int)b2<<16) + ((int)b3<<8) + b4;
}

int	LongNoSwap (int l)
{
	return l;
}

float FloatSwap (float f)
{
	union
	{
		float	f;
		byte	b[4];
	} dat1, dat2;
	
	
	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

float FloatNoSwap (float f)
{
	return f;
}

/*
==============================================================================

			MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
*/

//
// writing functions
//

#ifdef FTE_PEXT_FLOATCOORDS

int msg_coordsize = 2; // 2 or 4.
int msg_anglesize = 1; // 1 or 2.

float MSG_FromCoord(coorddata c, int bytes)
{
	switch(bytes)
	{
	case 2:	//encode 1/8th precision, giving -4096 to 4096 map sizes
		return LittleShort(c.b2)/8.0f;
	case 4:
		return LittleFloat(c.f);
	default:
		Host_Error("MSG_FromCoord: not a sane size");
		return 0;
	}
}

coorddata MSG_ToCoord(float f, int bytes)	//return value should be treated as (char*)&ret;
{
	coorddata r;
	switch(bytes)
	{
	case 2:
		r.b4 = 0;
		if (f >= 0)
			r.b2 = LittleShort((short)(f*8+0.5f));
		else
			r.b2 = LittleShort((short)(f*8-0.5f));
		break;
	case 4:
		r.f = LittleFloat(f);
		break;
	default:
		Host_Error("MSG_ToCoord: not a sane size");
		r.b4 = 0;
	}

	return r;
}

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable:4761) /* FS: Disable the conversion warning since it's on purpose */
#endif
coorddata MSG_ToAngle(float f, int bytes)	//return value is NOT byteswapped.
{
	coorddata r;
	switch(bytes)
	{
	case 1:
		r.b4 = 0;
		if (f >= 0)
			r.b[0] = (int)(f*(256.0f/360.0f) + 0.5f) & 255;
		else
			r.b[0] = (int)(f*(256.0f/360.0f) - 0.5f) & 255;
		break;
	case 2:
		r.b4 = 0;
		if (f >= 0)
			r.b2 = LittleShort((int)(f*(65536.0f/360.0f) + 0.5f) & 65535);
		else
			r.b2 = LittleShort((int)(f*(65536.0f/360.0f) - 0.5f) & 65535);
		break;
//	case 4:
//		r.f = LittleFloat(f);
//		break;
	default:
		Host_Error("MSG_ToAngle: not a sane size");
		r.b4 = 0;
	}

	return r;
#ifdef _WIN32
#pragma warning(pop)
#endif
}

#endif

void MSG_WriteChar (sizebuf_t *sb, int c)
{
	byte *buf;

#ifdef PARANOID
	if (c < -128 || c > 127)
	{
		Sys_Error ("MSG_WriteChar: range error");
		return;
	}
#endif

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteByte (sizebuf_t *sb, int c)
{
	byte *buf;

#ifdef PARANOID
	if (c < 0 || c > 255)
	{
		Sys_Error ("MSG_WriteByte: range error");
		return;
	}
#endif

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteShort (sizebuf_t *sb, int c)
{
	byte *buf;

#ifdef PARANOID
	if (c < ((short)0x8000) || c > (short)0x7fff)
	{
		Sys_Error ("MSG_WriteShort: range error");
		return;
	}
#endif

	buf = SZ_GetSpace (sb, 2);
	buf[0] = c&0xff;
	buf[1] = c>>8;
}

void MSG_WriteLong (sizebuf_t *sb, int c)
{
	byte	*buf;
	
	buf = SZ_GetSpace (sb, 4);
	buf[0] = c&0xff;
	buf[1] = (c>>8)&0xff;
	buf[2] = (c>>16)&0xff;
	buf[3] = c>>24;
}

void MSG_WriteFloat (sizebuf_t *sb, float f)
{
	union
	{
		float	f;
		int		l;
	} dat;
	
	
	dat.f = f;
	dat.l = LittleLong (dat.l);
	
	SZ_Write (sb, &dat.l, 4);
}

void MSG_WriteString (sizebuf_t *sb, char *s)
{
	if (!s)
		SZ_Write (sb, "", 1);
	else
		SZ_Write (sb, s, Q_strlen(s)+1);
}

//johnfitz -- original behavior, 13.3 fixed point coords, max range +-4096
void MSG_WriteCoord16 (sizebuf_t *sb, float f)
{
	MSG_WriteShort (sb, Q_rint(f*8));
}

//johnfitz -- 16.8 fixed point coords, max range +-32768
void MSG_WriteCoord24 (sizebuf_t *sb, float f)
{
	MSG_WriteShort (sb, f);
	MSG_WriteByte (sb, (int)(f*255)%255);
}

//johnfitz -- 32-bit float coords
void MSG_WriteCoord32f (sizebuf_t *sb, float f)
{
	MSG_WriteFloat (sb, f);
}

void MSG_WriteCoord (sizebuf_t *sb, float f)
{
	MSG_WriteCoord16 (sb, f);
}

void MSG_WriteAngle (sizebuf_t *sb, float f)
{
	MSG_WriteByte (sb, Q_rint(f * 256.0 / 360.0) & 255); //johnfitz -- use Q_rint instead of (int)
}

//johnfitz -- for PROTOCOL_FITZQUAKE and QW
void MSG_WriteAngle16 (sizebuf_t *sb, float f)
{
	MSG_WriteShort (sb, Q_rint(f * 65536.0 / 360.0) & 65535);
}
//johnfitz

void MSG_WriteDeltaUsercmd (sizebuf_t *buf, usercmd_t *from, usercmd_t *cmd)
{
	int		bits;

//
// send the movement message
//
	bits = 0;
	if (cmd->angles[0] != from->angles[0])
		bits |= CM_ANGLE1;
	if (cmd->angles[1] != from->angles[1])
		bits |= CM_ANGLE2;
	if (cmd->angles[2] != from->angles[2])
		bits |= CM_ANGLE3;
	if (cmd->forwardmove != from->forwardmove)
		bits |= CM_FORWARD;
	if (cmd->sidemove != from->sidemove)
		bits |= CM_SIDE;
	if (cmd->upmove != from->upmove)
		bits |= CM_UP;
	if (cmd->buttons != from->buttons)
		bits |= CM_BUTTONS;
	if (cmd->impulse != from->impulse)
		bits |= CM_IMPULSE;

    MSG_WriteByte (buf, bits);

	if (bits & CM_ANGLE1)
		MSG_WriteAngle16 (buf, cmd->angles[0]);
	if (bits & CM_ANGLE2)
		MSG_WriteAngle16 (buf, cmd->angles[1]);
	if (bits & CM_ANGLE3)
		MSG_WriteAngle16 (buf, cmd->angles[2]);
	
	if (bits & CM_FORWARD)
		MSG_WriteShort (buf, cmd->forwardmove);
	if (bits & CM_SIDE)
	  	MSG_WriteShort (buf, cmd->sidemove);
	if (bits & CM_UP)
		MSG_WriteShort (buf, cmd->upmove);

 	if (bits & CM_BUTTONS)
	  	MSG_WriteByte (buf, cmd->buttons);
 	if (bits & CM_IMPULSE)
	    MSG_WriteByte (buf, cmd->impulse);
	MSG_WriteByte (buf, cmd->msec);
}

//
// reading functions
//
int			msg_readcount;
qboolean	msg_badread;

void MSG_BeginReading (void)
{
	msg_readcount = 0;
	msg_badread = false;
}

int MSG_GetReadCount(void)
{
	return msg_readcount;
}

// returns -1 and sets msg_badread if no more characters are available
int MSG_ReadChar (void)
{
	int	c;
	
	if (msg_readcount+1 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}
		
	c = (signed char)net_message.data[msg_readcount];
	msg_readcount++;
	
	return c;
}

int MSG_ReadByte (void)
{
	int	c;
	
	if (msg_readcount+1 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}
		
	c = (unsigned char)net_message.data[msg_readcount];
	msg_readcount++;
	
	return c;
}

int MSG_ReadShort (void)
{
	int	c;
	
	if (msg_readcount+2 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}
		
	c = (short)(net_message.data[msg_readcount]
	+ (net_message.data[msg_readcount+1]<<8));
	
	msg_readcount += 2;
	
	return c;
}

int MSG_ReadLong (void)
{
	int	c;
	
	if (msg_readcount+4 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}
		
	c = net_message.data[msg_readcount]
	+ (net_message.data[msg_readcount+1]<<8)
	+ (net_message.data[msg_readcount+2]<<16)
	+ (net_message.data[msg_readcount+3]<<24);
	
	msg_readcount += 4;
	
	return c;
}

float MSG_ReadFloat (void)
{
	union
	{
		byte	b[4];
		float	f;
		int		l;
	} dat;
	
	dat.b[0] = net_message.data[msg_readcount];
	dat.b[1] = net_message.data[msg_readcount+1];
	dat.b[2] = net_message.data[msg_readcount+2];
	dat.b[3] = net_message.data[msg_readcount+3];
	msg_readcount += 4;
	
	dat.l = LittleLong (dat.l);

	return dat.f;
}

char *MSG_ReadString (void)
{
	static char     string[2048];
	size_t	l;
	int		c;
	
	l = 0;
	do
	{
		c = MSG_ReadByte ();
		if (c == -1 || c == 0)
			break;
		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);
	
	string[l] = 0;
	
	return string;
}

char *MSG_ReadStringLine (void)
{
	static char	string[2048];
	size_t	l;
	int		c;
	
	l = 0;
	do
	{
		c = MSG_ReadByte ();
		if (c == -1 || c == 0 || c == '\n')
			break;
		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);
	
	string[l] = 0;
	
	return string;
}

//johnfitz -- original behavior, 13.3 fixed point coords, max range +-4096
float MSG_ReadCoord16 (void)
{
	return MSG_ReadShort() * (1.0/8);
}

//johnfitz -- 16.8 fixed point coords, max range +-32768
float MSG_ReadCoord24 (void)
{
	return MSG_ReadShort() + MSG_ReadByte() * (1.0/255);
}

//johnfitz -- 32-bit float coords
float MSG_ReadCoord32f (void)
{
	return MSG_ReadFloat();
}

float MSG_ReadCoord (void)
{
	return MSG_ReadCoord16();
}

float MSG_ReadAngle (void)
{
	return MSG_ReadChar() * (360.0/256);
}

//johnfitz -- for PROTOCOL_FITZQUAKE and QW
float MSG_ReadAngle16 (void)
{
	return MSG_ReadShort() * (360.0/65536);
}
//johnfitz

void MSG_ReadDeltaUsercmd (usercmd_t *from, usercmd_t *move)
{
	int bits;

	memcpy (move, from, sizeof(*move));

	bits = MSG_ReadByte ();
		
// read current angles
	if (bits & CM_ANGLE1)
		move->angles[0] = MSG_ReadAngle16 ();
	if (bits & CM_ANGLE2)
		move->angles[1] = MSG_ReadAngle16 ();
	if (bits & CM_ANGLE3)
		move->angles[2] = MSG_ReadAngle16 ();
		
// read movement
	if (bits & CM_FORWARD)
		move->forwardmove = MSG_ReadShort ();
	if (bits & CM_SIDE)
		move->sidemove = MSG_ReadShort ();
	if (bits & CM_UP)
		move->upmove = MSG_ReadShort ();
	
// read buttons
	if (bits & CM_BUTTONS)
		move->buttons = MSG_ReadByte ();

	if (bits & CM_IMPULSE)
		move->impulse = MSG_ReadByte ();

// read time to run command
	move->msec = MSG_ReadByte ();
}

void MSG_ReadData (void *data, int len) /* FS */
{
	int	i;

	for (i = 0 ; i < len ; i++)
		((byte *)data)[i] = MSG_ReadByte ();
}

//===========================================================================

void SZ_InitEx (sizebuf_t *buf, byte *data, int length, qbool allowoverflow) /* FS: From EZQ */
{
	memset (buf, 0, sizeof (*buf));
	buf->data = data;
	buf->maxsize = length;
	buf->allowoverflow = allowoverflow;
}

void SZ_Init (sizebuf_t *buf, byte *data, int length) /* FS: From EZQ */
{
	SZ_InitEx (buf, data, length, false);
}

void SZ_Alloc (sizebuf_t *buf, int startsize)
{
	if (startsize < 256)
		startsize = 256;
	buf->data = Hunk_AllocName (startsize, "sizebuf");
	buf->maxsize = startsize;
	buf->cursize = 0;
}

void SZ_Clear (sizebuf_t *buf)
{
	buf->cursize = 0;
	buf->overflowed = false;
}

void *SZ_GetSpace (sizebuf_t *buf, int length)
{
	void	*data;
	
	if (buf->cursize + length > buf->maxsize)
	{
		if (!buf->allowoverflow)
		{
			Sys_Error ("SZ_GetSpace: overflow without allowoverflow set (%d)", buf->maxsize);
			return NULL;
		}

		if (length > buf->maxsize)
		{
			Sys_Error ("SZ_GetSpace: %i is > full buffer size", length);
			return NULL;
		}
			
		Sys_Printf ("SZ_GetSpace: overflow\n");	// because Com_Printf may be redirected
		SZ_Clear (buf); 
		buf->overflowed = true;
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;
	
	return data;
}

void SZ_Write (sizebuf_t *buf, void *data, int length)
{
	memcpy (SZ_GetSpace(buf,length),data,length);
}

void SZ_Print (sizebuf_t *buf, char *data)
{
	int	len;
	
	len = Q_strlen(data)+1;

	if (!buf->cursize || buf->data[buf->cursize-1])
		memcpy ((byte *)SZ_GetSpace(buf, len),data,len); // no trailing 0
	else
		memcpy ((byte *)SZ_GetSpace(buf, len-1)-1,data,len); // write over trailing 0
}


//============================================================================


/*
============
COM_SkipPath
============
*/
char *COM_SkipPath (char *pathname)
{
	char	*last;
	
	last = pathname;
	while (*pathname)
	{
		if (*pathname=='/')
			last = pathname+1;
		pathname++;
	}
	return last;
}

/*
============
COM_StripExtension
============
*/
void COM_StripExtension (char *in, char *out)
{
	while (*in && *in != '.')
		*out++ = *in++;
	*out = 0;
}

/*
============
COM_FileExtension
============
*/
const char *COM_FileExtension (const char *in)
{
	static char exten[8];
	int		i;
	const char *s;

	/* FS: Modified this to walk backwards instead, because DJGPP and probably Linux start at ./ which will mess this up. */
	s = in + strlen(in) - 1;

	while (s != in && *s != '.')
		s--;
	if (!*in)
		return "";
	s++;
	for (i=0 ; i<7 && *s ; i++,s++)
		exten[i] = *s;
	exten[i] = 0;
	return exten;
}

/*
============
COM_FilePath

Returns the path up to, but not including the last /
============
*/
void COM_FilePath (char *in, char *out)
{
	char *s;

	if (!*in) {
		*out = 0;
		return;
	}
	s = in + strlen(in) - 1;

	while (s != in && *s != '/')
		s--;

	strncpy (out,in, s-in);
	out[s-in] = 0;
}


/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension (char *path, const char *extension, size_t pathlen)
{
	char    *src;
//
// if path doesn't have a .EXT, append extension
// (extension should include the .)
//
	src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	Q_strlcat (path, extension, pathlen);
}

//============================================================================


/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse (char *data)
{
	int	c;
	int	len;
	
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;	// end of file
		data++;
	}
	
// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	
// skip /*..*/ comments
	if (c == '/' && data[1] == '*')
	{
		data += 2;
		while (*data && !(*data == '*' && data[1] == '/'))
			data++;
		if (*data)
			data += 2;
		goto skipwhite;
	}

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			if ((c = *data) != 0)
				++data;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

#ifdef QUAKE1
// parse single characters
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c==':')
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}
#endif // QUAKE1

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
#ifdef QUAKE1
		if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c==':')
			break;
#endif // QUAKE1
	} while (c>32);
	
	com_token[len] = 0;
	return data;
}


/*
================
COM_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int COM_CheckParm (char *parm)
{
	int	i;
	
	for (i=1 ; i<com_argc ; i++)
	{
		if (!com_argv[i])
			continue; // NEXTSTEP sometimes clears appkit vars.
		if (!Q_strcmp (parm,com_argv[i]))
			return i;
	}
		
	return 0;
}

/* FS: Quake 2 stuff */
int COM_Argc (void)
{
	return com_argc;
}

char *COM_Argv (int arg)
{
	if (arg < 0 || arg >= com_argc || !com_argv[arg])
		return "";
	return com_argv[arg];
}

void COM_ClearArgv (int arg)
{
	if (arg < 0 || arg >= com_argc || !com_argv[arg])
		return;
	com_argv[arg] = "";
}

/*
================
COM_CheckRegistered

Looks for the pop.txt file and verifies it.
Sets the "registered" cvar.
Immediately exits out if an alternate game was attempted to be started without
being registered.
================
*/
void COM_CheckRegistered (void)
{
	FILE		*h;
	unsigned short	check[128];
	int			i;

	COM_FOpenFile("gfx/pop.lmp", &h);
	static_registered = 0;

	if (!h)
	{
		Com_Printf ("Playing shareware version.\n");
#ifndef SERVERONLY
// FIXME DEBUG -- only temporary
		if (com_modified)
			Sys_Error ("You must have the registered version to play QuakeWorld");
#endif
		return;
	}

	fread (check, 1, sizeof(check), h);
	fclose (h);
	
	for (i = 0; i < 128; i++)
	{
		if (pop[i] != (unsigned short)BigShort (check[i]))
		{
			Sys_Error ("Corrupted data file.");
			return;
		}
	}

	Cvar_Set ("cmdline", com_cmdline+1); //johnfitz -- eliminate leading space
	Cvar_ForceSet("registered", "1");
	static_registered = 1;
	Com_Printf ("Playing registered version.\n");
}



/*
================
COM_InitArgv
================
*/
void COM_InitArgv (int argc, char **argv)
{
	qboolean        safe;
	int             i, j, n;

// reconstitute the command line for the cmdline externally visible cvar
	n = 0;

	for (j=0 ; (j<MAX_NUM_ARGVS) && (j< argc) ; j++)
	{
		i = 0;

		while ((n < (CMDLINE_LENGTH - 1)) && argv[j][i])
		{
			com_cmdline[n++] = argv[j][i++];
		}

		if (n < (CMDLINE_LENGTH - 1))
			com_cmdline[n++] = ' ';
		else
			break;
	}

	com_cmdline[n] = 0;

	safe = false;

	for (com_argc=0 ; (com_argc<MAX_NUM_ARGVS) && (com_argc < argc) ; com_argc++)
	{
		largv[com_argc] = argv[com_argc];
		if (!Q_strcmp ("-safe", argv[com_argc]))
			safe = true;
	}

	if (safe)
	{
	// force all the safe-mode switches. Note that we reserved extra space in
	// case we need to add these, so we don't need an overflow check
		for (i=0 ; i<NUM_SAFE_ARGVS ; i++)
		{
			largv[com_argc] = safeargvs[i];
			com_argc++;
		}
	}

	largv[com_argc] = argvdummy;
	com_argv = largv;

	if (COM_CheckParm ("-rogue"))
	{
		rogue = true;
		standard_quake = false;
	}

	if (COM_CheckParm ("-hipnotic") || COM_CheckParm ("-quoth") || COM_CheckParm ("-nehahra") || COM_CheckParm ("-warp")) //johnfitz -- "-quoth" support
	{
		hipnotic = true;
		standard_quake = false;
	}

	if (COM_CheckParm ("-warp") || COM_CheckParm ("-nehahra")) /* FS: So we get larger Hunk_Alloc by default */
	{
		if (COM_CheckParm ("-nehahra"))
			nehahra = true;
		if (COM_CheckParm ("-warp"))
			warpspasm = true;

		extended_mod = true;
	}
}

/*
================
COM_AddParm

Adds the given string at the end of the current argument list
================
*/
void COM_AddParm (char *parm)
{
	largv[com_argc++] = parm;
}

/*
================
COM_Init
================
*/
void COM_Init (void)
{
	byte    swaptest[2] = {1,0};

// set the byte swapping variables in a portable manner 
	if ( *(short *)swaptest == 1)
	{
		bigendien = false;
		BigShort = ShortSwap;
		LittleShort = ShortNoSwap;
		BigLong = LongSwap;
		LittleLong = LongNoSwap;
		BigFloat = FloatSwap;
		LittleFloat = FloatNoSwap;
	}
	else
	{
		bigendien = true;
		BigShort = ShortNoSwap;
		LittleShort = ShortSwap;
		BigLong = LongNoSwap;
		LittleLong = LongSwap;
		BigFloat = FloatNoSwap;
		LittleFloat = FloatSwap;
	}

#ifdef QUAKE1
	if (nehahra) /* FS: For Nehara */
	{
		cutscene = Cvar_Get("cutscene", "1", CVAR_ARCHIVE); 
		Cvar_SetDescription("cutscene", "Special internal CVAR for Nehara mod.");
		nehx00 = Cvar_Get("nehx00", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx00", "Special internal CVAR for Nehara mod.");
		nehx01 = Cvar_Get("nehx01", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx01", "Special internal CVAR for Nehara mod.");
		nehx02 = Cvar_Get("nehx02", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx02", "Special internal CVAR for Nehara mod.");
		nehx03 = Cvar_Get("nehx03", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx03", "Special internal CVAR for Nehara mod.");
		nehx04 = Cvar_Get("nehx04", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx04", "Special internal CVAR for Nehara mod.");
		nehx05 = Cvar_Get("nehx05", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx05", "Special internal CVAR for Nehara mod.");
		nehx06 = Cvar_Get("nehx06", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx06", "Special internal CVAR for Nehara mod.");
		nehx07 = Cvar_Get("nehx07", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx07", "Special internal CVAR for Nehara mod.");
		nehx08 = Cvar_Get("nehx08", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx08", "Special internal CVAR for Nehara mod.");
		nehx09 = Cvar_Get("nehx09", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx09", "Special internal CVAR for Nehara mod.");
		nehx10 = Cvar_Get("nehx10", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx10", "Special internal CVAR for Nehara mod.");
		nehx11 = Cvar_Get("nehx11", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx11", "Special internal CVAR for Nehara mod.");
		nehx12 = Cvar_Get("nehx12", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx12", "Special internal CVAR for Nehara mod.");
		nehx13 = Cvar_Get("nehx13", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx13", "Special internal CVAR for Nehara mod.");
		nehx14 = Cvar_Get("nehx14", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx14", "Special internal CVAR for Nehara mod.");
		nehx15 = Cvar_Get("nehx15", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx15", "Special internal CVAR for Nehara mod.");
		nehx16 = Cvar_Get("nehx16", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx16", "Special internal CVAR for Nehara mod.");
		nehx17 = Cvar_Get("nehx17", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx17", "Special internal CVAR for Nehara mod.");
		nehx18 = Cvar_Get("nehx18", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx18", "Special internal CVAR for Nehara mod.");
		nehx19 = Cvar_Get("nehx19", "0", CVAR_ARCHIVE);
		Cvar_SetDescription("nehx19", "Special internal CVAR for Nehara mod.");
	}
#endif // QUAKE1

	registered = Cvar_Get("registered", "0", CVAR_NOSET|CVAR_PROTECTED);
	Cvar_SetDescription("registered", "Special internal CVAR for setting Registered game.");
#ifdef QUAKE1
	cmdline = Cvar_Get("cmdline","", 0);
	Cvar_SetDescription("cmdline", "Adds command line parameters as script statements\nCommands lead with a +, and continue until a - or another +\nquake +prog jctest.qp +cmd amlev1\nquake -nosound +cmd amlev1");
#endif // QUAKE1

	/* FS: Allow use of the error cmd */
	sv_allow_errorcmd = Cvar_Get("sv_allow_errorcmd", "0", CVAR_ARCHIVE);
	Cvar_SetDescription("sv_allow_errorcmd", "Allow the use of error command.");

	Cmd_AddCommand ("path", COM_Path_f);
	Cmd_AddCommand ("dir", COM_Dir_f); /* FS: From Quake 2 */
	Cmd_AddCommand ("error", COM_Error_f); /* FS: From Quake 2 */

	COM_InitFilesystem ();
	COM_CheckRegistered ();
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
============
*/
char *va (const char *fmt, ...)
{
	va_list     args;
	static char string[MAXPRINTMSG];

	va_start (args, fmt);
	Q_vsnprintf (string, sizeof(string), fmt, args);
	va_end (args);

	return string;
}

/// just for debugging
int memsearch (byte *start, int count, int search)
{
	int	i;
	
	for (i=0 ; i<count ; i++)
		if (start[i] == search)
			return i;
	return -1;
}

/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/

int com_filesize;


//
// in memory
//

typedef struct
{
	char	name[MAX_QPATH];
	long	hash;		// Knightmare added- To speed up searching
	int		filepos, filelen;
	qboolean		ignore;		// Knightmare added- whether this file should be ignored
} packfile_t;

typedef struct pack_s
{
	char	filename[MAX_OSPATH];
	FILE	*handle;
	int		numfiles;
	packfile_t	*files;
	unsigned int	contentFlags;	// Knightmare added- to skip cetain paks
} pack_t;

//
// on disk
//
typedef struct
{
	char	name[56];
	int		filepos, filelen;
} dpackfile_t;

typedef struct
{
	char	id[4];
	int		dirofs;
	int		dirlen;
} dpackheader_t;

#define MAX_FILES_IN_PACK	2048

char	com_basedir[MAX_OSPATH];
char	com_gamedir[MAX_OSPATH];

typedef struct searchpath_s
{
	char	filename[MAX_OSPATH];
	pack_t	*pack; // only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

searchpath_t	*com_searchpaths;
searchpath_t	*com_base_searchpaths;	// without gamedirs

// Knightmare added
static const char *type_extensions[] =
{
	"bsp",
	"mdl",
	"lmp",
	"dat",
	"rc",
	"spr",
	"wav",
	"mp3",
	"flac",
	"ogg",
	"pcx",
	"wal",
	"tga",
	"cfg",
	"txt",
	"ent",
	NULL
};

extern const char *COM_FileExtension (const char *in);

/*
============
COM_FileBase
============
*/
void COM_FileBase (char *in, char *out)
{
	char *s, *s2;

	if (!*in) {
		*out = 0;
		return;
	}
	s = in + strlen(in) - 1;

	while (s != in && *s != '.')
		s--;

	for (s2 = s ; s2 != in && *s2 != '/' ; s2--)
	 ;

	if (s-s2 < 2)
		strcpy (out,"?model?");
	else
	{
		s--;
		strncpy (out,s2+1, s-s2);
		out[s-s2] = 0;
	}
}

/*
=================
FS_TypeFlagForPakItem
Returns bit flag based on pak item's extension.
=================
*/
static unsigned int FS_TypeFlagForPakItem (const char *itemName)
{
	int		i;
	const char *tmp;

	tmp = COM_FileExtension (itemName);
	for (i = 0; type_extensions[i]; i++)
	{
		if (!stricmp(tmp, type_extensions[i]))
			return (1 << i);
	}
	return 0;
}
// end Knightmare

// Knightmare added
/*
=============
Com_HashFileName
=============
*/
long Com_HashFileName (const char *fname, int hashSize, qboolean sized)
{
	int		i = 0;
	int		count = 0;
	long	hash = 0;
	char	letter;

	if (fname[0] == '/' || fname[0] == '\\') i++;	// skip leading slash
	while (fname[i] != '\0')
	{
		letter = tolower(fname[i]);
		//	if (letter == '.') break;
		if (letter == '\\')
			letter = '/';	// fix filepaths

		/* FS: Sounds with extra slashes will fail this test and not load them.  We have to fix it here... */
		if (i > 0 && fname[i - 1] == '/')
		{
			if (letter == '/')
			{
				i++;
				continue;
			}
		}

		hash += (long)(letter) * (count + 119);
		count++;
		i++;
	}
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	if (sized)
	{
		hash &= (hashSize - 1);
	}
	return hash;
}

//performs a binary search through the pack file entries looking for the given file name.
static int FindFileInPack (pack_t *pak, const char *filename, long itemHash)
{
	int top, bottom, middle, compare; //the boundaries of our search.

	//set the initial top and bottom.
	top = 0;
	bottom = pak->numfiles - 1; /* FS: Out-of-bounds unreachable address found by Dr. Memory */

	//search until top and bottom get close.
	while (bottom - top > 5)
	{
		middle = (top + bottom) / 2;

		compare = itemHash - pak->files[middle].hash;

		if (compare == 0)
		{
			break;
		}
		else if (compare < 0) //check if it is above the middle.
		{
			//the file is above this one.
			bottom = middle - 1;
			continue;
		}
		else //check if it is below the middle.
		{
			//the file is below this one.
			top = middle + 1;
			continue;
		}
	}

	//linear search through the remaining indices.
	for (; top <= bottom; top++)
	{
		if (pak->files[top].hash == itemHash && !pak->files[top].ignore && !stricmp(filename, pak->files[top].name))
		{
			//found it.
//			Com_DPrintf(DEVELOPER_MSG_IO, "File: %s found in %d searches\n", filename, count );
			return top;
		}
	}

	//could not find the file
	return -1;
}

/*
================
COM_filelength
================
*/
int COM_filelength (FILE *f)
{
	int		pos;
	int		end;

	if (!f)
		return 0;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

int COM_FileOpenRead (char *path, FILE **hndl)
{
	FILE	*f;

	f = fopen(path, "rb");
	if (!f)
	{
		*hndl = NULL;
		return -1;
	}
	*hndl = f;
	
	return COM_filelength(f);
}

/*
============
COM_Path_f

============
*/
void COM_Path_f (void)
{
	searchpath_t	*s;
	
	Com_Printf ("Current search path:\n");
	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (s == com_base_searchpaths)
			Com_Printf ("----------\n");
		if (s->pack)
			Com_Printf ("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		else
			Com_Printf ("%s\n", s->filename);
	}
}

/*
============
COM_WriteFile

The filename will be prefixed by the current game directory
============
*/
void COM_WriteFile (const char *filename, void *data, int len)
{
	FILE	*f;
	char	name[MAX_OSPATH];

	Sys_mkdir (com_gamedir); //johnfitz -- if we've switched to a nonexistant gamedir, create it now so we don't crash

	Com_sprintf (name, sizeof(name), "%s/%s", com_gamedir, filename);
	
	f = fopen (name, "wb");
	if (!f) {
		Sys_mkdir(com_gamedir);
		f = fopen (name, "wb");
		if (!f)
		{
			Sys_Error ("Error opening %s", filename);
			return;
		}
	}
	
	Sys_Printf ("COM_WriteFile: %s\n", name);
	fwrite (data, 1, len, f);
	fclose (f);
}


/*
============
COM_CreatePath

Only used for CopyFile and download
============
*/
void	COM_CreatePath (char *path)
{
	char	*ofs;
	
	for (ofs = path+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/')
		{	// create the directory
			*ofs = 0;
			Sys_mkdir (path);
			*ofs = '/';
		}
	}
}


/*
===========
COM_CopyFile

Copies a file over from the net to the local cache, creating any directories
needed.  This is for the convenience of developers using ISDN from home.
===========
*/
void COM_CopyFile (char *netpath, char *cachepath)
{
	FILE	*in, *out;
	size_t	remaining, count;
	char	buf[4096];
	
	remaining = COM_FileOpenRead (netpath, &in);		
	COM_CreatePath (cachepath);	// create directories up to the cache file
	out = fopen(cachepath, "wb");
	if (!out)
	{
		Sys_Error ("COM_CopyFile: error opening %s", cachepath);
		return;
	}
	
	while (remaining)
	{
		if (remaining < sizeof(buf))
			count = remaining;
		else
			count = sizeof(buf);
		fread (buf, 1, count, in);
		fwrite (buf, 1, count, out);
		remaining -= count;
	}

	fclose (in);
	fclose (out);
}

/*
===========
COM_FOpenFile

Finds the file in the search path.
Sets com_filesize and one of handle or file
===========
*/

int COM_FOpenFile (const char *filename, FILE **file)
{
	searchpath_t	*search;
	char	netpath[MAX_OSPATH];
	pack_t		*pak;
	int			index;
	int			findtime;
	// Knightmare added
	long			hash;
	unsigned int	typeFlag;

	// Knightmare added
	hash = Com_HashFileName(filename, 0, false);
	typeFlag = FS_TypeFlagForPakItem(filename);

//
// search through the path, one element at a time
//
	for (search = com_searchpaths ; search ; search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
			// look through all the pak file elements
			pak = search->pack;

			// Knightmare- skip if pack doesn't contain this type of file
			if (typeFlag != 0 && !(pak->contentFlags & typeFlag))
				continue;

			//get the index that the file is stored at
			index = FindFileInPack (pak, filename, hash);
			if (index != -1)
			{	// found it!
				Sys_Printf ("PackFile: %s : %s\n",pak->filename, filename);
				// open a new file on the pakfile
				*file = fopen (pak->filename, "rb");
				if (!*file)
				{
					Sys_Error ("Couldn't reopen %s", pak->filename);
					return -1;
				}
				fseek (*file, pak->files[index].filepos, SEEK_SET);
				return pak->files[index].filelen;
			}
		}
		else
		{		
			// check a file in the directory tree
			if (!static_registered)
			{	// if not a registered version, don't ever go beyond base
				if ( strchr (filename, '/') || strchr (filename,'\\'))
					continue;
			}
			
			Com_sprintf (netpath, sizeof(netpath), "%s/%s",search->filename, filename);
			
			findtime = Sys_FileTime (netpath);
			if (findtime == -1)
				continue;
				
			Sys_Printf ("FindFile: %s\n",netpath);

			*file = fopen (netpath, "rb");
			return COM_filelength (*file);
		}
	}
	
	Sys_Printf ("FindFile: can't find %s\n", filename);
	
	*file = NULL;
	com_filesize = -1;
	return -1;
}

int COM_FileExists (const char *filename) /* FS */
{
	searchpath_t	*search;
	char	netpath[MAX_OSPATH];
	pack_t		*pak;
	int			index;
	int			findtime;
	// Knightmare added
	long			hash;
	unsigned int	typeFlag;

	// Knightmare added
	hash = Com_HashFileName(filename, 0, false);
	typeFlag = FS_TypeFlagForPakItem(filename);

//
// search through the path, one element at a time
//
	for (search = com_searchpaths ; search ; search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
			// look through all the pak file elements
			pak = search->pack;

			// Knightmare- skip if pack doesn't contain this type of file
			if (typeFlag != 0 && !(pak->contentFlags & typeFlag))
				continue;

			//get the index that the file is stored at
			index = FindFileInPack (pak, filename, hash);
			if (index != -1)
			{	// found it!
				return 1;
			}
		}
		else
		{
			// check a file in the directory tree
			if (!static_registered)
			{	// if not a registered version, don't ever go beyond base
				if ( strchr (filename, '/') || strchr (filename,'\\'))
					continue;
			}

			Com_sprintf (netpath, sizeof(netpath), "%s/%s",search->filename, filename);

			findtime = Sys_FileTime (netpath);
			if (findtime == -1)
				continue;

			return 1;
		}
	}

	Sys_Printf ("FindFile: can't find %s\n", filename);
	return 0;
}

#define	MAX_READ	0x10000		// read in blocks of 64k

/*
=================
FS_Read

Properly handles partial reads
=================
*/
void FS_Read (void *buffer, int len, FILE *f)
{
	int		block, remaining;
	int		read;
	byte	*buf;
	int		tries;

	buf = (byte *)buffer;

	// read in chunks for progress bar
	remaining = len;
	tries = 0;
	while (remaining)
	{
		block = remaining;
		if (block > MAX_READ)
			block = MAX_READ;
		read = fread (buf, 1, block, f);
		if (read == 0)
		{
			// we might have been trying to read from a CD
			if (!tries)
			{
				tries = 1;
				CDAudio_Stop();
			}
			else
				Sys_Error ("FS_Read: 0 bytes read");
		}

		// do some progress bar thing here...

		remaining -= read;
		buf += read;
	}
}

void COM_FreeFile (void *buffer)
{
	if (buffer)
		Z_Free (buffer);
}

/*
===========
COM_OpenFile

filename never has a leading slash, but may contain directory walks
returns a handle and a length
it may actually be inside a pak file
===========
*/
int COM_OpenFile (char *filename, FILE *handle)
{
	return COM_FOpenFile (filename, &handle);
}

/*
============
COM_CloseFile

If it is a pak file handle, don't really close it
============
*/
void COM_CloseFile (FILE *h)
{
	searchpath_t    *s;
	
	for (s = com_searchpaths ; s ; s=s->next)
		if (s->pack && s->pack->handle == h)
			return;
			
	fclose(h);
}


/*
============
COM_LoadFile

Filename are reletive to the quake directory.
Allways appends a 0 byte.
============
*/

cache_user_t *loadcache;
byte    *loadbuf;
int             loadsize;

byte *COM_LoadFile (char *path, loadfiletype_t usehunk)
{
	FILE    *h;
	byte    *buf;
	char    base[32];
	int             len;

	buf = NULL;     // quiet compiler warning

// look for it in the filesystem or pack files
	len = COM_FOpenFile (path, &h);
	if (len == -1)
		return NULL;
	
// extract the filename base name for hunk tag
	COM_FileBase (path, base);
	
	switch (usehunk)
	{
		case COM_LOADFILE_ZMALLOC:
			buf = Z_Malloc (len + 1);
			break;
		case COM_LOADFILE_HUNKALLOCNAME:
			buf = Hunk_AllocName (len + 1, base);
			break;
		case COM_LOADFILE_TEMPALLOC:
			buf = Hunk_TempAlloc (len + 1);
			break;
		case COM_LOADFILE_CACHEALLOC:
			buf = Cache_Alloc (loadcache, len + 1, base);
			break;
		case COM_LOADFILE_STACKFILE:
			if (len + 1 > loadsize)
				buf = Hunk_TempAlloc (len + 1);
			else
				buf = loadbuf;
			break;
		case COM_LOADFILE_CALLOC:
			buf = calloc(1, len + 1);
			break;
		default:
			Sys_Error ("COM_LoadFile: bad usehunk %d", usehunk);
			return NULL;
	}

	if (!buf)
	{
		Sys_Error ("COM_LoadFile: not enough space for %s", path);
		return NULL;
	}

	Draw_BeginDisc ();
	FS_Read (buf, len, h);
	COM_CloseFile (h);
	Draw_EndDisc ();

	com_filesize = len;

	buf[len] = 0;

	return buf;
}

byte *COM_LoadHunkFile (char *path)
{
	return COM_LoadFile (path, COM_LOADFILE_HUNKALLOCNAME);
}

byte *COM_LoadTempFile (char *path)
{
	return COM_LoadFile (path, COM_LOADFILE_TEMPALLOC);
}

void COM_LoadCacheFile (char *path, struct cache_user_s *cu)
{
	loadcache = cu;
	COM_LoadFile (path, COM_LOADFILE_CACHEALLOC);
}

// uses temp hunk if larger than bufsize
byte *COM_LoadStackFile (char *path, void *buffer, int bufsize)
{
	byte    *buf;
	
	loadbuf = (byte *)buffer;
	loadsize = bufsize;
	buf = COM_LoadFile (path, COM_LOADFILE_STACKFILE);
	
	return buf;
}

/*
=================
COM_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/

//compare function used to sort the pack file entries based on their names.
static long *name_hashes = NULL;

static int pack_file_compare_hash (const void *e1, const void *e2)
{
	if (!name_hashes)
		return 1;

	return (name_hashes[*((int *)(e1))] - name_hashes[*((int *)(e2))]);
}

pack_t *COM_LoadPackFile (char *packfile)
{
	dpackheader_t	header;
	int				i;
	packfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	FILE			*packhandle;
	dpackfile_t		info[MAX_FILES_IN_PACK];
	unsigned short	crc;
	int			 *sort_table;
	long		*sort_hashes;
	unsigned int	contentFlags = 0;

	if (COM_FileOpenRead (packfile, &packhandle) == -1)
		return NULL;

	fread (&header, 1, sizeof(header), packhandle);
	if (header.id[0] != 'P' || header.id[1] != 'A' || header.id[2] != 'C' || header.id[3] != 'K')
	{
		Sys_Error ("%s is not a packfile", packfile);
		return NULL;
	}

	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

	if (header.dirlen < 0 || header.dirofs < 0)
	{
		Sys_Error ("Invalid packfile %s (dirlen: %i, dirofs: %i)",
					packfile, header.dirlen, header.dirofs);
		return NULL;
	}

	if (!numpackfiles)
	{
		Com_Printf ("WARNING: %s has no files, ignored\n", packfile);
		fclose (packhandle);
		return NULL;
	}

	if (numpackfiles > MAX_FILES_IN_PACK)
	{
		Sys_Error ("%s has %i files", packfile, numpackfiles);
		return NULL;
	}

	if (numpackfiles != PAK0_COUNT)
		com_modified = true;    // not the original file

	newfiles = Z_Malloc(numpackfiles * sizeof(packfile_t));

	fseek (packhandle, header.dirofs, SEEK_SET);
	fread (&info, 1, header.dirlen, packhandle);

	// crc the directory to check for modifications
#ifdef QUAKE1
	CRC_Init (&crc);
	for (i = 0; i < header.dirlen ; i++)
		CRC_ProcessByte (&crc, ((byte *)info)[i]);
	if (crc != PAK0_CRC_V106 && crc != PAK0_CRC_V101 && crc != PAK0_CRC_V100)
		com_modified = true;
#else
	crc = CRC_Block((byte *)info, header.dirlen);
	if (crc != PAK0_CRC)
		com_modified = true;
#endif // QUAKE1

	//alloc our array of file descriptors.
	newfiles = Z_Malloc (numpackfiles * sizeof(packfile_t));
	sort_table = Z_Malloc(numpackfiles * sizeof(int));
	sort_hashes = Z_Malloc(numpackfiles * sizeof(long));

	//initialize our sort table.
	for (i = 0; i < numpackfiles; i++)
	{
		sort_table[i] = i;
		sort_hashes[i] = Com_HashFileName(info[i].name, 0, false);
	}

	//give our compare function the name hashes array.
	name_hashes = &sort_hashes[0];

	//sort our table of indexes.
	qsort(&sort_table[0], numpackfiles, sizeof(int), pack_file_compare_hash);

	name_hashes = NULL;

	// parse the directory
	for (i = 0; i < numpackfiles ; i++)
	{
		Q_strlcpy (newfiles[i].name, info[sort_table[i]].name, sizeof(newfiles[i].name));
		newfiles[i].hash = sort_hashes[sort_table[i]];

		newfiles[i].filepos = LittleLong(info[sort_table[i]].filepos);
		newfiles[i].filelen = LittleLong(info[sort_table[i]].filelen);

		contentFlags |= FS_TypeFlagForPakItem(newfiles[i].name);
	}

	pack = Z_Malloc (sizeof (pack_t));
	Q_strlcpy (pack->filename, packfile, sizeof(pack->filename));
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;
	pack->contentFlags = contentFlags;	// Knightmare added
	
	Com_Printf ("Added packfile %s (%i files)\n", packfile, numpackfiles);

	Z_Free(sort_table);
	Z_Free(sort_hashes);

	return pack;
}

/* FS: From Q2 */
char **COM_ListFiles( char *findname, int *numfiles, unsigned musthave, unsigned canthave )
{
	char *s;
	int nfiles = 0;
	char **list = 0;

	s = Sys_FindFirst( findname, musthave, canthave );
	while ( s )
	{
		if ( s[strlen(s)-1] != '.' )
			nfiles++;
		s = Sys_FindNext( musthave, canthave );
	}
	Sys_FindClose ();

	if ( !nfiles ) {
		*numfiles = 0;
		return NULL;
	}

	nfiles++; // add space for a guard
	*numfiles = nfiles;

	list = malloc( sizeof( char * ) * nfiles );
	if (!list)
	{
		Sys_Error("COM_ListFiles: out of memory");
		return NULL;
	}

	memset( list, 0, sizeof( char * ) * nfiles );

	s = Sys_FindFirst( findname, musthave, canthave );
	nfiles = 0;
	while ( s )
	{
		if ( s[strlen(s)-1] != '.' )
		{
			list[nfiles] = strdup( s );
			if (!list[nfiles])
			{
				free(list);
				Sys_Error("COM_ListFiles: out of memory");
				return NULL;
			}
#if defined(_WIN32) || defined(__MSDOS__)
			strlwr( list[nfiles] );
#endif
			nfiles++;
		}
		s = Sys_FindNext( musthave, canthave );
	}
	Sys_FindClose ();

	return list;
}

/* FS: From Q2 */
char *COM_NextPath (char *prevpath)
{
	searchpath_t	*s;
	char			*prev;

	if (!prevpath)
		return com_gamedir;

	prev = com_gamedir;
	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (s->pack)
			continue;
		if (prevpath == prev)
			return s->filename;
		prev = s->filename;
	}

	return NULL;
}

/* FS: From Q2 */
void COM_FreeFileList (char **list, int n)
{
	int i;

	for (i = 0; i < n; i++)
	{
		if (list && list[i])
		{
			free(list[i]);
			list[i] = 0;
		}
	}
	free(list);
}

/* FS: From Q2 */
qboolean COM_ItemInList (char *check, int num, char **list)
{
	int		i;

	if (!check || !list)
		return false;
	for (i=0; i<num; i++)
	{
		if (!list[i])
			continue;
		if (!Q_strcasecmp(check, list[i]))
			return true;
	}
	return false;
}

/* FS: From Quake 2 */
void COM_Dir_f (void)
{
	char	*path = NULL;
	char	findname[1024];
	char	wildcard[1024] = "*.*";
	char	**dirnames;
	int		ndirs;

	if ( Cmd_Argc() != 1 )
	{
		Q_strlcpy( wildcard, Cmd_Argv( 1 ), sizeof(wildcard) );
	}

	while ( ( path = COM_NextPath( path ) ) != NULL )
	{
		char *tmp = findname;

		Com_sprintf( findname, sizeof(findname), "%s/%s", path, wildcard );

		while ( *tmp != 0 )
		{
			if ( *tmp == '\\' ) 
				*tmp = '/';
			tmp++;
		}
		Com_Printf( "Directory of %s\n", findname );
		Com_Printf( "----\n" );

		if ( ( dirnames = COM_ListFiles( findname, &ndirs, 0, 0 ) ) != 0 )
		{
			int i;

			for ( i = 0; i < ndirs-1; i++ )
			{
				if ( strrchr( dirnames[i], '/' ) )
					Com_Printf( "%s\n", strrchr( dirnames[i], '/' ) + 1 );
				else
					Com_Printf( "%s\n", dirnames[i] );

				free( dirnames[i] );
			}
			free( dirnames );
		}
		Com_Printf( "\n" );
	};
}

/*
=================
COM_AddGameDirectory -- johnfitz -- modified based on topaz's tutorial

Sets com_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ... 
=================
*/
void COM_AddGameDirectory (char *dir)
{
	int				i;
	searchpath_t	*search;
	pack_t			*pak;
	char			pakfile[MAX_OSPATH];
	char			*p;

	if (!dir)
	{
		return;
	}

	if ((p = strrchr(dir, '/')) != NULL)
		Q_strlcpy(gamedirfile, ++p, sizeof(gamedirfile));
	else if ((p = strrchr(dir, '\\')) != NULL) /* FS: For -cddir */
		Q_strlcpy(gamedirfile, ++p, sizeof(gamedirfile));
	else
		Q_strlcpy(gamedirfile, p, sizeof(gamedirfile));
	Q_strlcpy (com_gamedir, dir, sizeof(com_gamedir));

//
// add the directory to the search path
//
	search = Z_Malloc(sizeof(searchpath_t));
	Q_strlcpy (search->filename, dir, sizeof(search->filename));
	search->next = com_searchpaths;
	com_searchpaths = search;

//
// add any pak files in the format pak0.pak pak1.pak, ...
//
	for (i=0 ; ; i++)
	{
		Com_sprintf (pakfile, sizeof(pakfile), "%s/pak%i.pak", dir, i);
		pak = COM_LoadPackFile (pakfile);
		if (!pak)
			break;
		search = Z_Malloc(sizeof(searchpath_t));
		search->pack = pak;
		search->next = com_searchpaths;
		com_searchpaths = search;
	}
}

/*
================
COM_Gamedir

Sets the gamedir and path to a different directory.
================
*/
void COM_Gamedir (char *dir)
{
	searchpath_t	*search, *next;
	int				i;
	pack_t			*pak;
	char			pakfile[MAX_OSPATH];

	if (!dir)
	{
		return;
	}

	if (strstr(dir, "..") || strchr(dir, '/')
		|| strchr(dir, '\\') || strchr(dir, ':') )
	{
		Com_Printf ("Gamedir should be a single filename, not a path\n");
		return;
	}

	if (!strcmp(gamedirfile, dir))
		return;		// still the same

	Q_strlcpy (gamedirfile, dir, sizeof(gamedirfile));

	//
	// free up any current game dir info
	//
	while (com_searchpaths != com_base_searchpaths)
	{
		if (com_searchpaths->pack)
		{
			fclose (com_searchpaths->pack->handle);
			Z_Free (com_searchpaths->pack->files);
			Z_Free (com_searchpaths->pack);
		}
		next = com_searchpaths->next;
		Z_Free (com_searchpaths);
		com_searchpaths = next;
	}

	if (!strcmp(dir,"id1") || !strcmp(dir, "qw"))
		return;

	Com_sprintf (com_gamedir, sizeof(com_gamedir), "%s/%s", com_basedir, dir);

	//
	// add the directory to the search path
	//
	search = Z_Malloc (sizeof(searchpath_t));
	Q_strlcpy (search->filename, com_gamedir, sizeof(search->filename));
	search->next = com_searchpaths;
	com_searchpaths = search;

	//
	// add any pak files in the format pak0.pak pak1.pak, ...
	//
	for (i=0 ; ; i++)
	{
		Com_sprintf (pakfile, sizeof(pakfile), "%s/pak%i.pak", com_gamedir, i);
		pak = COM_LoadPackFile (pakfile);
		if (!pak)
			break;
		search = Z_Malloc (sizeof(searchpath_t));
		search->pack = pak;
		search->next = com_searchpaths;
		com_searchpaths = search;		
	}
}

/*
================
COM_InitFilesystem
================
*/
void COM_InitFilesystem (void) //johnfitz -- modified based on topaz's tutorial
{
	int             i, j;
	searchpath_t    *search;

//
// -basedir <path>
// Overrides the system supplied base directory (under id1)
//
	i = COM_CheckParm ("-basedir");
	if (i && i < com_argc-1)
		Q_strlcpy (com_basedir, com_argv[i+1], sizeof(com_basedir));
	else
		Q_strlcpy (com_basedir, host_parms.basedir, sizeof(com_basedir));

	j = strlen (com_basedir);

	if (j > 0)
	{
		if ((com_basedir[j-1] == '\\') || (com_basedir[j-1] == '/'))
			com_basedir[j-1] = 0;
	}

//
// start up with id1 by default
//
	COM_AddGameDirectory (va("%s/id1", com_basedir) );
	Q_strlcpy (com_gamedir, va("%s/id1", com_basedir), sizeof(com_gamedir));
#ifdef QUAKEWORLD
	COM_AddGameDirectory (va("%s/qw", com_basedir) );
	Q_strlcpy (com_gamedir, va("%s/qw", com_basedir), sizeof(com_gamedir));
#endif

	//johnfitz -- track number of mission packs added
	//since we don't want to allow the "game" command to strip them away
	com_nummissionpacks = 0;
	if (COM_CheckParm ("-rogue"))
	{
		COM_AddGameDirectory (va("%s/rogue", com_basedir) );
		com_nummissionpacks++;
	}

	if (COM_CheckParm ("-hipnotic"))
	{
		COM_AddGameDirectory (va("%s/hipnotic", com_basedir) );
		com_nummissionpacks++;
	}

	if (COM_CheckParm ("-quoth"))
	{
		COM_AddGameDirectory (va("%s/quoth", com_basedir) );
		com_nummissionpacks++;
	}

	if (COM_CheckParm ("-nehahra")) /* FS: Nehahra */
	{
		COM_AddGameDirectory (va("%s/hipnotic", com_basedir) );
		com_nummissionpacks++;
		COM_AddGameDirectory (va("%s/nehahra", com_basedir) );
		com_nummissionpacks++;
	}

	if (COM_CheckParm ("-warp")) /* FS: Warpspasm */
	{
		COM_AddGameDirectory (va("%s/quoth", com_basedir) );
		com_nummissionpacks++;
		COM_AddGameDirectory (va("%s/warp", com_basedir) );
		com_nummissionpacks++;
	}                
	//johnfitz

	i = COM_CheckParm ("-cddir"); /* FS: One of my computers has 3 small drives and I keep the WAVs on a separate drive... */
	if(i && i < com_argc-1)
	{
		COM_AddGameDirectory ( va("%s", com_argv[i+1]));
	}

	i = COM_CheckParm ("-game");
	if (i && i < com_argc-1)
	{
		com_modified = true;
		COM_AddGameDirectory (va("%s/%s", com_basedir, com_argv[i+1]));
	}

//
// -path <dir or packfile> [<dir or packfile>] ...
// Fully specifies the exact search path, overriding the generated one
//
	i = COM_CheckParm ("-path");
	if (i)
	{
		com_modified = true;
		com_searchpaths = NULL;
		while (++i < com_argc)
		{
			if (!com_argv[i] || com_argv[i][0] == '+' || com_argv[i][0] == '-')
				break;
			
			search = Hunk_Alloc (sizeof(searchpath_t));
			if ( !strcmp(COM_FileExtension(com_argv[i]), "pak") )
			{
				search->pack = COM_LoadPackFile (com_argv[i]);
				if (!search->pack)
				{
					Sys_Error ("Couldn't load packfile: %s", com_argv[i]);
					return;
				}
			}
			else
				Q_strlcpy (search->filename, com_argv[i], sizeof(search->filename));
			search->next = com_searchpaths;
			com_searchpaths = search;
		}
	}

	// any set gamedirs will be freed up to here
	com_base_searchpaths = com_searchpaths;
}

/*
=====================================================================

  INFO STRINGS

=====================================================================
*/

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
char *Info_ValueForKey (char *s, const char *key)
{
	char	pkey[512];
	static	char value[4][512];	// use two buffers so compares
								// work without stomping on each other
	static	int	valueindex;
	char	*o;
	
	valueindex = (valueindex + 1) % 4;
	if (*s == '\\')
		s++;
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

void Info_RemoveKey (char *s, const char *key)
{
	char	*start;
	char	pkey[512];
	char	value[512];
	char	*o;

	if (!s || !key)
	{
		return;
	}

	if (strchr (key, '\\'))
	{
		Com_Printf ("Can't use a key with a \\\n");
		return;
	}

	while (1)
	{
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
		{
			memmove(start, s, strlen(s) + 1);	// remove this part
			return;
		}

		if (!*s)
			return;
	}

}

void Info_RemovePrefixedKeys (char *start, char prefix)
{
	char	*s;
	char	pkey[512];
	char	value[512];
	char	*o;

	s = start;

	while (1)
	{
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (pkey[0] == prefix)
		{
			Info_RemoveKey (start, pkey);
			s = start;
		}

		if (!*s)
			return;
	}

}

void Info_SetValueForStarKey (char *s, const char *key, const char *value, size_t maxsize)
{
	char	new[1024], *v;
	int		c;
#ifdef SERVERONLY
	extern cvar_t sv_highchars;
#endif

	if (!s || !key || !value)
	{
		return;
	}

	if (strchr (key, '\\') || strchr (value, '\\') )
	{
		Com_Printf ("Can't use keys or values with a \\\n");
		return;
	}

	if (strchr (key, '\"') || strchr (value, '\"'))
	{
		Com_Printf ("Can't use keys or values with a \"\n");
		return;
	}

	if (strlen(key) > 63 || strlen(value) > 63)
	{
		Com_Printf ("Keys and values must be < 64 characters.\n");
		return;
	}

	// this next line is kinda trippy
	if (*(v = Info_ValueForKey(s, key))) {
		// key exists, make sure we have enough room for new value, if we don't,
		// don't change it!
		if (strlen(value) - strlen(v) + strlen(s) > maxsize) {
			Com_Printf ("Info string length exceeded\n");
			return;
		}
	}
	Info_RemoveKey (s, key);
	if (!value || !strlen(value))
		return;

	Com_sprintf (new, sizeof(new), "\\%s\\%s", key, value);

	if ((strlen(new) + strlen(s)) > maxsize)
	{
		Com_Printf ("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = new;
	while (*v)
	{
		c = (unsigned char)*v++;
#ifndef SERVERONLY
		// client only allows highbits on name
		if (stricmp(key, "name") != 0) {
			c &= 127;
			if (c < 32 || c > 127)
				continue;
			// auto lowercase team
			if (stricmp(key, "team") == 0)
				c = tolower(c);
		}
#else
		if (!sv_highchars->value) {
			c &= 127;
			if (c < 32 || c > 127)
				continue;
		}
#endif
//		c &= 127;		// strip high bits
		if (c > 13) // && c < 127)
			*s++ = c;
	}
	*s = 0;
}

void Info_SetValueForKey (char *s, const char *key, const char *value, size_t maxsize)
{
	if (key[0] == '*')
	{
		Com_Printf ("Can't set * keys\n");
		return;
	}

	Info_SetValueForStarKey (s, key, value, maxsize);
}

void Info_Print (char *s)
{
	char	key[512];
	char	value[512];
	char	*o;
	int		l;

	if (*s == '\\')
		s++;
	while (*s)
	{
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		if (l < 20)
		{
			memset (o, ' ', 20-l);
			key[20] = 0;
		}
		else
			*o = 0;
		Com_Printf ("%s", key);

		if (!*s)
		{
			Com_Printf ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		Com_Printf ("%s\n", value);
	}
}

static byte chktbl[1024 + 4] = {
0x78,0xd2,0x94,0xe3,0x41,0xec,0xd6,0xd5,0xcb,0xfc,0xdb,0x8a,0x4b,0xcc,0x85,0x01,
0x23,0xd2,0xe5,0xf2,0x29,0xa7,0x45,0x94,0x4a,0x62,0xe3,0xa5,0x6f,0x3f,0xe1,0x7a,
0x64,0xed,0x5c,0x99,0x29,0x87,0xa8,0x78,0x59,0x0d,0xaa,0x0f,0x25,0x0a,0x5c,0x58,
0xfb,0x00,0xa7,0xa8,0x8a,0x1d,0x86,0x80,0xc5,0x1f,0xd2,0x28,0x69,0x71,0x58,0xc3,
0x51,0x90,0xe1,0xf8,0x6a,0xf3,0x8f,0xb0,0x68,0xdf,0x95,0x40,0x5c,0xe4,0x24,0x6b,
0x29,0x19,0x71,0x3f,0x42,0x63,0x6c,0x48,0xe7,0xad,0xa8,0x4b,0x91,0x8f,0x42,0x36,
0x34,0xe7,0x32,0x55,0x59,0x2d,0x36,0x38,0x38,0x59,0x9b,0x08,0x16,0x4d,0x8d,0xf8,
0x0a,0xa4,0x52,0x01,0xbb,0x52,0xa9,0xfd,0x40,0x18,0x97,0x37,0xff,0xc9,0x82,0x27,
0xb2,0x64,0x60,0xce,0x00,0xd9,0x04,0xf0,0x9e,0x99,0xbd,0xce,0x8f,0x90,0x4a,0xdd,
0xe1,0xec,0x19,0x14,0xb1,0xfb,0xca,0x1e,0x98,0x0f,0xd4,0xcb,0x80,0xd6,0x05,0x63,
0xfd,0xa0,0x74,0xa6,0x86,0xf6,0x19,0x98,0x76,0x27,0x68,0xf7,0xe9,0x09,0x9a,0xf2,
0x2e,0x42,0xe1,0xbe,0x64,0x48,0x2a,0x74,0x30,0xbb,0x07,0xcc,0x1f,0xd4,0x91,0x9d,
0xac,0x55,0x53,0x25,0xb9,0x64,0xf7,0x58,0x4c,0x34,0x16,0xbc,0xf6,0x12,0x2b,0x65,
0x68,0x25,0x2e,0x29,0x1f,0xbb,0xb9,0xee,0x6d,0x0c,0x8e,0xbb,0xd2,0x5f,0x1d,0x8f,
0xc1,0x39,0xf9,0x8d,0xc0,0x39,0x75,0xcf,0x25,0x17,0xbe,0x96,0xaf,0x98,0x9f,0x5f,
0x65,0x15,0xc4,0x62,0xf8,0x55,0xfc,0xab,0x54,0xcf,0xdc,0x14,0x06,0xc8,0xfc,0x42,
0xd3,0xf0,0xad,0x10,0x08,0xcd,0xd4,0x11,0xbb,0xca,0x67,0xc6,0x48,0x5f,0x9d,0x59,
0xe3,0xe8,0x53,0x67,0x27,0x2d,0x34,0x9e,0x9e,0x24,0x29,0xdb,0x69,0x99,0x86,0xf9,
0x20,0xb5,0xbb,0x5b,0xb0,0xf9,0xc3,0x67,0xad,0x1c,0x9c,0xf7,0xcc,0xef,0xce,0x69,
0xe0,0x26,0x8f,0x79,0xbd,0xca,0x10,0x17,0xda,0xa9,0x88,0x57,0x9b,0x15,0x24,0xba,
0x84,0xd0,0xeb,0x4d,0x14,0xf5,0xfc,0xe6,0x51,0x6c,0x6f,0x64,0x6b,0x73,0xec,0x85,
0xf1,0x6f,0xe1,0x67,0x25,0x10,0x77,0x32,0x9e,0x85,0x6e,0x69,0xb1,0x83,0x00,0xe4,
0x13,0xa4,0x45,0x34,0x3b,0x40,0xff,0x41,0x82,0x89,0x79,0x57,0xfd,0xd2,0x8e,0xe8,
0xfc,0x1d,0x19,0x21,0x12,0x00,0xd7,0x66,0xe5,0xc7,0x10,0x1d,0xcb,0x75,0xe8,0xfa,
0xb6,0xee,0x7b,0x2f,0x1a,0x25,0x24,0xb9,0x9f,0x1d,0x78,0xfb,0x84,0xd0,0x17,0x05,
0x71,0xb3,0xc8,0x18,0xff,0x62,0xee,0xed,0x53,0xab,0x78,0xd3,0x65,0x2d,0xbb,0xc7,
0xc1,0xe7,0x70,0xa2,0x43,0x2c,0x7c,0xc7,0x16,0x04,0xd2,0x45,0xd5,0x6b,0x6c,0x7a,
0x5e,0xa1,0x50,0x2e,0x31,0x5b,0xcc,0xe8,0x65,0x8b,0x16,0x85,0xbf,0x82,0x83,0xfb,
0xde,0x9f,0x36,0x48,0x32,0x79,0xd6,0x9b,0xfb,0x52,0x45,0xbf,0x43,0xf7,0x0b,0x0b,
0x19,0x19,0x31,0xc3,0x85,0xec,0x1d,0x8c,0x20,0xf0,0x3a,0xfa,0x80,0x4d,0x2c,0x7d,
0xac,0x60,0x09,0xc0,0x40,0xee,0xb9,0xeb,0x13,0x5b,0xe8,0x2b,0xb1,0x20,0xf0,0xce,
0x4c,0xbd,0xc6,0x04,0x86,0x70,0xc6,0x33,0xc3,0x15,0x0f,0x65,0x19,0xfd,0xc2,0xd3,

// map checksum goes here
0x00,0x00,0x00,0x00
};

/*
====================
COM_BlockSequenceCRCByte

For proxy protecting
====================
*/
byte	COM_BlockSequenceCRCByte (byte *base, int length, int sequence)
{
	unsigned short crc;
	byte	*p;
	byte chkb[60 + 4];

	p = chktbl + (sequence % (sizeof(chktbl) - 8));

	if (length > 60)
		length = 60;
	memcpy (chkb, base, length);

	chkb[length] = (sequence & 0xff) ^ p[0];
	chkb[length+1] = p[1];
	chkb[length+2] = ((sequence>>8) & 0xff) ^ p[2];
	chkb[length+3] = p[3];

	length += 4;

	crc = CRC_Block(chkb, length);

	crc &= 0xff;

	return crc;
}

// char *date = "Oct 24 1996";
static const char *date = __DATE__ ;
static const char *mon[12] = 
{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static const char mond[12] = 
{ 31,    28,    31,    30,    31,    30,    31,    31,    30,    31,    30,    31 };

// returns days since Oct 24 1996
int build_number( void )
{
	int m = 0; 
	int d = 0;
	int y = 0;
	static int b = 0;

	if (b != 0)
		return b;

	for (m = 0; m < 11; m++)
	{
		if (Q_strncasecmp( &date[0], mon[m], 3 ) == 0)
			break;
		d += mond[m];
	}

	d += atoi( &date[4] ) - 1;

	y = atoi( &date[7] ) - 1900;

	b = d + (int)((y - 1) * 365.25);

	if (((y % 4) == 0) && m > 1)
	{
		b += 1;
	}

	b -= 35778; // Dec 16 1998

	return b;
}

#ifdef __DJGPP__
int vsnprintf(char *str, size_t n, const char *fmt, va_list ap)
{
  FILE _strbuf;
  int len;

  /* _cnt is an int in the FILE structure. To prevent wrap-around, we limit
   * n to between 0 and INT_MAX inclusively. */
  if (n > INT_MAX)
  {
    errno = EFBIG;
    return -1;
  }

  memset(&_strbuf, 0, sizeof(_strbuf));
  _strbuf._flag = _IOWRT | _IOSTRG | _IONTERM;  

  /* If n == 0, just querying how much space is needed. */
  if (n > 0)
  {
    _strbuf._cnt = n - 1;
    _strbuf._ptr = str;
  }
  else
  {
    _strbuf._cnt = 0;
    _strbuf._ptr = NULL;
  }

  len = _doprnt(fmt, ap, &_strbuf);

  /* Ensure nul termination */
  if (n > 0)
    *_strbuf._ptr = 0;

  return len;
}
#endif

#if defined(__DJGPP__) || defined(_WIN32)
char *strtok_r(char *s, const char *delim, char **last) /* from OpenBSD */
{
	const char *spanp;
	int c, sc;
	char *tok;

	if (s == NULL && (s = *last) == NULL)
		return (NULL);

	/*
	 * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
	 */
cont:
	c = *s++;
	for (spanp = delim; (sc = *spanp++) != 0;) {
		if (c == sc)
			goto cont;
	}

	if (c == 0) {		/* no non-delimiter characters */
		*last = NULL;
		return (NULL);
	}
	tok = s - 1;

	/*
	 * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
	 * Note that delim must have one NUL; we stop if we see that, too.
	 */
	for (;;) {
		c = *s++;
		spanp = delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = '\0';
				*last = s;
				return (tok);
			}
		} while (sc != 0);
	}
	/* NOTREACHED */
}
#endif

/* FS: Buffer safe sprintf so we aren't va'ing all over the place */
void Com_sprintf (char *dest, int size, char *fmt, ...)
{
	int		len;
	va_list		argptr;

	va_start (argptr,fmt);
	len = Q_vsnprintf (dest, size, fmt, argptr);
	va_end (argptr);
	if (size > 0) dest[size - 1] = 0;
	if (len < 0 || len >= size) {
		Com_Printf ("Com_sprintf: overflow of %i in %i\n", len, size);
	}
}

void COM_Error_f (void)
{
	if (sv_allow_errorcmd && sv_allow_errorcmd->intValue) /* FS: Disabled by default */
		Sys_Error("%s", Cmd_Argv(1));
}
