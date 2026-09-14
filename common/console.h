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

#ifndef __CONSOLE_H
#define __CONSOLE_H

//
// console
//

#define         CON_TEXTSIZE    65536 /* FS: Was 16384 */

/* FS: Developer flags for developer cvar and DPrintf's */
/* FS: No 0x00000001 because that would be developer->intValue 1 and we use that to show it all! */
#define DEVELOPER_MSG_STANDARD		0x00000002 // 2
#define DEVELOPER_MSG_SOUND			0x00000004 // 4
#define DEVELOPER_MSG_NET			0x00000008 // 8
#define DEVELOPER_MSG_IO			0x00000010 // 16
#define DEVELOPER_MSG_VIDEO			0x00000020 // 32
#define DEVELOPER_MSG_CD			0x00000040 // 64
#define DEVELOPER_MSG_MEM			0x00000080 // 128
#ifdef QUAKE1
#define DEVELOPER_MSG_SERVER		0x00000100 // 256
#define DEVELOPER_MSG_PROGS			0x00000200 // 512
#define DEVELOPER_MSG_UNUSED1		0x00000400 // 1024
#endif
#define DEVELOPER_MSG_PHYSICS		0x00000800 // 2048
#define DEVELOPER_MSG_UNUSED2		0x00001000 // 4096
#define DEVELOPER_MSG_UNUSED3		0x00002000 // 8192
#define DEVELOPER_MSG_ENTITY		0x00004000 // 16384
#ifdef QUAKE1
#define DEVELOPER_MSG_SAVE			0x00008000 // 32768
#endif
#define DEVELOPER_MSG_VERBOSE		0x00010000 // 65536
#define DEVELOPER_MSG_GAMESPY		0x00020000 // 131072

typedef struct
{
	char	text[CON_TEXTSIZE];
	int		current;		// line where next message will be printed
	int		x;				// offset in current line for next print
	int		display;		// bottom of console displays this line
	int		firstline; /* FS */
	int		totalrows; /* FS */
} console_t;

extern	console_t	con_main;
extern	console_t	con_chat;
extern	console_t	*con;			// point to either con_main or con_chat

extern	int			con_ormask;

/* FS: EZQ Chat */
#define EZQ_CHAT_OFF 0
#define EZQ_CHAT_TYPING 1
#define EZQ_CHAT_AFK 2
#define EZQ_CHAT_AFK_TYPING 3
extern  int			afk;

extern int con_totallines;
extern qboolean con_initialized;
extern byte *con_chars;
extern	int	con_notifylines;		// scan lines to clear for notify lines
extern char con_lastcenterstring[]; //johnfitz

void Con_DrawCharacter (int cx, int line, int num);

void Con_CheckResize (void);
void Con_Init (void);
void Con_DrawConsole (int lines);
void Con_Print (char *txt);
void Com_Printf (const char *fmt, ...);
void Con_Warning (const char *fmt, ...); //johnfitz
void Com_DPrintf (unsigned long developerFlags, const char *fmt, ...);
void Com_SafePrintf (const char *fmt, ...);
void Con_SafeDPrintf (unsigned long developerFlags, const char *fmt, ...);
void Con_Clear_f (void);
void Con_DrawNotify (void);
void Con_ClearNotify (void);
void Con_ToggleConsole_f (void);

void Con_NotifyBox (char *text); // during startup for sound / cd warnings

char *Con_Quakebar (int len); /* FS: From fitzquake */
void Con_LogCenterPrint (char *str);  /* FS: from fitzquake */

#endif // __CONSOLE_H
