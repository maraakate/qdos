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
// console.c

#include <time.h>
#include "quakedef.h"

int			afk; /* EZQ Chat */
int			con_ormask;
console_t	con_main;
console_t	con_chat;
console_t	*con;			// point to either con_main or con_chat

int 		con_linewidth;	// characters across screen
int			con_totallines;		// total lines in console scrollback

float		con_cursorspeed = 4;


cvar_t		*con_notifytime;
cvar_t		*con_logcenterprint; //johnfitz
cvar_t		*timestamp; /* FS: Timestamp logs */

char		con_lastcenterstring[1024]; //johnfitz

#define	NUM_CON_TIMES 4
float		con_times[NUM_CON_TIMES];	// realtime time the line was generated
								// for transparent notify lines

int			con_vislines;
int			con_notifylines;		// scan lines to clear for notify lines

qboolean	con_debuglog;

#define		MAXCMDLINE	256
#define		CONWIDTH_AT_640X480	78 /* FS: Added */

extern	char	key_lines[32][MAXCMDLINE];
extern	int		edit_line;
extern	int		key_linepos;

qboolean	con_initialized;


void Key_ClearTyping (void)
{
	key_lines[edit_line][1] = 0;	// clear any typing
	key_linepos = 1;
}

/*
================
Con_Quakebar -- johnfitz -- returns a bar of the desired length, but never wider than the console

includes a newline, unless len >= con_linewidth.
================
*/
char *Con_Quakebar (int len)
{
	static char bar[42];
	int i;

	len = MIN(len, sizeof(bar) - 2);
	len = MIN(len, con_linewidth);

	bar[0] = '\35';
	for (i = 1; i < len - 1; i++)
		bar[i] = '\36';
	bar[len-1] = '\37';

	if (len < con_linewidth)
	{
		bar[len] = '\n';
		bar[len+1] = 0;
	}
	else
		bar[len] = 0;

	return bar;
}

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void)
{
	Key_ClearTyping ();

	if (key_dest == key_console)
	{
		Cmd_ChatInfo(EZQ_CHAT_OFF); /* FS: EZQ Chat */

		if (cls.state == ca_active)
		{
			key_dest = key_game;
		}
	}
	else
	{
		Cmd_ChatInfo(EZQ_CHAT_AFK); /* FS: EZQ Chat */
		key_dest = key_console;
	}
	Con_ClearNotify ();

#ifdef QUAKE1
	SCR_EndLoadingPlaque ();
#endif
}

/*
================
Con_ToggleChat_f
================
*/
void Con_ToggleChat_f (void)
{
	Key_ClearTyping ();

	if (key_dest == key_console)
	{
		if (cls.state == ca_active)
			key_dest = key_game;
	}
	else
		key_dest = key_console;
	
	Con_ClearNotify ();
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	Q_memset (con_main.text, ' ', CON_TEXTSIZE);
	Q_memset (con_chat.text, ' ', CON_TEXTSIZE);
}

/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	int		i;
	
	for (i=0 ; i<NUM_CON_TIMES ; i++)
		con_times[i] = 0;
}

/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f (void)
{
	Cmd_ChatInfo(EZQ_CHAT_TYPING); /* FS: EZQ Chat */
	chat_team = false;
	key_dest = key_message;
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	Cmd_ChatInfo(EZQ_CHAT_TYPING); /* FS: EZQ Chat */
	chat_team = true;
	key_dest = key_message;
}

/*
================
Con_Resize

================
*/
void Con_Resize (console_t *con)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char	tbuf[CON_TEXTSIZE];

	width = (vid.width >> 3) - 2;

	if (width == con_linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = 38;
		con_linewidth = width;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		Q_memset (con->text, ' ', CON_TEXTSIZE);
	}
	else
	{
		oldwidth = con_linewidth;
		con_linewidth = width;
		oldtotallines = con_totallines;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		numlines = oldtotallines;

		if (con_totallines < numlines)
			numlines = con_totallines;

		numchars = oldwidth;
	
		if (con_linewidth < numchars)
			numchars = con_linewidth;

		Q_memcpy (tbuf, con->text, CON_TEXTSIZE);
		Q_memset (con->text, ' ', CON_TEXTSIZE);

		for (i=0 ; i<numlines ; i++)
		{
			for (j=0 ; j<numchars ; j++)
			{
				con->text[(con_totallines - 1 - i) * con_linewidth + j] =
						tbuf[((con->current - i + oldtotallines) %
							  oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con->current = con_totallines - 1;
	con->display = con->current;
}

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	Con_Resize (&con_main);
	Con_Resize (&con_chat);
}

void Con_afk_f (void) /* FS: EZQ Chat */
{
	if (afk < 2)
	{
		Cmd_ChatInfo(EZQ_CHAT_AFK);
		Con_Printf("AFK Mode ON\n");
		afk = EZQ_CHAT_AFK;
	}
	else
	{
		afk = EZQ_CHAT_OFF;
		Cmd_ChatInfo(afk);
		Con_Printf("AFK Mode OFF\n");
	}
}

void Con_MemClear_f (void) /* FS: Force clear state */
{
	CL_ClearState();
}

/*
================
Con_Init
================
*/
void Con_Init (void)
{
	con_debuglog = true;//COM_CheckParm("-condebug");

	con = &con_main;
	con_linewidth = -1;
	Con_CheckResize ();
	
	Con_Printf ("Console initialized.\n");

//
// register our commands
//
	con_notifytime = Cvar_Get("con_notifytime", "3", 0); //seconds
	con_notifytime->description = "Time (in seconds) a console notification message is displayed.";
	con_logcenterprint = Cvar_Get("con_logcenterprint", "1", 0);  //johnfitz
	con_logcenterprint->description = "Log centerprints to console.";
	timestamp = Cvar_Get("timestamp", "0", 0); /* FS: Timestamp logs */

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("togglechat", Con_ToggleChat_f);
	Cmd_AddCommand ("afk", Con_afk_f); /* FS: EZQ Chat */
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
	Cmd_AddCommand ("memclear", Con_MemClear_f); /* FS: Force clear state */

	con_initialized = true;
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (void)
{
	con->x = 0;
	if (con->display == con->current)
		con->display++;
	con->current++;
	Q_memset (&con->text[(con->current%con_totallines)*con_linewidth]
	, ' ', con_linewidth);
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the notify window will pop up.
================
*/
void Con_Print (char *txt)
{
	int		y;
	int		c, l;
	static int	cr;
	int		mask;

	if (txt[0] == 1 || txt[0] == 2)
	{
#ifdef QUAKE1
		if(txt[0] == 1)
			S_LocalSound ("misc/talk.wav");
#endif
		mask = 128;		// go to colored text
		txt++;
	}
	else
		mask = 0;


	while ( (c = *txt) )
	{
	// count word length
		for (l=0 ; l< con_linewidth ; l++)
			if ( txt[l] <= ' ')
				break;

	// word wrap
		if (l != con_linewidth && (con->x + l > con_linewidth) )
			con->x = 0;

		txt++;

		if (cr)
		{
			con->current--;
			cr = false;
		}

		
		if (!con->x)
		{
			Con_Linefeed ();
		// mark time for transparent overlay
			if (con->current >= 0)
				con_times[con->current % NUM_CON_TIMES] = realtime;
		}

		switch (c)
		{
		case '\n':
			con->x = 0;
			break;

		case '\r':
			con->x = 0;
			cr = 1;
			break;

		default:	// display character and advance
			y = con->current % con_totallines;
			con->text[y*con_linewidth+con->x] = c | mask | con_ormask;
			con->x++;
			if (con->x >= con_linewidth)
				con->x = 0;
			break;
		}
		
	}
}


/*
================
Con_Printf

Handles cursor positioning, line wrapping, etc
================
*/
void Con_Printf (const char *fmt, ...)
{
	va_list argptr;
	static dstring_t *msg;
#if 0
	static qboolean	inupdate;
#endif

	if (!msg)
		msg = dstring_new ();

	va_start (argptr,fmt);
	dvsprintf (msg,fmt,argptr);
	va_end (argptr);

	/* FS: Timestamp code */
	if(timestamp && (timestamp->intValue > 0)) /* FS: A Con_Printf might sneak in before this is init'd */
	{
		struct tm *local;
		time_t utc;
		const char *timefmt;
		char st[80];

		utc = time (NULL);
		local = localtime (&utc);
#ifdef _MSC_VER
		if (timestamp->intValue == 1)
			timefmt = "[%m/%d/%y @ %H:%M:%S %p] ";
		else	timefmt = "[%m/%d/%y @ %I:%M:%S %p] ";
#else
		if (timestamp->intValue == 1)
			timefmt = "[%m/%d/%y @ %k:%M:%S %p] ";
		else	timefmt = "[%m/%d/%y @ %l:%M:%S %p] ";
#endif
		strftime (st, sizeof (st), timefmt, local);
		Sys_Printf("%s", st);

		if(con_initialized)
			Con_Print(st);
	}

	// also echo to debugging console
	Sys_Printf("%s",msg->str); // also echo to debugging console

	// log all messages to file
	if (con_debuglog)
		Sys_DebugLog(va("%s/qconsole.log",com_gamedir), "%s", msg->str);

	if (!con_initialized)
		return;

#ifdef QUAKE1
	if (cls.state == ca_dedicated)
		return;		// no graphics mode
#endif

	// write it to the scrollable buffer
	Con_Print (msg->str);

	// update the screen immediately if the console is displayed
#if 0 /* FS: This makes scrolling painfully slow, and Quake 2 doesn't even use something like this */
	if (cls.state != ca_active)
	{
		// protect against infinite loop if something in SCR_UpdateScreen calls Con_Printf
		if (!inupdate)
		{
			inupdate = true;
			SCR_UpdateScreen ();
			inupdate = false;
		}
	}
#endif
}

/*
================
Con_Warning -- johnfitz -- prints a warning to the console
================
*/
void Con_Warning (const char *fmt, ...)
{
	va_list argptr;
	static dstring_t *msg;

	if (!msg)
		msg = dstring_new();

	va_start (argptr,fmt);
	dvsprintf (msg,fmt,argptr);
	va_end (argptr);

	Con_SafePrintf ("\x02Warning: ");
	Con_Printf ("%s", msg->str);
}

/*
================
Con_DPrintf

A Con_Printf that only shows up if the "developer" cvar is set
================
*/
void Con_DPrintf (unsigned long developerFlags, const char *fmt, ...)
{
	va_list argptr;
	static dstring_t *msg;
	unsigned long devValue = 0; /* FS: Developer Flags */

	if (!developer->value)
		return;			// don't confuse non-developers with techie stuff...

	if (!msg)
		msg = dstring_new();

	devValue = (unsigned long)developer->value;
	
	if (developer->value == 1)
		devValue = 65534;

	if (!(devValue & developerFlags))
		return;

	va_start (argptr,fmt);
	dvsprintf (msg,fmt,argptr);
	va_end (argptr);
	
	Con_Printf ("%s", msg->str);
}

/*
================
Con_CenterPrintf -- johnfitz -- pad each line with spaces to make it appear centered
================
*/
#define MAXPRINTMSG	8192
void Con_CenterPrintf (int linewidth, char *fmt, ...)
{
	va_list	argptr;
	static dstring_t *msg; //the original message
	char line[MAXPRINTMSG]; //one line from the message
	char spaces[21]; //buffer for spaces
	char *src, *dst;
	int	 len, s;

	if (!msg)
		msg = dstring_new();

	va_start (argptr,fmt);
	dvsprintf (msg,fmt,argptr);
	va_end (argptr);

	linewidth = MIN(linewidth, con_linewidth);
	for (src = msg->str; *src; )
	{
		dst = line;
		while (*src && *src != '\n')
			*dst++ = *src++;
		*dst = 0;
		if (*src == '\n')
			src++;

		len = strlen(line);
		if (len < linewidth)
		{
			s = (linewidth-len)/2;
			memset (spaces, ' ', s);
			spaces[s] = 0;
			Con_Printf ("%s%s\n", spaces, line);
		}
		else
			Con_Printf ("%s\n", line);
	}
}

/*
==================
Con_LogCenterPrint -- johnfitz -- echo centerprint message to the console
==================
*/
void Con_LogCenterPrint (char *str)
{
	if (!strcmp(str, con_lastcenterstring))
		return; //ignore duplicates

#ifdef QUAKE1
	if (cl.gametype == GAME_DEATHMATCH && con_logcenterprint->value != 2)
		return; //don't log in deathmatch
#endif

	strcpy(con_lastcenterstring, str);

	if (con_logcenterprint->value)
	{
		Con_Printf (Con_Quakebar(40));
		Con_CenterPrintf (40, "%s\n", str);
		Con_Printf (Con_Quakebar(40));
		Con_ClearNotify ();
	}
}
/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
void Con_DrawInput (void)
{
//	int		y;
	int		i;
	char	*text;

	if (key_dest != key_console && cls.state == ca_active)
		return;		// don't draw anything (allways draw if not active)

	text = key_lines[edit_line];
	
// add the cursor frame
	text[key_linepos] = 10+((int)(realtime*con_cursorspeed)&1);
	
// fill out remainder with spaces
	for (i=key_linepos+1 ; i< con_linewidth ; i++)
		text[i] = ' ';
		
//	prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;
		
// draw it
//	y = con_vislines-22;

	for (i=0 ; i<con_linewidth ; i++)
		Draw_Character ( (i+1)<<3, con_vislines - 22, text[i]);

// remove cursor
	key_lines[edit_line][key_linepos] = 0;
}


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	int		x, v;
	char	*text;
	int		i;
	float	time;
	char	*s;
	int		skip;

	v = 0;
	for (i= con->current-NUM_CON_TIMES+1 ; i<=con->current ; i++)
	{
		if (i < 0)
			continue;
		time = con_times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = realtime - time;
		if (time > con_notifytime->value)
			continue;
		text = con->text + (i % con_totallines)*con_linewidth;
		
		clearnotify = 0;
		scr_copytop = 1;

		for (x = 0 ; x < con_linewidth ; x++)
			Draw_Character ( (x+1)<<3, v, text[x]);

		v += 8;
	}


	if (key_dest == key_message)
	{
		clearnotify = 0;
		scr_copytop = 1;
	
		if (chat_team)
		{
			Draw_String (8, v, "say_team:");
			skip = 11;
		}
		else
		{
			Draw_String (8, v, "say:");
			skip = 5;
		}

		s = chat_buffer;
		if (chat_bufferlen > (vid.width>>3)-(skip+1))
			s += chat_bufferlen - ((vid.width>>3)-(skip+1));
		x = 0;
		while(s[x])
		{
			Draw_Character ( (x+skip)<<3, v, s[x]);
			x++;
		}
		Draw_Character ( (x+skip)<<3, v, 10+((int)(realtime*con_cursorspeed)&1));
		v += 8;
	}
	
	if (v > con_notifylines)
		con_notifylines = v;
}

/*
================
Con_DrawConsole

Draws the console with the solid background
================
*/
void Con_DrawConsole (int lines)
{
	int				i, x, y;
	int				rows;
	char			*text;
	int				row;
#if defined(QUAKEWORLD) || defined(GAMESPY)
	int				j, n;
	char			dlbar[1024];
#endif

	if (lines <= 0)
		return;

// draw the background
	Draw_ConsoleBackground (lines);

// draw the text
	con_vislines = lines;
	
// changed to line things up better
	rows = (lines-22)>>3;		// rows of text to draw

	y = lines - 30;

// draw from the bottom up
	if (con->display != con->current)
	{
	// draw arrows to show the buffer is backscrolled
		for (x=0 ; x<con_linewidth ; x+=4)
			Draw_Character ( (x+1)<<3, y, '^');
	
		y -= 8;
		rows--;
	}
	
	row = con->display;
	for (i=0 ; i<rows ; i++, y-=8, row--)
	{
		if (row < 0)
			break;
		if (con->current - row >= con_totallines)
			break;		// past scrollback wrap point
			
		text = con->text + (row % con_totallines)*con_linewidth;

		for (x=0 ; x<con_linewidth ; x++)
			Draw_Character ( (x+1)<<3, y, text[x]);
	}

#ifdef QUAKEWORLD
	// draw the download bar
	// figure out width
	if (cls.download)
	{
		if ((text = strrchr(cls.downloadname->str, '/')) != NULL)
				text++;
		else
			text = cls.downloadname->str;

		x = con_linewidth - ((con_linewidth * 7) / 40);

		if (cls.downloadrate > 0.0f)
		{
			if(con_linewidth > CONWIDTH_AT_640X480)
				y = x - strlen(text) - 25;
			else
			{
				y = x - strlen(text) - 19;
			}
		}
		else
		{
			if(con_linewidth > CONWIDTH_AT_640X480)
				y = x - strlen(text) - 8;
			else
			{
				y = x - strlen(text) - 2;
			}
		}

		i = con_linewidth/3;
		if (strlen(text) > i)
		{
			y = x - i - 11;
			strncpy(dlbar, text, i);
			dlbar[i] = 0;
			strcat(dlbar, "...");
		}
		else
			strcpy(dlbar, text);
		strcat(dlbar, ": ");
		i = strlen(dlbar);
		dlbar[i++] = '\x80';
		// where's the dot go?
		if (cls.downloadpercent == 0)
			n = 0;
		else
			n = y * cls.downloadpercent / 100;
			
		for (j = 0; j < y; j++)
			if (j == n)
				dlbar[i++] = '\x83';
			else
				dlbar[i++] = '\x81';
		dlbar[i++] = '\x82';
		dlbar[i] = 0;

//		sprintf(dlbar + strlen(dlbar), " %02d%%", cls.downloadpercent);
		if (cls.downloadrate > 0.0f)
			Com_sprintf(dlbar + strlen(dlbar), sizeof(dlbar)-strlen(dlbar), " %2d%% (%4.2fKB/s)", cls.downloadpercent, cls.downloadrate);
		else
			Com_sprintf(dlbar + strlen(dlbar), sizeof(dlbar)-strlen(dlbar), " %2d%%", cls.downloadpercent);

		// draw it
		y = con_vislines-22 + 8;
		for (i = 0; i < strlen(dlbar); i++)
			Draw_Character ( (i+1)<<3, y, dlbar[i]);
	}
#endif

#ifdef GAMESPY /* FS: Gamespy stuff */
	if (cls.gamespyupdate)
	{
		text = "Gamespy";

		x = con_linewidth - ((con_linewidth * 7) / 40);
		y = x - strlen(text) - 8;
		i = con_linewidth/3;
		if (strlen(text) > i)
		{
			y = x - i - 11;
			strncpy(dlbar, text, i);
			dlbar[i] = 0;
			strcat(dlbar, "...");
		}
		else
			strcpy(dlbar, text);
		strcat(dlbar, ": ");

		i = strlen(dlbar);
		dlbar[i++] = '\x80';
		// where's the dot go?
		if (cls.gamespypercent == 0)
			n = 0;
		else
			n = y * cls.gamespypercent / 100;

		for (j = 0; j < y; j++)
			if (j == n)
				dlbar[i++] = '\x83';
			else
				dlbar[i++] = '\x81';
		dlbar[i++] = '\x82';
		dlbar[i] = 0;

		sprintf(dlbar + strlen(dlbar), " %02d%%", cls.gamespypercent);

		// draw it
		y = con_vislines-22 + 8;
		for (i = 0; i < strlen(dlbar); i++)
			Draw_Character ( (i+1)<<3, y, dlbar[i]);
	}
#endif

// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();
}

/*
==================
Con_NotifyBox
==================
*/
void Con_NotifyBox (char *text)
{
	double		t1, t2;

// during startup for sound / cd warnings
	Con_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

	Con_Printf (text);

	Con_Printf ("Press a key.\n");
	Con_Printf("\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

	key_count = -2;		// wait for a key down and up
	key_dest = key_console;

	do
	{
		t1 = Sys_DoubleTime();
		SCR_UpdateScreen ();
		Sys_SendKeyEvents ();
		t2 = Sys_DoubleTime();
		realtime += t2-t1;		// make the cursor blink
	} while (key_count < 0);

	Con_Printf ("\n");
	key_dest = key_game;
	realtime = 0;				// put the cursor back to invisible
}


/*
==================
Con_SafePrintf

Okay to call even when the screen can't be updated
==================
*/
void Con_SafePrintf (const char *fmt, ...)
{
	va_list				argptr;
	int					temp;
	static dstring_t	*msg;

	if(!msg)
		msg = dstring_new();

	va_start (argptr,fmt);
	dvsprintf (msg,fmt,argptr);
	va_end (argptr);

	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	Con_Printf ("%s", msg->str);

	scr_disabled_for_loading = temp;
}

/*
==================
Con_SafeDPrintf

Okay to call even when the screen can't be updated
==================
*/
void Con_SafeDPrintf (unsigned long developerFlags, const char *fmt, ...)
{
	va_list				argptr;
	int					temp;
	static dstring_t	*msg;

	if(!msg)
		msg = dstring_new();

	va_start (argptr,fmt);
	dvsprintf (msg,fmt,argptr);
	va_end (argptr);

	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	Con_DPrintf (developerFlags, "%s", msg->str);

	scr_disabled_for_loading = temp;
}
