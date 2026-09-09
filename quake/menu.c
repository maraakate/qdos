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
#include "quakedef.h"

void (*vid_menudrawfn)(void);
void (*vid_menukeyfn)(int key);

enum {m_none, m_main, m_singleplayer, m_load, m_save, m_multiplayer, m_setup, m_net, m_options, m_video, m_keys, m_help, m_quit, m_serialconfig, m_modemconfig, m_lanconfig, m_gameoptions, m_search, m_slist,
      m_extended,
      m_demo,
#if 0/* not yet */
      m_credits,
#endif
#ifdef GAMESPY
      m_gamespy, m_gamespy_pages
#endif
} m_state;

void M_Menu_Main_f (void);
	void M_Menu_SinglePlayer_f (void);
		void M_Menu_Load_f (void);
		void M_Menu_Save_f (void);
	void M_Menu_MultiPlayer_f (void);
		void M_Menu_Setup_f (void);
		void M_Menu_Net_f (void);
	void M_Menu_Options_f (void);
		void M_Menu_Keys_f (void);
		void M_Menu_Video_f (void);
	void M_Menu_Help_f (void);
	void M_Menu_Quit_f (void);
void M_Menu_SerialConfig_f (void);
	void M_Menu_ModemConfig_f (void);
void M_Menu_LanConfig_f (void);
void M_Menu_GameOptions_f (void);
void M_Menu_Search_f (void);
void M_Menu_ServerList_f (void);
void M_Menu_Extended_f (void); /* FS: Extended options unique to QDOS */

void M_Main_Draw (void);
	void M_SinglePlayer_Draw (void);
		void M_Load_Draw (void);
		void M_Save_Draw (void);
	void M_MultiPlayer_Draw (void);
		void M_Setup_Draw (void);
		void M_Net_Draw (void);
	void M_Options_Draw (void);
		void M_Keys_Draw (void);
		void M_Video_Draw (void);
	void M_Help_Draw (void);
	void M_Quit_Draw (void);
void M_SerialConfig_Draw (void);
	void M_ModemConfig_Draw (void);
void M_LanConfig_Draw (void);
void M_GameOptions_Draw (void);
void M_Search_Draw (void);
void M_ServerList_Draw (void);
void M_Extended_Draw (void); /* FS: Extended options unique to QDOS */

void M_Main_Key (int key);
	void M_SinglePlayer_Key (int key);
		void M_Load_Key (int key);
		void M_Save_Key (int key);
	void M_MultiPlayer_Key (int key);
		void M_Setup_Key (int key);
		void M_Net_Key (int key);
	void M_Options_Key (int key);
		void M_Keys_Key (int key);
		void M_Video_Key (int key);
	void M_Help_Key (int key);
	void M_Quit_Key (int key);
void M_SerialConfig_Key (int key);
	void M_ModemConfig_Key (int key);
void M_LanConfig_Key (int key);
void M_GameOptions_Key (int key);
void M_Search_Key (int key);
void M_ServerList_Key (int key);
void M_Extended_Key (int key); /* FS: Extended options unique to QDOS */

#if 0 /* not yet */
void M_Menu_Credits_f (void);
void M_Credits_Draw (void);
void M_Credits_Key (int key);
#endif

#ifdef GAMESPY
/* FS: Gamespy stuff */
void M_Menu_Gamespy_f (void);
void M_Gamespy_Draw (void);
void M_Gamespy_Key (int key);
static void SearchGamespyGames (void);
static void JoinGamespyServer_Redraw(int scale);
static int serverscale;
#endif

qboolean	m_entersound;		// play after drawing a frame, so caching
								// won't disrupt the sound
qboolean	m_recursiveDraw;

int			m_return_state;
qboolean	m_return_onerror;
char		m_return_reason [32];

extern cvar_t *scr_fov; /* FS */

#define StartingGame	(m_multiplayer_cursor == 1)
#define JoiningGame		(m_multiplayer_cursor == 0)
#define SerialConfig	(m_net_cursor == 0)

/* FS: Nehahra Stuff */
#define NumberOfNehahraDemos 34
typedef struct nehahrademonames_s
{
	char *name;
	char *desc;
} nehahrademonames_t;

static nehahrademonames_t NehahraDemos[NumberOfNehahraDemos] =
{
	{"intro", "Prologue"},
	{"genf", "The Beginning"},
	{"genlab", "A Doomed Project"},
	{"nehcre", "The New Recruits"},
	{"maxneh", "Breakthrough"},
	{"maxchar", "Renewal and Duty"},
	{"crisis", "Worlds Collide"},
	{"postcris", "Darkening Skies"},
	{"hearing", "The Hearing"},
	{"getjack", "On a Mexican Radio"},
	{"prelude", "Honor and Justice"},
	{"abase", "A Message Sent"},
	{"effect", "The Other Side"},
	{"uhoh", "Missing in Action"},
	{"prepare", "The Response"},
	{"vision", "Farsighted Eyes"},
	{"maxturns", "Enter the Immortal"},
	{"backlot", "Separate Ways"},
	{"maxside", "The Ancient Runes"},
	{"counter", "The New Initiative"},
	{"warprep", "Ghosts to the World"},
	{"counter1", "A Fate Worse Than Death"},
	{"counter2", "Friendly Fire"},
	{"counter3", "Minor Setback"},
	{"madmax", "Scores to Settle"},
	{"quake", "One Man"},
	{"cthmm", "Shattered Masks"},
	{"shades", "Deal with the Dead"},
	{"gophil", "An Unlikely Hero"},
	{"cstrike", "War in Hell"},
	{"shubset", "The Conspiracy"},
	{"shubdie", "Even Death May Die"},
	{"newranks", "An Empty Throne"},
	{"seal", "The Seal is Broken"}
};
#define DirectConfig	(m_net_cursor == 1)
#define	IPXConfig		(m_net_cursor == 2)
#define	TCPIPConfig		(m_net_cursor == 3)

void M_ConfigureNetSubsystem(void);

//=============================================================================
/* Support Routines */

/*
================
M_DrawCharacter

Draws one solid graphics character
================
*/
void M_DrawCharacter (int cx, int line, int num)
{
	Draw_Character ( cx + ((vid.width - 320)>>1), line, num);
}

void M_Print (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawCharacter (cx, cy, (*str)+128);
		str++;
		cx += 8;
	}
}

void M_PrintWhite (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawCharacter (cx, cy, *str);
		str++;
		cx += 8;
	}
}

void M_DrawTransPic (int x, int y, qpic_t *pic)
{
	Draw_TransPic (x + ((vid.width - 320)>>1), y, pic);
}

void M_DrawPic (int x, int y, qpic_t *pic)
{
	Draw_Pic (x + ((vid.width - 320)>>1), y, pic);
}

byte identityTable[256];
byte translationTable[256];

void M_BuildTranslationTable(int top, int bottom)
{
	int	     j;
	byte    *dest, *source;

	for (j = 0; j < 256; j++)
		identityTable[j] = j;
	dest = translationTable;
	source = identityTable;
	memcpy (dest, source, 256);

	if (top < 128)  // the artists made some backwards ranges.  sigh.
		memcpy (dest + TOP_RANGE, source + top, 16);
	else
		for (j=0 ; j<16 ; j++)
			dest[TOP_RANGE+j] = source[top+15-j];

	if (bottom < 128)
		memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
	else
		for (j=0 ; j<16 ; j++)
			dest[BOTTOM_RANGE+j] = source[bottom+15-j];
}


void M_DrawTransPicTranslate (int x, int y, qpic_t *pic)
{
	Draw_TransPicTranslate (x + ((vid.width - 320)>>1), y, pic, translationTable);
}


void M_DrawTextBox (int x, int y, int width, int lines)
{
	qpic_t  *p;
	int	     cx, cy;
	int	     n;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_CachePic ("gfx/box_tl.lmp");
	M_DrawTransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawTransPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_bl.lmp");
	M_DrawTransPic (cx, cy+8, p);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		p = Draw_CachePic ("gfx/box_tm.lmp");
		M_DrawTransPic (cx, cy, p);
		p = Draw_CachePic ("gfx/box_mm.lmp");
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			if (n == 1)
				p = Draw_CachePic ("gfx/box_mm2.lmp");
			M_DrawTransPic (cx, cy, p);
		}
		p = Draw_CachePic ("gfx/box_bm.lmp");
		M_DrawTransPic (cx, cy+8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Draw_CachePic ("gfx/box_tr.lmp");
	M_DrawTransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_mr.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawTransPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_br.lmp");
	M_DrawTransPic (cx, cy+8, p);
}

//=============================================================================

int m_save_demonum;

/*
================
M_ToggleMenu_f
================
*/
void M_ToggleMenu_f (void)
{
	m_entersound = true;

	if (key_dest == key_menu)
	{
		if (m_state != m_main)
		{
			M_Menu_Main_f ();
			return;
		}

		key_dest = key_game;
		m_state = m_none;
		return;
	}
	if (key_dest == key_console)
	{
		Con_ToggleConsole_f ();
	}
	else
	{
		M_Menu_Main_f ();
	}
}

static int demo_cursor;
static void M_Demo_Draw (void)
{
	int i;

//	M_Background(320, 200);

	for (i = 0;i < NumberOfNehahraDemos;i++)
		M_Print(16, 16 + 8*i, NehahraDemos[i].desc);

	// line cursor
	M_DrawCharacter (8, 16 + demo_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Menu_Demos_f (void)
{
	key_dest = key_menu;
	m_state = m_demo;
	m_entersound = true;
}


static void M_Demo_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu2.wav");
		m_state = m_none;
		key_dest = key_game;
		Cbuf_AddText (va ("playdemo %s\n", NehahraDemos[demo_cursor].name));
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		demo_cursor--;
		if (demo_cursor < 0)
			demo_cursor = NumberOfNehahraDemos-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		demo_cursor++;
		if (demo_cursor >= NumberOfNehahraDemos)
			demo_cursor = 0;
		break;
	}
}

//=============================================================================
/* MAIN MENU */

int	m_main_cursor;
static int	MAIN_ITEMS = 5;


void M_Menu_Main_f (void)
{
	if (nehahra)
	{
		MAIN_ITEMS = 6;
	}
	
	if (key_dest != key_menu)
	{
		m_save_demonum = cls.demonum;
		cls.demonum = -1;
	}

	key_dest = key_menu;
	m_state = m_main;
	m_entersound = true;
}

void M_Main_Draw (void)
{
	int	     f;
	qpic_t  *p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_main.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	M_DrawTransPic (72, 32, Draw_CachePic ("gfx/mainmenu.lmp") );

	f = (int)(host_time * 10)%6;

	M_DrawTransPic (54, 32 + m_main_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );
}


void M_Main_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		key_dest = key_game;
		m_state = m_none;
		if (!cl_demos->intValue || nostartupdemos) /* FS: Maybe you disabled startup demos via menu? */
		{
			cls.demonum = -1;
			break;
		}
		cls.demonum = m_save_demonum;
		if (cls.demonum != -1 && !cls.demoplayback && cls.state != ca_connected)
			CL_NextDemo ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		break;

/* FS: Menu shortcuts */
	case 's':
		M_Menu_SinglePlayer_f ();
		break;
	case 'm':
		M_Menu_MultiPlayer_f ();
		break;
	case 'o':
		M_Menu_Options_f ();
		break;
	case 'h':
		M_Menu_Help_f ();
		break;
	case 'q':
		M_Menu_Quit_f ();
		break;

	case K_ENTER:
		m_entersound = true;

		if(nehahra)
		{
			switch(m_main_cursor)
			{
			case 0:
				if (!COM_FileExists("maps/neh1m3.bsp"))
					break;

				M_Menu_SinglePlayer_f ();
				break;

			case 1:
				if (!COM_FileExists("hearing.dem"))
					break;

				M_Menu_Demos_f ();
				break;

			case 2:
				M_Menu_MultiPlayer_f ();
				break;

			case 3:
				M_Menu_Options_f ();
				break;

			case 4:
				if (!COM_FileExists("endcred.dem"))
					break;

				key_dest = key_game;
				if (sv.active)
					Cbuf_AddText ("disconnect\n");
				Cbuf_AddText ("playdemo endcred\n");
				break;

			case 5:
				M_Menu_Quit_f ();
				break;
			}
		}
		else
		{
			switch (m_main_cursor)
			{
			case 0:
				M_Menu_SinglePlayer_f ();
				break;

			case 1:
				M_Menu_MultiPlayer_f ();
				break;

			case 2:
				M_Menu_Options_f ();
				break;

			case 3:
				M_Menu_Help_f ();
				break;

			case 4:
				M_Menu_Quit_f ();
				break;
			}
		}
	}
}

//=============================================================================
/* SINGLE PLAYER MENU */

int	m_singleplayer_cursor;
#define	SINGLEPLAYER_ITEMS	3


void M_Menu_SinglePlayer_f (void)
{
	key_dest = key_menu;
	m_state = m_singleplayer;
	m_entersound = true;
}


void M_SinglePlayer_Draw (void)
{
	int		f;
	qpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	M_DrawTransPic (72, 32, Draw_CachePic ("gfx/sp_menu.lmp") );

	f = (int)(host_time * 10)%6;

	M_DrawTransPic (54, 32 + m_singleplayer_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );
}


void M_SinglePlayer_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS)
			m_singleplayer_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_singleplayer_cursor < 0)
			m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
		break;

	case K_ENTER:
		m_entersound = true;

		switch (m_singleplayer_cursor)
		{
		case 0:
			if (sv.active)
				if (!SCR_ModalMessage("Are you sure you want to\nstart a new game?\n"))
					break;

			key_dest = key_game;

			if (sv.active)
				Cbuf_AddText ("disconnect\n");

			Cbuf_AddText ("maxplayers 1\n");

			if (nehahra) /* FS: Nehara stuff */
				Cbuf_AddText ("map nehstart\n");
			else
				Cbuf_AddText ("map start\n");

			break;
		case 1:
			M_Menu_Load_f ();
			break;
		case 2:
			M_Menu_Save_f ();
			break;
		}
	}
}

//=============================================================================
/* LOAD/SAVE MENU */

int		load_cursor;		// 0 < load_cursor < MAX_SAVEGAMES

#define	MAX_SAVEGAMES		13
#define AUTOSAVE_POS (MAX_SAVEGAMES-1)
char	m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH+1];
int		loadable[MAX_SAVEGAMES];

void M_ScanSaves (void)
{
	int		i, j;
	char	name[MAX_OSPATH];
	FILE	*f;
	int		version;

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
	{
		Q_strlcpy (m_filenames[i], "--- UNUSED SLOT ---", sizeof(m_filenames[i]));
		loadable[i] = false;
		if (i == AUTOSAVE_POS)
		{
			Com_sprintf (name, sizeof(name), "%s/auto.sav", com_gamedir, i);
		}
		else
		{
			Com_sprintf (name, sizeof(name), "%s/s%i.sav", com_gamedir, i);
		}
		f = fopen (name, "r");
		if (!f)
			continue;
		fscanf (f, "%i\n", &version);
		fscanf (f, "%79s\n", name);
		Q_strlcpy (m_filenames[i], name, sizeof(m_filenames[i]));

	// change _ back to space
		for (j=0 ; j<SAVEGAME_COMMENT_LENGTH ; j++)
			if (m_filenames[i][j] == '_')
				m_filenames[i][j] = ' ';
		loadable[i] = true;
		fclose (f);
	}
}

void M_Menu_Load_f (void)
{
	m_entersound = true;
	m_state = m_load;
	key_dest = key_menu;
	M_ScanSaves ();
}


void M_Menu_Save_f (void)
{
	if (!sv.active)
		return;
	if (cl.intermission)
		return;
	if (svs.maxclients != 1)
		return;
	m_entersound = true;
	m_state = m_save;
	key_dest = key_menu;
	M_ScanSaves ();
}


void M_Load_Draw (void)
{
	int		i;
	qpic_t	*p;

	p = Draw_CachePic ("gfx/p_load.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	for (i=0 ; i< MAX_SAVEGAMES; i++)
		M_Print (16, 32 + 8*i, m_filenames[i]);

// line cursor
	M_DrawCharacter (8, 32 + load_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Save_Draw (void)
{
	int		i;
	qpic_t	*p;

	p = Draw_CachePic ("gfx/p_save.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
		M_Print (16, 32 + 8*i, m_filenames[i]);

// line cursor
	M_DrawCharacter (8, 32 + load_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Load_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_SinglePlayer_f ();
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu2.wav");
		if (!loadable[load_cursor])
			return;
		m_state = m_none;
		key_dest = key_game;

	// Host_Loadgame_f can't bring up the loading plaque because too much
	// stack space has been used, so do it now
		SCR_BeginLoadingPlaque ();

	// issue the load command
		if (load_cursor == AUTOSAVE_POS)
		{
			Cbuf_AddText ("deathmatch 0; coop 0;load auto\n");
		}
		else
		{
			Cbuf_AddText (va ("deathmatch 0; coop 0;load s%i\n", load_cursor) );
		}
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;
	}
}


void M_Save_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_SinglePlayer_f ();
		break;

	case K_ENTER:
		m_state = m_none;
		key_dest = key_game;
		if (load_cursor == AUTOSAVE_POS)
		{
			Cbuf_AddText ("save auto\n");
		}
		else
		{
			Cbuf_AddText (va("save s%i\n", load_cursor));
		}
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;
	}
}

//=============================================================================
/* MULTIPLAYER MENU */

int	m_multiplayer_cursor;
#define	MULTIPLAYER_ITEMS	3


void M_Menu_MultiPlayer_f (void)
{
	key_dest = key_menu;
	m_state = m_multiplayer;
	m_entersound = true;
}


void M_MultiPlayer_Draw (void)
{
	int		f;
	qpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	M_DrawTransPic (72, 32, Draw_CachePic ("gfx/mp_menu.lmp") );

	f = (int)(host_time * 10)%6;

	M_DrawTransPic (54, 32 + m_multiplayer_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );

	if (serialAvailable || ipxAvailable || tcpipAvailable)
		return;
	M_PrintWhite ((320/2) - ((27*8)/2), 148, "No Communications Available");
}


void M_MultiPlayer_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS)
			m_multiplayer_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_multiplayer_cursor < 0)
			m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
		break;

	case K_ENTER:
		m_entersound = true;
		switch (m_multiplayer_cursor)
		{
		case 0:
			if (serialAvailable || ipxAvailable || tcpipAvailable)
				M_Menu_Net_f ();
			break;

		case 1:
			if (serialAvailable || ipxAvailable || tcpipAvailable)
				M_Menu_Net_f ();
			break;

		case 2:
			M_Menu_Setup_f ();
			break;
		}
	}
}

//=============================================================================
/* SETUP MENU */

int		setup_cursor = 4;
int		setup_cursor_table[] = {40, 56, 80, 104, 140};

char	setup_hostname[16];
char	setup_myname[16];
int		setup_oldtop;
int		setup_oldbottom;
int		setup_top;
int		setup_bottom;

#define	NUM_SETUP_CMDS	5

void M_Menu_Setup_f (void)
{
	key_dest = key_menu;
	m_state = m_setup;
	m_entersound = true;
	Q_strlcpy(setup_myname, cl_name->string, sizeof(setup_myname));
	Q_strlcpy(setup_hostname, hostname->string, sizeof(setup_hostname));
	setup_top = setup_oldtop = (cl_color->intValue) >> 4;
	setup_bottom = setup_oldbottom = (cl_color->intValue) & 15;
}


void M_Setup_Draw (void)
{
	qpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	M_Print (64, 40, "Hostname");
	M_DrawTextBox (160, 32, 16, 1);
	M_Print (168, 40, setup_hostname);

	M_Print (64, 56, "Your name");
	M_DrawTextBox (160, 48, 16, 1);
	M_Print (168, 56, setup_myname);

	M_Print (64, 80, "Shirt color");
	M_Print (64, 104, "Pants color");

	M_DrawTextBox (64, 140-8, 14, 1);
	M_Print (72, 140, "Accept Changes");

	p = Draw_CachePic ("gfx/bigbox.lmp");
	M_DrawTransPic (160, 64, p);
	p = Draw_CachePic ("gfx/menuplyr.lmp");
	M_BuildTranslationTable(setup_top*16, setup_bottom*16);
	M_DrawTransPicTranslate (172, 72, p);

	M_DrawCharacter (56, setup_cursor_table [setup_cursor], 12+((int)(realtime*4)&1));

	if (setup_cursor == 0)
		M_DrawCharacter (168 + 8*strlen(setup_hostname), setup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1));

	if (setup_cursor == 1)
		M_DrawCharacter (168 + 8*strlen(setup_myname), setup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1));
}


void M_Setup_Key (int k)
{
	int			l;

	switch (k)
	{
	case K_ESCAPE:
		M_Menu_MultiPlayer_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor--;
		if (setup_cursor < 0)
			setup_cursor = NUM_SETUP_CMDS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor++;
		if (setup_cursor >= NUM_SETUP_CMDS)
			setup_cursor = 0;
		break;

	case K_LEFTARROW:
		if (setup_cursor < 2)
			return;
		S_LocalSound ("misc/menu3.wav");
		if (setup_cursor == 2)
			setup_top = setup_top - 1;
		if (setup_cursor == 3)
			setup_bottom = setup_bottom - 1;
		break;
	case K_RIGHTARROW:
		if (setup_cursor < 2)
			return;
forward:
		S_LocalSound ("misc/menu3.wav");
		if (setup_cursor == 2)
			setup_top = setup_top + 1;
		if (setup_cursor == 3)
			setup_bottom = setup_bottom + 1;
		break;

	case K_ENTER:
		if (setup_cursor == 0 || setup_cursor == 1)
			return;

		if (setup_cursor == 2 || setup_cursor == 3)
			goto forward;

		// setup_cursor == 4 (OK)
		if (Q_strcmp(cl_name->string, setup_myname) != 0)
			Cbuf_AddText ( va ("name \"%s\"\n", setup_myname) );
		if (Q_strcmp(hostname->string, setup_hostname) != 0)
			Cvar_Set("hostname", setup_hostname);
		if (setup_top != setup_oldtop || setup_bottom != setup_oldbottom)
			Cbuf_AddText( va ("color %i %i\n", setup_top, setup_bottom) );
		m_entersound = true;
		M_Menu_MultiPlayer_f ();
		break;

	case K_BACKSPACE:
		if (setup_cursor == 0)
		{
			if (strlen(setup_hostname))
				setup_hostname[strlen(setup_hostname)-1] = 0;
		}

		if (setup_cursor == 1)
		{
			if (strlen(setup_myname))
				setup_myname[strlen(setup_myname)-1] = 0;
		}
		break;

	default:
		if (k < 32 || k > 127)
			break;
		if (setup_cursor == 0)
		{
			l = strlen(setup_hostname);
			if (l < 15)
			{
				setup_hostname[l+1] = 0;
				setup_hostname[l] = k;
			}
		}
		if (setup_cursor == 1)
		{
			l = strlen(setup_myname);
			if (l < 15)
			{
				setup_myname[l+1] = 0;
				setup_myname[l] = k;
			}
		}
	}

	if (setup_top > 13)
		setup_top = 0;
	if (setup_top < 0)
		setup_top = 13;
	if (setup_bottom > 13)
		setup_bottom = 0;
	if (setup_bottom < 0)
		setup_bottom = 13;
}

//=============================================================================
/* NET MENU */

int	m_net_cursor;
int m_net_items;
int m_net_saveHeight;

char *net_helpMessage [] =
{
/* .........1.........2.... */
  "                        ",
  " Two computers connected",
  "   through two modems.  ",
  "                        ",

  "                        ",
  " Two computers connected",
  " by a null-modem cable. ",
  "                        ",

  " Novell network LANs    ",
  " or Windows 95 DOS-box. ",
  "                        ",
  "(LAN=Local Area Network)",

  " Commonly used to play  ",
  " over the Internet, but ",
  " also used on a Local   ",
  " Area Network.          "
};

void M_Menu_Net_f (void)
{
	key_dest = key_menu;
	m_state = m_net;
	m_entersound = true;
	m_net_items = 4;

	if (m_net_cursor >= m_net_items)
		m_net_cursor = 0;
	m_net_cursor--;
	M_Net_Key (K_DOWNARROW);
}


void M_Net_Draw (void)
{
	int		f;
	qpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	f = 32;

	if (serialAvailable)
	{
		p = Draw_CachePic ("gfx/netmen1.lmp");
	}
	else
	{
#ifdef _WIN32aa
		p = NULL;
#else
		p = Draw_CachePic ("gfx/dim_modm.lmp");
#endif
	}

	if (p)
		M_DrawTransPic (72, f, p);

	f += 19;

	if (serialAvailable)
	{
		p = Draw_CachePic ("gfx/netmen2.lmp");
	}
	else
	{
#ifdef _WIN32aa
		p = NULL;
#else
		p = Draw_CachePic ("gfx/dim_drct.lmp");
#endif
	}

	if (p)
		M_DrawTransPic (72, f, p);

	f += 19;
	if (ipxAvailable)
		p = Draw_CachePic ("gfx/netmen3.lmp");
	else
		p = Draw_CachePic ("gfx/dim_ipx.lmp");
	M_DrawTransPic (72, f, p);

	f += 19;
	if (tcpipAvailable)
		p = Draw_CachePic ("gfx/netmen4.lmp");
	else
		p = Draw_CachePic ("gfx/dim_tcp.lmp");
	M_DrawTransPic (72, f, p);

	if (m_net_items == 5)	// JDC, could just be removed
	{
		f += 19;
		p = Draw_CachePic ("gfx/netmen5.lmp");
		M_DrawTransPic (72, f, p);
	}

	f = (320-26*8)/2;
	M_DrawTextBox (f, 134, 24, 4);
	f += 8;
	M_Print (f, 142, net_helpMessage[m_net_cursor*4+0]);
	M_Print (f, 150, net_helpMessage[m_net_cursor*4+1]);
	M_Print (f, 158, net_helpMessage[m_net_cursor*4+2]);
	M_Print (f, 166, net_helpMessage[m_net_cursor*4+3]);

	f = (int)(host_time * 10)%6;
	M_DrawTransPic (54, 32 + m_net_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );
}


void M_Net_Key (int k)
{
again:
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_MultiPlayer_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_net_cursor >= m_net_items)
			m_net_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_net_cursor < 0)
			m_net_cursor = m_net_items - 1;
		break;

	case K_ENTER:
		m_entersound = true;

		switch (m_net_cursor)
		{
		case 0:
			M_Menu_SerialConfig_f ();
			break;

		case 1:
			M_Menu_SerialConfig_f ();
			break;

		case 2:
			M_Menu_LanConfig_f ();
			break;

		case 3:
			M_Menu_LanConfig_f ();
			break;

		case 4:
// multiprotocol
			break;
		}
	}

	if (m_net_cursor == 0 && !serialAvailable)
		goto again;
	if (m_net_cursor == 1 && !serialAvailable)
		goto again;
	if (m_net_cursor == 2 && !ipxAvailable)
		goto again;
	if (m_net_cursor == 3 && !tcpipAvailable)
		goto again;
}

//=============================================================================
/* OPTIONS MENU */

#define OPTIONS_ITEMS   14

#define SLIDER_RANGE    10

int	     options_cursor;

void M_Menu_Options_f (void)
{
	key_dest = key_menu;
	m_state = m_options;
	m_entersound = true;
}


void M_AdjustSliders (int dir)
{
	S_LocalSound ("misc/menu3.wav");

	switch (options_cursor)
	{
	case 3: // screen size
		scr_viewsize->value += dir * 10;
		if (scr_viewsize->value < 30)
			scr_viewsize->value = 30;
		if (scr_viewsize->value > 120)
			scr_viewsize->value = 120;
		Cvar_SetValue ("viewsize", scr_viewsize->value);
		break;
	case 4: // gamma
		v_gamma->value -= dir * 0.05;
		if (v_gamma->value < 0.5)
			v_gamma->value = 0.5;
		if (v_gamma->value > 1.0)
			v_gamma->value = 1.0;
		Cvar_SetValue ("gamma", v_gamma->value);
		break;
	case 5: // mouse speed
		sensitivity->value += dir * 0.5;
		if (sensitivity->value < 1)
			sensitivity->value = 1;
		if (sensitivity->value > 50) /* FS: 11 is so 1997. */
			sensitivity->value = 50;
		Cvar_SetValue ("sensitivity", sensitivity->value);
		break;
	case 6: // music volume
		s_musicvolume->value += dir * 0.1;
		if (s_musicvolume->value < 0)
			s_musicvolume->value = 0;
		if (s_musicvolume->value > 1)
			s_musicvolume->value = 1;
		Cvar_SetValue ("s_musicvolume", s_musicvolume->value);
		break;
	case 7: // sfx volume
		s_volume->value += dir * 0.1;
		if (s_volume->value < 0)
			s_volume->value = 0;
		if (s_volume->value > 1)
			s_volume->value = 1;
		Cvar_SetValue ("s_volume", s_volume->value);
		break;
	case 8: // always run
		if (cl_forwardspeed->value > 200)
		{
			Cvar_SetValue ("cl_forwardspeed", 200);
			Cvar_SetValue ("cl_backspeed", 200);
			Cvar_SetValue ("cl_sidespeed", 200); /* FS */
		}
		else
		{
			Cvar_SetValue ("cl_forwardspeed", 400);
			Cvar_SetValue ("cl_backspeed", 400);
			Cvar_SetValue ("cl_sidespeed", 400); /* FS */
		}
		break;
	case 9: // invert mouse
		Cvar_SetValue ("m_pitch", -m_pitch->value);
		break;
	case 10:	// lookspring
		Cvar_SetValue ("lookspring", !lookspring->value);
		break;
	case 11:	// lookstrafe
		Cvar_SetValue ("lookstrafe", !lookstrafe->value);
		break;
	}
}


void M_DrawSlider (int x, int y, float range)
{
	int     i;

	if (range < 0)
		range = 0;
	if (range > 1)
		range = 1;
	M_DrawCharacter (x-8, y, 128);
	for (i=0 ; i<SLIDER_RANGE ; i++)
		M_DrawCharacter (x + i*8, y, 129);
	M_DrawCharacter (x+i*8, y, 130);
	M_DrawCharacter (x + (SLIDER_RANGE-1)*8 * range, y, 131);
}

void M_DrawCheckbox (int x, int y, int on)
{
	if (on)
		M_Print (x, y, "on");
	else
		M_Print (x, y, "off");
}

void M_Options_Draw (void)
{
	float	r;
	qpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	
	M_Print (16, 32, "    Customize controls");
	M_Print (16, 40, "         Go to console");
	M_Print (16, 48, "     Reset to defaults");

	M_Print (16, 56, "           Screen size");
	r = (scr_viewsize->value - 30) / (120 - 30);
	M_DrawSlider (220, 56, r);

	M_Print (16, 64, "            Brightness");
	r = (1.0 - v_gamma->value) / 0.5;
	M_DrawSlider (220, 64, r);

	M_Print (16, 72, "           Mouse Speed");
	r = (sensitivity->value)/50; /* FS: Was 11 */
	M_DrawSlider (220, 72, r);

	M_Print (16, 80, "          Music Volume");
	r = s_musicvolume->value;
	M_DrawSlider (220, 80, r);

	M_Print (16, 88, "          Sound Volume");
	r = s_volume->value;
	M_DrawSlider (220, 88, r);

	M_Print (16, 96,  "            Always Run");
	M_DrawCheckbox (220, 96, cl_forwardspeed->value > 200);

	M_Print (16, 104, "          Invert Mouse");
	M_DrawCheckbox (220, 104, m_pitch->value < 0);

	M_Print (16, 112, "            Lookspring");
	M_DrawCheckbox (220, 112, lookspring->value);

	M_Print (16, 120, "            Lookstrafe");
	M_DrawCheckbox (220, 120, lookstrafe->value);

	if (vid_menudrawfn)
		M_Print (16, 128, "         Video Options");

	M_Print (16, 136, "      Extended Options"); /* FS: Extended options unique to QDOS */

// cursor
	M_DrawCharacter (200, 32 + options_cursor*8, 12+((int)(realtime*4)&1));
}

void M_Options_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case 'v': /* FS */
	case 'V':
		M_Menu_Video_f();
		break;

	case 'e': /* FS */
	case 'E':
		M_Menu_Extended_f();
		break;

	case K_ENTER:
		m_entersound = true;
		switch (options_cursor)
		{
		case 0:
			M_Menu_Keys_f ();
			break;
		case 1:
			m_state = m_none;
			Con_ToggleConsole_f ();
			break;
		case 2:
			Cbuf_AddText ("exec default.cfg\n");
			break;
		case 12:
			M_Menu_Video_f ();
			break;
        case 13: /* FS: Extended options unique to QDOS */
			M_Menu_Extended_f ();
			break;
		default:
			M_AdjustSliders (1);
			break;
		}
		return;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor--;
		if (options_cursor < 0)
			options_cursor = OPTIONS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor++;
		if (options_cursor >= OPTIONS_ITEMS)
			options_cursor = 0;
		break;

	case K_LEFTARROW:
		M_AdjustSliders (-1);
		break;

	case K_RIGHTARROW:
		M_AdjustSliders (1);
		break;
	}
}

//=============================================================================
/* KEYS MENU */

char *bindnames[][2] =
{
{"+attack", 		"attack"},
{"impulse 10", 		"change weapon"},
{"+jump", 			"jump / swim up"},
{"+forward", 		"walk forward"},
{"+back", 			"backpedal"},
{"+left", 			"turn left"},
{"+right", 			"turn right"},
{"+speed", 			"run"},
{"+moveleft", 		"step left"},
{"+moveright", 		"step right"},
{"+strafe", 		"sidestep"},
{"+lookup", 		"look up"},
{"+lookdown", 		"look down"},
{"centerview", 		"center view"},
{"+mlook", 			"mouse look"},
{"+klook", 			"keyboard look"},
{"+moveup",			"swim up"},
{"+movedown",		"swim down"}
};

#define NUMCOMMANDS     (sizeof(bindnames)/sizeof(bindnames[0]))

int	     keys_cursor;
int	     bind_grab;

void M_Menu_Keys_f (void)
{
	key_dest = key_menu;
	m_state = m_keys;
	m_entersound = true;
}


void M_FindKeysForCommand (char *command, int *twokeys)
{
	int	     count;
	int	     j;
	int	     l;
	char    *b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen(command);
	count = 0;

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

void M_UnbindCommand (char *command)
{
	int	     j;
	int	     l;
	char    *b;

	l = strlen(command);

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
			Key_SetBinding (j, "");
	}
}


void M_Keys_Draw (void)
{
	size_t   i; //, l;
	int	     keys[2];
	char    *name;
	int	     x, y;
	qpic_t  *p;

	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	if (bind_grab)
		M_Print (12, 32, "Press a key or button for this action");
	else
		M_Print (18, 32, "Enter to change, backspace to clear");

// search for known bindings
	for (i=0 ; i<NUMCOMMANDS ; i++)
	{
		y = 48 + 8*i;

		M_Print (16, y, bindnames[i][1]);

//		l = strlen (bindnames[i][0]);

		M_FindKeysForCommand (bindnames[i][0], keys);

		if (keys[0] == -1)
		{
			M_Print (140, y, "???");
		}
		else
		{
			name = Key_KeynumToString (keys[0]);
			M_Print (140, y, name);
			x = strlen(name) * 8;
			if (keys[1] != -1)
			{
				M_Print (140 + x + 8, y, "or");
				M_Print (140 + x + 32, y, Key_KeynumToString (keys[1]));
			}
		}
	}

	if (bind_grab)
		M_DrawCharacter (130, 48 + keys_cursor*8, '=');
	else
		M_DrawCharacter (130, 48 + keys_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Keys_Key (int k)
{
	char    cmd[80];
	int	     keys[2];
	
	if (bind_grab)
	{
		// defining a key
		S_LocalSound ("misc/menu1.wav");
		if (k == K_ESCAPE)
		{
			bind_grab = false;
		}
		else if (k != '`')
		{
			Com_sprintf (cmd, sizeof(cmd), "bind \"%s\" \"%s\"\n", Key_KeynumToString (k), bindnames[keys_cursor][0]);
			Cbuf_InsertText (cmd);
		}
		
		bind_grab = false;
		return;
	}
	
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Options_f ();
		break;

	case K_LEFTARROW:
	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor--;
		if (keys_cursor < 0)
			keys_cursor = NUMCOMMANDS-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor++;
		if (keys_cursor >= NUMCOMMANDS)
			keys_cursor = 0;
		break;

	case K_ENTER:	   // go into bind mode
		M_FindKeysForCommand (bindnames[keys_cursor][0], keys);
		S_LocalSound ("misc/menu2.wav");
		if (keys[1] != -1)
			M_UnbindCommand (bindnames[keys_cursor][0]);
		bind_grab = true;
		break;

	case K_BACKSPACE:	       // delete bindings
	case K_DEL:			     // delete bindings
		S_LocalSound ("misc/menu2.wav");
		M_UnbindCommand (bindnames[keys_cursor][0]);
		break;
	}
}

//=============================================================================
/* VIDEO MENU */

void M_Menu_Video_f (void)
{
	key_dest = key_menu;
	m_state = m_video;
	m_entersound = true;
}


void M_Video_Draw (void)
{
	(*vid_menudrawfn) ();
}


void M_Video_Key (int key)
{
	(*vid_menukeyfn) (key);
}

//=============================================================================
/* HELP MENU */

int	     help_page;
#define NUM_HELP_PAGES  6


void M_Menu_Help_f (void)
{
	key_dest = key_menu;
	m_state = m_help;
	m_entersound = true;
	help_page = 0;
}



void M_Help_Draw (void)
{
	M_DrawPic (0, 0, Draw_CachePic ( va("gfx/help%i.lmp", help_page)) );
}


void M_Help_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;
		
	case K_UPARROW:
	case K_RIGHTARROW:
		m_entersound = true;
		if (++help_page >= NUM_HELP_PAGES)
			help_page = 0;
		break;

	case K_DOWNARROW:
	case K_LEFTARROW:
		m_entersound = true;
		if (--help_page < 0)
			help_page = NUM_HELP_PAGES-1;
		break;
	}

}

//=============================================================================
/* CEDITS MENU */
#if 0 /* not yet */
void M_Menu_Credits_f (void)
{
	key_dest = key_menu;
	m_state = m_credits;
	m_entersound = true;
}

void M_Credits_Draw (void)
{
//	M_Background(640, 480);
	M_DrawPic (0, 0, Draw_CachePic("gfx/creditsmiddle.lmp"));
	M_Print (640/2 - 14/2*8, 236, "Coming soon...");
	M_DrawPic (0, 0, Draw_CachePic("gfx/creditstop.lmp"));
	M_DrawPic (0, 433, Draw_CachePic("gfx/creditsbottom.lmp"));
}

void M_Credits_Key (int key)
{
	M_Menu_Main_f ();
}
#endif

//=============================================================================
/* QUIT MENU */

int	     msgNumber;
int	     m_quit_prevstate;
qboolean	wasInMenus;

char *quitMessage [] = 
{
/* .........1.........2.... */
  "  Are you gonna quit    ",
  "  this game just like   ",
  "   everything else?     ",
  "                        ",
 
  " Milord, methinks that  ",
  "   thou art a lowly     ",
  " quitter. Is this true? ",
  "                        ",

  " Do I need to bust your ",
  "  face open for trying  ",
  "        to quit?        ",
  "                        ",

  " Man, I oughta smack you",
  "   for trying to quit!  ",
  "     Press Y to get     ",
  "      smacked out.      ",
 
  " Press Y to quit like a ",
  "   big loser in life.   ",
  "  Press N to stay proud ",
  "    and successful!     ",
 
  "   If you press Y to    ",
  "  quit, I will summon   ",
  "  Satan all over your   ",
  "      hard drive!       ",
 
  "  Um, Asmodeus dislikes ",
  " his children trying to ",
  " quit. Press Y to return",
  "   to your Tinkertoys.  ",
 
  "  If you quit now, I'll ",
  "  throw a blanket-party ",
  "   for you next time!   ",
  "                        "
};

#ifdef OLDQUIT_F /* FS: Use QWs version of quitting */
void M_Menu_Quit_f (void)
{
	if (m_state == m_quit)
		return;
	wasInMenus = (key_dest == key_menu);
	key_dest = key_menu;
	m_quit_prevstate = m_state;
	m_state = m_quit;
	m_entersound = true;
	msgNumber = rand()&7;
}


void M_Quit_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case 'n':
	case 'N':
		if (wasInMenus)
		{
			m_state = m_quit_prevstate;
			m_entersound = true;
		}
		else
		{
			key_dest = key_game;
			m_state = m_none;
		}
		break;
	case K_ENTER: /* FS: You can press enter to default Y */
	case 'Y':
	case 'y':
		key_dest = key_console;
		Host_Quit_f ();
		break;

	default:
		break;
	}

}
#endif

void M_Menu_Quit_f (void)
{
	if (m_state == m_quit)
		return;
	wasInMenus = (key_dest == key_menu);
	key_dest = key_menu;
	m_quit_prevstate = m_state;
	m_state = m_quit;
	m_entersound = true;
	msgNumber = rand()&7;
}


void M_Quit_Key (int key)
{
	switch (key)
	{
		case K_ESCAPE:
		case 'n':
		case 'N':
			if (wasInMenus)
			{
				m_state = m_quit_prevstate;
				m_entersound = true;
			}
			else
			{
				key_dest = key_game;
				m_state = m_none;
			}
			break;

		case K_ENTER: /* FS: You can press enter to default Y */
		case 'Y':
		case 'y':
			key_dest = key_console;
			CL_Disconnect ();
			Host_ShutdownServer(false);
			Sys_Quit ();
			break;

		default:
			break;
	}
}


void M_Quit_Draw (void)
{
	if (wasInMenus)
	{
		m_state = m_quit_prevstate;
		m_recursiveDraw = true;
		M_Draw ();
		m_state = m_quit;
	}

	M_DrawTextBox (56, 76, 24, 4);
	M_Print (64, 84,  quitMessage[msgNumber*4+0]);
	M_Print (64, 92,  quitMessage[msgNumber*4+1]);
	M_Print (64, 100, quitMessage[msgNumber*4+2]);
	M_Print (64, 108, quitMessage[msgNumber*4+3]);
}

//=============================================================================

/* SERIAL CONFIG MENU */

int		serialConfig_cursor;
int		serialConfig_cursor_table[] = {48, 64, 80, 96, 112, 132};
#define	NUM_SERIALCONFIG_CMDS	6

static int ISA_uarts[]	= {0x3f8,0x2f8,0x3e8,0x2e8};
static int ISA_IRQs[]	= {4,3,4,3};
int serialConfig_baudrate[] = {9600,14400,19200,28800,38400,57600};

int		serialConfig_comport;
int		serialConfig_irq ;
int		serialConfig_baud;
char	serialConfig_phone[16];

void M_Menu_SerialConfig_f (void)
{
	int		n;
	int		port;
	int		baudrate;
	qboolean	useModem;

	key_dest = key_menu;
	m_state = m_serialconfig;
	m_entersound = true;
	if (JoiningGame && SerialConfig)
		serialConfig_cursor = 4;
	else
		serialConfig_cursor = 5;

	(*GetComPortConfig) (0, &port, &serialConfig_irq, &baudrate, &useModem);

	// map uart's port to COMx
	for (n = 0; n < 4; n++)
		if (ISA_uarts[n] == port)
			break;
	if (n == 4)
	{
		n = 0;
		serialConfig_irq = 4;
	}
	serialConfig_comport = n + 1;

	// map baudrate to index
	for (n = 0; n < 6; n++)
		if (serialConfig_baudrate[n] == baudrate)
			break;
	if (n == 6)
		n = 5;
	serialConfig_baud = n;

	m_return_onerror = false;
	m_return_reason[0] = 0;
}


void M_SerialConfig_Draw (void)
{
	qpic_t	*p;
	int		basex;
	char	*startJoin;
	char	*directModem;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	basex = (320-p->width)/2;
	M_DrawPic (basex, 4, p);

	if (StartingGame)
		startJoin = "New Game";
	else
		startJoin = "Join Game";
	if (SerialConfig)
		directModem = "Modem";
	else
		directModem = "Direct Connect";
	M_Print (basex, 32, va ("%s - %s", startJoin, directModem));
	basex += 8;

	M_Print (basex, serialConfig_cursor_table[0], "Port");
	M_DrawTextBox (160, 40, 4, 1);
	M_Print (168, serialConfig_cursor_table[0], va("COM%u", serialConfig_comport));

	M_Print (basex, serialConfig_cursor_table[1], "IRQ");
	M_DrawTextBox (160, serialConfig_cursor_table[1]-8, 1, 1);
	M_Print (168, serialConfig_cursor_table[1], va("%u", serialConfig_irq));

	M_Print (basex, serialConfig_cursor_table[2], "Baud");
	M_DrawTextBox (160, serialConfig_cursor_table[2]-8, 5, 1);
	M_Print (168, serialConfig_cursor_table[2], va("%u", serialConfig_baudrate[serialConfig_baud]));

	if (SerialConfig)
	{
		M_Print (basex, serialConfig_cursor_table[3], "Modem Setup...");
		if (JoiningGame)
		{
			M_Print (basex, serialConfig_cursor_table[4], "Phone number");
			M_DrawTextBox (160, serialConfig_cursor_table[4]-8, 16, 1);
			M_Print (168, serialConfig_cursor_table[4], serialConfig_phone);
		}
	}

	if (JoiningGame)
	{
		M_DrawTextBox (basex, serialConfig_cursor_table[5]-8, 7, 1);
		M_Print (basex+8, serialConfig_cursor_table[5], "Connect");
	}
	else
	{
		M_DrawTextBox (basex, serialConfig_cursor_table[5]-8, 2, 1);
		M_Print (basex+8, serialConfig_cursor_table[5], "OK");
	}

	M_DrawCharacter (basex-8, serialConfig_cursor_table [serialConfig_cursor], 12+((int)(realtime*4)&1));

	if (serialConfig_cursor == 4)
		M_DrawCharacter (168 + 8*strlen(serialConfig_phone), serialConfig_cursor_table [serialConfig_cursor], 10+((int)(realtime*4)&1));

	if (*m_return_reason)
		M_PrintWhite (basex, 148, m_return_reason);
}


void M_SerialConfig_Key (int key)
{
	int		l;

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Net_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		serialConfig_cursor--;
		if (serialConfig_cursor < 0)
			serialConfig_cursor = NUM_SERIALCONFIG_CMDS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		serialConfig_cursor++;
		if (serialConfig_cursor >= NUM_SERIALCONFIG_CMDS)
			serialConfig_cursor = 0;
		break;

	case K_LEFTARROW:
		if (serialConfig_cursor > 2)
			break;
		S_LocalSound ("misc/menu3.wav");

		if (serialConfig_cursor == 0)
		{
			serialConfig_comport--;
			if (serialConfig_comport == 0)
				serialConfig_comport = 4;
			serialConfig_irq = ISA_IRQs[serialConfig_comport-1];
		}

		if (serialConfig_cursor == 1)
		{
			serialConfig_irq--;
			if (serialConfig_irq == 6)
				serialConfig_irq = 5;
			if (serialConfig_irq == 1)
				serialConfig_irq = 7;
		}

		if (serialConfig_cursor == 2)
		{
			serialConfig_baud--;
			if (serialConfig_baud < 0)
				serialConfig_baud = 5;
		}

		break;

	case K_RIGHTARROW:
		if (serialConfig_cursor > 2)
			break;
forward:
		S_LocalSound ("misc/menu3.wav");

		if (serialConfig_cursor == 0)
		{
			serialConfig_comport++;
			if (serialConfig_comport > 4)
				serialConfig_comport = 1;
			serialConfig_irq = ISA_IRQs[serialConfig_comport-1];
		}

		if (serialConfig_cursor == 1)
		{
			serialConfig_irq++;
			if (serialConfig_irq == 6)
				serialConfig_irq = 7;
			if (serialConfig_irq == 8)
				serialConfig_irq = 2;
		}

		if (serialConfig_cursor == 2)
		{
			serialConfig_baud++;
			if (serialConfig_baud > 5)
				serialConfig_baud = 0;
		}

		break;

	case K_ENTER:
		if (serialConfig_cursor < 3)
			goto forward;

		m_entersound = true;

		if (serialConfig_cursor == 3)
		{
			(*SetComPortConfig) (0, ISA_uarts[serialConfig_comport-1], serialConfig_irq, serialConfig_baudrate[serialConfig_baud], SerialConfig);

			M_Menu_ModemConfig_f ();
			break;
		}

		if (serialConfig_cursor == 4)
		{
			serialConfig_cursor = 5;
			break;
		}

		// serialConfig_cursor == 5 (OK/CONNECT)
		(*SetComPortConfig) (0, ISA_uarts[serialConfig_comport-1], serialConfig_irq, serialConfig_baudrate[serialConfig_baud], SerialConfig);

		M_ConfigureNetSubsystem ();

		if (StartingGame)
		{
			M_Menu_GameOptions_f ();
			break;
		}

		m_return_state = m_state;
		m_return_onerror = true;
		key_dest = key_game;
		m_state = m_none;

		if (SerialConfig)
			Cbuf_AddText (va ("connect \"%s\"\n", serialConfig_phone));
		else
			Cbuf_AddText ("connect\n");
		break;

	case K_BACKSPACE:
		if (serialConfig_cursor == 4)
		{
			if (strlen(serialConfig_phone))
				serialConfig_phone[strlen(serialConfig_phone)-1] = 0;
		}
		break;

	default:
		if (key < 32 || key > 127)
			break;
		if (serialConfig_cursor == 4)
		{
			l = strlen(serialConfig_phone);
			if (l < 15)
			{
				serialConfig_phone[l+1] = 0;
				serialConfig_phone[l] = key;
			}
		}
	}

	if (DirectConfig && (serialConfig_cursor == 3 || serialConfig_cursor == 4))
	{
		if (key == K_UPARROW)
		{
			serialConfig_cursor = 2;
		}
		else
		{
			serialConfig_cursor = 5;
		}
	}
	if (SerialConfig && StartingGame && serialConfig_cursor == 4)
	{
		if (key == K_UPARROW)
		{
			serialConfig_cursor = 3;
		}
		else
		{
			serialConfig_cursor = 5;
		}
	}
}

//=============================================================================
/* MODEM CONFIG MENU */

int		modemConfig_cursor;
int		modemConfig_cursor_table [] = {40, 56, 88, 120, 156};
#define NUM_MODEMCONFIG_CMDS	5

char	modemConfig_dialing;
char	modemConfig_clear [16];
char	modemConfig_init [32];
char	modemConfig_hangup [16];

void M_Menu_ModemConfig_f (void)
{
	key_dest = key_menu;
	m_state = m_modemconfig;
	m_entersound = true;
	(*GetModemConfig) (0, &modemConfig_dialing, modemConfig_clear, modemConfig_init, modemConfig_hangup, sizeof(modemConfig_clear), sizeof(modemConfig_init), sizeof(modemConfig_hangup));
}

void M_ModemConfig_Draw (void)
{
	qpic_t	*p;
	int		basex;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	basex = (320-p->width)/2;
	M_DrawPic (basex, 4, p);
	basex += 8;

	if (modemConfig_dialing == 'P')
		M_Print (basex, modemConfig_cursor_table[0], "Pulse Dialing");
	else
		M_Print (basex, modemConfig_cursor_table[0], "Touch Tone Dialing");

	M_Print (basex, modemConfig_cursor_table[1], "Clear");
	M_DrawTextBox (basex, modemConfig_cursor_table[1]+4, 16, 1);
	M_Print (basex+8, modemConfig_cursor_table[1]+12, modemConfig_clear);
	if (modemConfig_cursor == 1)
		M_DrawCharacter (basex+8 + 8*strlen(modemConfig_clear), modemConfig_cursor_table[1]+12, 10+((int)(realtime*4)&1));

	M_Print (basex, modemConfig_cursor_table[2], "Init");
	M_DrawTextBox (basex, modemConfig_cursor_table[2]+4, 30, 1);
	M_Print (basex+8, modemConfig_cursor_table[2]+12, modemConfig_init);
	if (modemConfig_cursor == 2)
		M_DrawCharacter (basex+8 + 8*strlen(modemConfig_init), modemConfig_cursor_table[2]+12, 10+((int)(realtime*4)&1));

	M_Print (basex, modemConfig_cursor_table[3], "Hangup");
	M_DrawTextBox (basex, modemConfig_cursor_table[3]+4, 16, 1);
	M_Print (basex+8, modemConfig_cursor_table[3]+12, modemConfig_hangup);
	if (modemConfig_cursor == 3)
		M_DrawCharacter (basex+8 + 8*strlen(modemConfig_hangup), modemConfig_cursor_table[3]+12, 10+((int)(realtime*4)&1));

	M_DrawTextBox (basex, modemConfig_cursor_table[4]-8, 2, 1);
	M_Print (basex+8, modemConfig_cursor_table[4], "OK");

	M_DrawCharacter (basex-8, modemConfig_cursor_table [modemConfig_cursor], 12+((int)(realtime*4)&1));
}


void M_ModemConfig_Key (int key)
{
	int		l;

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_SerialConfig_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		modemConfig_cursor--;
		if (modemConfig_cursor < 0)
			modemConfig_cursor = NUM_MODEMCONFIG_CMDS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		modemConfig_cursor++;
		if (modemConfig_cursor >= NUM_MODEMCONFIG_CMDS)
			modemConfig_cursor = 0;
		break;

	case K_LEFTARROW:
	case K_RIGHTARROW:
		if (modemConfig_cursor == 0)
		{
			if (modemConfig_dialing == 'P')
				modemConfig_dialing = 'T';
			else
				modemConfig_dialing = 'P';
			S_LocalSound ("misc/menu1.wav");
		}
		break;

	case K_ENTER:
		if (modemConfig_cursor == 0)
		{
			if (modemConfig_dialing == 'P')
				modemConfig_dialing = 'T';
			else
				modemConfig_dialing = 'P';
			m_entersound = true;
		}

		if (modemConfig_cursor == 4)
		{
			(*SetModemConfig) (0, va ("%c", modemConfig_dialing), modemConfig_clear, modemConfig_init, modemConfig_hangup);
			m_entersound = true;
			M_Menu_SerialConfig_f ();
		}
		break;

	case K_BACKSPACE:
		if (modemConfig_cursor == 1)
		{
			if (strlen(modemConfig_clear))
				modemConfig_clear[strlen(modemConfig_clear)-1] = 0;
		}

		if (modemConfig_cursor == 2)
		{
			if (strlen(modemConfig_init))
				modemConfig_init[strlen(modemConfig_init)-1] = 0;
		}

		if (modemConfig_cursor == 3)
		{
			if (strlen(modemConfig_hangup))
				modemConfig_hangup[strlen(modemConfig_hangup)-1] = 0;
		}
		break;

	default:
		if (key < 32 || key > 127)
			break;

		if (modemConfig_cursor == 1)
		{
			l = strlen(modemConfig_clear);
			if (l < 15)
			{
				modemConfig_clear[l+1] = 0;
				modemConfig_clear[l] = key;
			}
		}

		if (modemConfig_cursor == 2)
		{
			l = strlen(modemConfig_init);
			if (l < 29)
			{
				modemConfig_init[l+1] = 0;
				modemConfig_init[l] = key;
			}
		}

		if (modemConfig_cursor == 3)
		{
			l = strlen(modemConfig_hangup);
			if (l < 15)
			{
				modemConfig_hangup[l+1] = 0;
				modemConfig_hangup[l] = key;
			}
		}
	}
}

//=============================================================================
/* LAN CONFIG MENU */

int		lanConfig_cursor = -1;
#ifndef GAMESPY
int		lanConfig_cursor_table [] = {72, 92, 124};
#define NUM_LANCONFIG_CMDS	3
#else /* FS: Added gamespy */
int		lanConfig_cursor_table [] = {72, 92, 100, 132};
#define NUM_LANCONFIG_CMDS	4
#endif

int 	lanConfig_port;
char	lanConfig_portname[6];
char	lanConfig_joinname[22];

void M_Menu_LanConfig_f (void)
{
	key_dest = key_menu;
	m_state = m_lanconfig;
	m_entersound = true;
	if (lanConfig_cursor == -1)
	{
		if (JoiningGame && TCPIPConfig)
			lanConfig_cursor = 2;
		else
			lanConfig_cursor = 1;
	}
	if (StartingGame && lanConfig_cursor == 2)
		lanConfig_cursor = 1;
	lanConfig_port = DEFAULTnet_hostport;
	Com_sprintf(lanConfig_portname, sizeof(lanConfig_portname), "%u", lanConfig_port);

	m_return_onerror = false;
	m_return_reason[0] = 0;
}


void M_LanConfig_Draw (void)
{
	qpic_t	*p;
	int		basex;
	char	*startJoin;
	char	*protocol;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	basex = (320-p->width)/2;
	M_DrawPic (basex, 4, p);

	if (StartingGame)
		startJoin = "New Game";
	else
		startJoin = "Join Game";
	if (IPXConfig)
		protocol = "IPX";
	else
		protocol = "TCP/IP";
	M_Print (basex, 32, va ("%s - %s", startJoin, protocol));
	basex += 8;

	M_Print (basex, 52, "Address:");
	if (IPXConfig)
		M_Print (basex+9*8, 52, my_ipx_address);
	else
		M_Print (basex+9*8, 52, my_tcpip_address);

	M_Print (basex, lanConfig_cursor_table[0], "Port");
	M_DrawTextBox (basex+8*8, lanConfig_cursor_table[0]-8, 6, 1);
	M_Print (basex+9*8, lanConfig_cursor_table[0], lanConfig_portname);

	if (JoiningGame)
	{
		M_Print (basex, lanConfig_cursor_table[1], "Search for local games...");
#ifdef GAMESPY /* FS: Gamespy stuff */
		M_Print (basex, lanConfig_cursor_table[2], "Search Gamespy...");
#endif
		M_Print (basex, 108+(NUM_LANCONFIG_CMDS-3)*8, "Join game at:");
		M_DrawTextBox (basex+8, lanConfig_cursor_table[NUM_LANCONFIG_CMDS-1]-8, 22, 1);
		M_Print (basex+16, lanConfig_cursor_table[NUM_LANCONFIG_CMDS-1], lanConfig_joinname);
	}
	else
	{
		M_DrawTextBox (basex, lanConfig_cursor_table[1]-8, 2, 1);
		M_Print (basex+8, lanConfig_cursor_table[1], "OK");
	}

	M_DrawCharacter (basex-8, lanConfig_cursor_table [lanConfig_cursor], 12+((int)(realtime*4)&1));

	if (lanConfig_cursor == 0)
		M_DrawCharacter (basex+9*8 + 8*strlen(lanConfig_portname), lanConfig_cursor_table [0], 10+((int)(realtime*4)&1));

	if (lanConfig_cursor == NUM_LANCONFIG_CMDS-1)
		M_DrawCharacter (basex+16 + 8*strlen(lanConfig_joinname), lanConfig_cursor_table [NUM_LANCONFIG_CMDS-1], 10+((int)(realtime*4)&1));

	if (*m_return_reason)
		M_PrintWhite (basex, 148, m_return_reason);
}


void M_LanConfig_Key (int key)
{
	int		l;

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Net_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		lanConfig_cursor--;
#ifdef GAMESPY
		if (StartingGame && lanConfig_cursor < 0) /* FS: -2 now because we got the gamespy stuff. */
			lanConfig_cursor = NUM_LANCONFIG_CMDS-2;
		else
#endif
		if (lanConfig_cursor < 0)
			lanConfig_cursor = NUM_LANCONFIG_CMDS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		lanConfig_cursor++;
		if (lanConfig_cursor >= NUM_LANCONFIG_CMDS)
			lanConfig_cursor = 0;
		break;

	case K_ENTER:
		if (lanConfig_cursor == 0)
			break;

		m_entersound = true;

		M_ConfigureNetSubsystem ();

		if (lanConfig_cursor == 1)
		{
			if (StartingGame)
			{
				M_Menu_GameOptions_f ();
				break;
			}
			M_Menu_Search_f();
			break;
		}

#ifdef GAMESPY /* FS: Added Gamespy */
		if (lanConfig_cursor == 2)
		{
			M_Menu_Gamespy_f();
			break;
		}
#endif

		if (lanConfig_cursor == NUM_LANCONFIG_CMDS-1)
		{
			m_return_state = m_state;
			m_return_onerror = true;
			key_dest = key_game;
			m_state = m_none;
			Cbuf_AddText ( va ("connect \"%s\"\n", lanConfig_joinname) );
			break;
		}

		break;

	case K_BACKSPACE:
		if (lanConfig_cursor == 0)
		{
			if (strlen(lanConfig_portname))
				lanConfig_portname[strlen(lanConfig_portname)-1] = 0;
		}

		if (lanConfig_cursor == NUM_LANCONFIG_CMDS-1)
		{
			if (strlen(lanConfig_joinname))
				lanConfig_joinname[strlen(lanConfig_joinname)-1] = 0;
		}
		break;

	default:
		if (key < 32 || key > 127)
			break;

		if (lanConfig_cursor == NUM_LANCONFIG_CMDS-1)
		{
			l = strlen(lanConfig_joinname);
			if (l < 21)
			{
				lanConfig_joinname[l+1] = 0;
				lanConfig_joinname[l] = key;
			}
		}

		if (key < '0' || key > '9')
			break;
		if (lanConfig_cursor == 0)
		{
			l = strlen(lanConfig_portname);
			if (l < 5)
			{
				lanConfig_portname[l+1] = 0;
				lanConfig_portname[l] = key;
			}
		}
	}

#ifdef GAMESPY /* FS: Added Gamespy */
	if (StartingGame && lanConfig_cursor == NUM_LANCONFIG_CMDS-2) /* FS: -2 now because we got the gamespy stuff. */
#else
	if (StartingGame && lanConfig_cursor == NUM_LANCONFIG_CMDS-1)
#endif
	{
		if (key == K_UPARROW)
			lanConfig_cursor = 1;
		else
			lanConfig_cursor = 0;
	}

	l =  atoi(lanConfig_portname);

	if (l > 65535)
		l = lanConfig_port;
	else
		lanConfig_port = l;

	Com_sprintf(lanConfig_portname, sizeof(lanConfig_portname), "%u", lanConfig_port);
}

//=============================================================================
/* GAME OPTIONS MENU */

typedef struct
{
	char	*name;
	char	*description;
} level_t;

level_t		levels[] =
{
	{"start", "Entrance"},	// 0

	{"e1m1", "Slipgate Complex"},				// 1
	{"e1m2", "Castle of the Damned"},
	{"e1m3", "The Necropolis"},
	{"e1m4", "The Grisly Grotto"},
	{"e1m5", "Gloom Keep"},
	{"e1m6", "The Door To Chthon"},
	{"e1m7", "The House of Chthon"},
	{"e1m8", "Ziggurat Vertigo"},

	{"e2m1", "The Installation"},				// 9
	{"e2m2", "Ogre Citadel"},
	{"e2m3", "Crypt of Decay"},
	{"e2m4", "The Ebon Fortress"},
	{"e2m5", "The Wizard's Manse"},
	{"e2m6", "The Dismal Oubliette"},
	{"e2m7", "Underearth"},

	{"e3m1", "Termination Central"},			// 16
	{"e3m2", "The Vaults of Zin"},
	{"e3m3", "The Tomb of Terror"},
	{"e3m4", "Satan's Dark Delight"},
	{"e3m5", "Wind Tunnels"},
	{"e3m6", "Chambers of Torment"},
	{"e3m7", "The Haunted Halls"},

	{"e4m1", "The Sewage System"},				// 23
	{"e4m2", "The Tower of Despair"},
	{"e4m3", "The Elder God Shrine"},
	{"e4m4", "The Palace of Hate"},
	{"e4m5", "Hell's Atrium"},
	{"e4m6", "The Pain Maze"},
	{"e4m7", "Azure Agony"},
	{"e4m8", "The Nameless City"},

	{"end", "Shub-Niggurath's Pit"},			// 31

	{"dm1", "Place of Two Deaths"},				// 32
	{"dm2", "Claustrophobopolis"},
	{"dm3", "The Abandoned Base"},
	{"dm4", "The Bad Place"},
	{"dm5", "The Cistern"},
	{"dm6", "The Dark Zone"}
};

//MED 01/06/97 added hipnotic levels
level_t     hipnoticlevels[] =
{
   {"start", "Command HQ"},  // 0

   {"hip1m1", "The Pumping Station"},          // 1
   {"hip1m2", "Storage Facility"},
   {"hip1m3", "The Lost Mine"},
   {"hip1m4", "Research Facility"},
   {"hip1m5", "Military Complex"},

   {"hip2m1", "Ancient Realms"},          // 6
   {"hip2m2", "The Black Cathedral"},
   {"hip2m3", "The Catacombs"},
   {"hip2m4", "The Crypt"},
   {"hip2m5", "Mortum's Keep"},
   {"hip2m6", "The Gremlin's Domain"},

   {"hip3m1", "Tur Torment"},       // 12
   {"hip3m2", "Pandemonium"},
   {"hip3m3", "Limbo"},
   {"hip3m4", "The Gauntlet"},

   {"hipend", "Armagon's Lair"},       // 16

   {"hipdm1", "The Edge of Oblivion"}           // 17
};

//PGM 01/07/97 added rogue levels
//PGM 03/02/97 added dmatch level
level_t		roguelevels[] =
{
	{"start",	"Split Decision"},
	{"r1m1",	"Deviant's Domain"},
	{"r1m2",	"Dread Portal"},
	{"r1m3",	"Judgement Call"},
	{"r1m4",	"Cave of Death"},
	{"r1m5",	"Towers of Wrath"},
	{"r1m6",	"Temple of Pain"},
	{"r1m7",	"Tomb of the Overlord"},
	{"r2m1",	"Tempus Fugit"},
	{"r2m2",	"Elemental Fury I"},
	{"r2m3",	"Elemental Fury II"},
	{"r2m4",	"Curse of Osiris"},
	{"r2m5",	"Wizard's Keep"},
	{"r2m6",	"Blood Sacrifice"},
	{"r2m7",	"Last Bastion"},
	{"r2m8",	"Source of Evil"},
	{"ctf1",    "Division of Change"}
};

typedef struct
{
	char	*description;
	int		firstLevel;
	int		levels;
} episode_t;

episode_t	episodes[] =
{
	{"Welcome to Quake", 0, 1},
	{"Doomed Dimension", 1, 8},
	{"Realm of Black Magic", 9, 7},
	{"Netherworld", 16, 7},
	{"The Elder World", 23, 8},
	{"Final Level", 31, 1},
	{"Deathmatch Arena", 32, 6}
};

//MED 01/06/97  added hipnotic episodes
episode_t   hipnoticepisodes[] =
{
   {"Scourge of Armagon", 0, 1},
   {"Fortress of the Dead", 1, 5},
   {"Dominion of Darkness", 6, 6},
   {"The Rift", 12, 4},
   {"Final Level", 16, 1},
   {"Deathmatch Arena", 17, 1}
};

//PGM 01/07/97 added rogue episodes
//PGM 03/02/97 added dmatch episode
episode_t	rogueepisodes[] =
{
	{"Introduction", 0, 1},
	{"Hell's Fortress", 1, 7},
	{"Corridors of Time", 8, 8},
	{"Deathmatch Arena", 16, 1}
};

int	startepisode;
int	startlevel;
int maxplayers;
qboolean m_serverInfoMessage = false;
double m_serverInfoMessageTime;

void M_Menu_GameOptions_f (void)
{
	key_dest = key_menu;
	m_state = m_gameoptions;
	m_entersound = true;
	if (maxplayers == 0)
		maxplayers = svs.maxclients;
	if (maxplayers < 2)
		maxplayers = svs.maxclientslimit;
}


int gameoptions_cursor_table[] = {40, 56, 64, 72, 80, 88, 96, 112, 120};
#define	NUM_GAMEOPTIONS	9
int		gameoptions_cursor;

void M_GameOptions_Draw (void)
{
	qpic_t	*p;
	int		x;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	M_DrawTextBox (152, 32, 10, 1);
	M_Print (160, 40, "begin game");

	M_Print (0, 56, "      Max players");
	M_Print (160, 56, va("%i", maxplayers) );

	M_Print (0, 64, "        Game Type");
	if (coop->intValue)
		M_Print (160, 64, "Cooperative");
	else
		M_Print (160, 64, "Deathmatch");

	M_Print (0, 72, "        Teamplay");
	if (rogue)
	{
		char *msg;

		switch(teamplay->intValue)
		{
			case 1: msg = "No Friendly Fire"; break;
			case 2: msg = "Friendly Fire"; break;
			case 3: msg = "Tag"; break;
			case 4: msg = "Capture the Flag"; break;
			case 5: msg = "One Flag CTF"; break;
			case 6: msg = "Three Team CTF"; break;
			default: msg = "Off"; break;
		}
		M_Print (160, 72, msg);
	}
	else
	{
		char *msg;

		switch(teamplay->intValue)
		{
			case 1: msg = "No Friendly Fire"; break;
			case 2: msg = "Friendly Fire"; break;
			default: msg = "Off"; break;
		}
		M_Print (160, 72, msg);
	}

	M_Print (0, 80, "            Skill");
	if (skill->value == 0)
		M_Print (160, 80, "Easy difficulty");
	else if (skill->value == 1)
		M_Print (160, 80, "Normal difficulty");
	else if (skill->value == 2)
		M_Print (160, 80, "Hard difficulty");
	else
		M_Print (160, 80, "Nightmare difficulty");

	M_Print (0, 88, "       Frag Limit");
	if (fraglimit->value == 0)
		M_Print (160, 88, "none");
	else
		M_Print (160, 88, va("%i frags", (int)fraglimit->value));

	M_Print (0, 96, "       Time Limit");
	if (timelimit->value == 0)
		M_Print (160, 96, "none");
	else
		M_Print (160, 96, va("%i minutes", (int)timelimit->value));

	M_Print (0, 112, "         Episode");
   //MED 01/06/97 added hipnotic episodes
   if (hipnotic)
      M_Print (160, 112, hipnoticepisodes[startepisode].description);
   //PGM 01/07/97 added rogue episodes
   else if (rogue)
      M_Print (160, 112, rogueepisodes[startepisode].description);
   else
      M_Print (160, 112, episodes[startepisode].description);

	M_Print (0, 120, "           Level");
   //MED 01/06/97 added hipnotic episodes
   if (hipnotic)
   {
      M_Print (160, 120, hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].description);
      M_Print (160, 128, hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].name);
   }
   //PGM 01/07/97 added rogue episodes
   else if (rogue)
   {
      M_Print (160, 120, roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].description);
      M_Print (160, 128, roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].name);
   }
   else
   {
      M_Print (160, 120, levels[episodes[startepisode].firstLevel + startlevel].description);
      M_Print (160, 128, levels[episodes[startepisode].firstLevel + startlevel].name);
   }

// line cursor
	M_DrawCharacter (144, gameoptions_cursor_table[gameoptions_cursor], 12+((int)(realtime*4)&1));

	if (m_serverInfoMessage)
	{
		if ((realtime - m_serverInfoMessageTime) < 5.0)
		{
			x = (320-26*8)/2;
			M_DrawTextBox (x, 138, 24, 4);
			x += 8;
			M_Print (x, 146, "  More than 4 players   ");
			M_Print (x, 154, " requires using command ");
			M_Print (x, 162, "line parameters; please ");
			M_Print (x, 170, "   see techinfo.txt.    ");
		}
		else
		{
			m_serverInfoMessage = false;
		}
	}
}


void M_NetStart_Change (int dir)
{
	int count;

	switch (gameoptions_cursor)
	{
	case 1:
		maxplayers += dir;
		if (maxplayers > svs.maxclientslimit)
		{
			maxplayers = svs.maxclientslimit;
			m_serverInfoMessage = true;
			m_serverInfoMessageTime = realtime;
		}
		if (maxplayers < 2)
			maxplayers = 2;
		break;

	case 2:
		Cvar_SetValue ("coop", coop->intValue ? 0 : 1);
		break;

	case 3:
		if (rogue)
			count = 6;
		else
			count = 2;

		Cvar_SetValue ("teamplay", teamplay->intValue + dir);
		if (teamplay->intValue > count)
			Cvar_SetValue ("teamplay", 0);
		else if (teamplay->intValue < 0)
			Cvar_SetValue ("teamplay", count);
		break;

	case 4:
		Cvar_SetValue ("skill", skill->intValue + dir);
		if (skill->intValue > 3)
			Cvar_SetValue ("skill", 0);
		if (skill->intValue < 0)
			Cvar_SetValue ("skill", 3);
		break;

	case 5:
		Cvar_SetValue ("fraglimit", fraglimit->intValue + dir*10);
		if (fraglimit->intValue > 100)
			Cvar_SetValue ("fraglimit", 0);
		if (fraglimit->intValue < 0)
			Cvar_SetValue ("fraglimit", 100);
		break;

	case 6:
		Cvar_SetValue ("timelimit", timelimit->intValue + dir*5);
		if (timelimit->intValue > 60)
			Cvar_SetValue ("timelimit", 0);
		if (timelimit->intValue < 0)
			Cvar_SetValue ("timelimit", 60);
		break;

	case 7:
		startepisode += dir;
	//MED 01/06/97 added hipnotic count
		if (hipnotic)
			count = 6;
	//PGM 01/07/97 added rogue count
	//PGM 03/02/97 added 1 for dmatch episode
		else if (rogue)
			count = 4;
		else if (registered->value)
			count = 7;
		else
			count = 2;

		if (startepisode < 0)
			startepisode = count - 1;

		if (startepisode >= count)
			startepisode = 0;

		startlevel = 0;
		break;

	case 8:
		startlevel += dir;
    //MED 01/06/97 added hipnotic episodes
		if (hipnotic)
			count = hipnoticepisodes[startepisode].levels;
	//PGM 01/06/97 added hipnotic episodes
		else if (rogue)
			count = rogueepisodes[startepisode].levels;
		else
			count = episodes[startepisode].levels;

		if (startlevel < 0)
			startlevel = count - 1;

		if (startlevel >= count)
			startlevel = 0;
		break;
	}
}

void M_GameOptions_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Net_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor--;
		if (gameoptions_cursor < 0)
			gameoptions_cursor = NUM_GAMEOPTIONS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor++;
		if (gameoptions_cursor >= NUM_GAMEOPTIONS)
			gameoptions_cursor = 0;
		break;

	case K_LEFTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (-1);
		break;

	case K_RIGHTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (1);
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu2.wav");
		if (gameoptions_cursor == 0)
		{
			if (sv.active)
				Cbuf_AddText ("disconnect\n");
			Cbuf_AddText ("listen 0\n");	// so host_netport will be re-examined
			Cbuf_AddText ( va ("maxplayers %u\n", maxplayers) );
			SCR_BeginLoadingPlaque ();

			if (hipnotic)
				Cbuf_AddText ( va ("map %s\n", hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].name) );
			else if (rogue)
				Cbuf_AddText ( va ("map %s\n", roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].name) );
			else
				Cbuf_AddText ( va ("map %s\n", levels[episodes[startepisode].firstLevel + startlevel].name) );

			return;
		}

		M_NetStart_Change (1);
		break;
	}
}

//=============================================================================
/* SEARCH MENU */

qboolean	searchComplete = false;
double		searchCompleteTime;

void M_Menu_Search_f (void)
{
	key_dest = key_menu;
	m_state = m_search;
	m_entersound = false;
	slistSilent = true;
	slistLocal = false;
	searchComplete = false;
	NET_Slist_f();

}


void M_Search_Draw (void)
{
	qpic_t	*p;
	int x;

	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	x = (320/2) - ((12*8)/2) + 4;
	M_DrawTextBox (x-8, 32, 12, 1);
	M_Print (x, 40, "Searching...");

	if(slistInProgress)
	{
		NET_Poll();
		return;
	}

	if (! searchComplete)
	{
		searchComplete = true;
		searchCompleteTime = realtime;
	}

	if (hostCacheCount)
	{
		M_Menu_ServerList_f ();
		return;
	}

	M_PrintWhite ((320/2) - ((22*8)/2), 64, "No Quake servers found");
	if ((realtime - searchCompleteTime) < 3.0)
		return;

	M_Menu_LanConfig_f ();
}


void M_Search_Key (int key)
{
}

//=============================================================================
/* SLIST MENU */

int		slist_cursor;
qboolean slist_sorted;

void M_Menu_ServerList_f (void)
{
	key_dest = key_menu;
	m_state = m_slist;
	m_entersound = true;
	slist_cursor = 0;
	m_return_onerror = false;
	m_return_reason[0] = 0;
	slist_sorted = false;
}


void M_ServerList_Draw (void)
{
	int		n;
	char	string [64];
	qpic_t	*p;

	if (!slist_sorted)
	{
		if (hostCacheCount > 1)
		{
			int	i,j;
			hostcache_t temp;
			for (i = 0; i < hostCacheCount; i++)
				for (j = i+1; j < hostCacheCount; j++)
					if (strcmp(hostcache[j].name, hostcache[i].name) < 0)
					{
						memcpy(&temp, &hostcache[j], sizeof(hostcache_t));
						memcpy(&hostcache[j], &hostcache[i], sizeof(hostcache_t));
						memcpy(&hostcache[i], &temp, sizeof(hostcache_t));
					}
		}
		slist_sorted = true;
	}

	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	for (n = 0; n < hostCacheCount; n++)
	{
		if (hostcache[n].maxusers)
			Com_sprintf(string, sizeof(string), "%-15.15s %-15.15s %2u/%2u\n", hostcache[n].name, hostcache[n].map, hostcache[n].users, hostcache[n].maxusers);
		else
			Com_sprintf(string, sizeof(string), "%-15.15s %-15.15s\n", hostcache[n].name, hostcache[n].map);
		M_Print (16, 32 + 8*n, string);
	}
	M_DrawCharacter (0, 32 + slist_cursor*8, 12+((int)(realtime*4)&1));

	if (*m_return_reason)
		M_PrintWhite (16, 148, m_return_reason);
}


void M_ServerList_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_LanConfig_f ();
		break;

	case K_SPACE:
		M_Menu_Search_f ();
		break;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		slist_cursor--;
		if (slist_cursor < 0)
			slist_cursor = hostCacheCount - 1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		slist_cursor++;
		if (slist_cursor >= hostCacheCount)
			slist_cursor = 0;
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu2.wav");
		m_return_state = m_state;
		m_return_onerror = true;
		slist_sorted = false;
		key_dest = key_game;
		m_state = m_none;
		Cbuf_AddText ( va ("connect \"%s\"\n", hostcache[slist_cursor].cname) );
		break;

	default:
		break;
	}

}

//=============================================================================
/* Menu Subsystem */



void M_Init (void)
{
	Cmd_AddCommand ("togglemenu", M_ToggleMenu_f);

	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
	Cmd_AddCommand ("menu_singleplayer", M_Menu_SinglePlayer_f);
	Cmd_AddCommand ("menu_load", M_Menu_Load_f);
	Cmd_AddCommand ("menu_save", M_Menu_Save_f);
	Cmd_AddCommand ("menu_multiplayer", M_Menu_MultiPlayer_f);
	Cmd_AddCommand ("menu_setup", M_Menu_Setup_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	Cmd_AddCommand ("help", M_Menu_Help_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);
	Cmd_AddCommand ("menu_extended", M_Menu_Extended_f); /* FS: Extended options unique to QDOS */
#if 0
	Cmd_AddCommand ("menu_credits", M_Menu_Credits_f);
#endif
	Cmd_AddCommand ("menu_demos", M_Menu_Demos_f);
#ifdef GAMESPY
	Cmd_AddCommand ("menu_gamespy", M_Menu_Gamespy_f); /* FS: Gamespy stuff */
#endif
}


void M_Draw (void)
{
	if (m_state == m_none || key_dest != key_menu)
		return;

	if (!m_recursiveDraw)
	{
		scr_copyeverything = 1;

		if (scr_con_current)
		{
			Draw_ConsoleBackground (vid.height);
		}
		else
			Draw_FadeScreen ();

		scr_fullupdate = 0;
	}
	else
	{
		m_recursiveDraw = false;
	}

	switch (m_state)
	{
	case m_none:
		break;
	case m_main:
		M_Main_Draw ();
		break;
	case m_demo:
		M_Demo_Draw ();
		break;
	case m_singleplayer:
		M_SinglePlayer_Draw ();
		break;
	case m_load:
		M_Load_Draw ();
		break;
	case m_save:
		M_Save_Draw ();
		break;
	case m_multiplayer:
		M_MultiPlayer_Draw ();
		break;
	case m_setup:
		M_Setup_Draw ();
		break;
	case m_net:
		M_Net_Draw ();
		break;
	case m_options:
		M_Options_Draw ();
		break;
	case m_keys:
		M_Keys_Draw ();
		break;
	case m_video:
		M_Video_Draw ();
		break;
	case m_help:
		M_Help_Draw ();
		break;
#if 0
	case m_credits:
		M_Credits_Draw ();
		break;
#endif
	case m_quit:
		M_Quit_Draw ();
		break;
	case m_serialconfig:
		M_SerialConfig_Draw ();
		break;
	case m_modemconfig:
		M_ModemConfig_Draw ();
		break;
	case m_lanconfig:
		M_LanConfig_Draw ();
		break;
	case m_gameoptions:
		M_GameOptions_Draw ();
		break;
	case m_search:
		M_Search_Draw ();
		break;
	case m_slist:
		M_ServerList_Draw ();
		break;
	case m_extended:  /* FS: Extended options unique to QDOS */
		M_Extended_Draw();
		break;	
#ifdef GAMESPY
	case m_gamespy:
		M_Gamespy_Draw();
		break;
	case m_gamespy_pages:
		JoinGamespyServer_Redraw(serverscale);
		break;
#endif
	}

	if (m_entersound)
	{
		S_LocalSound ("misc/menu2.wav");
		m_entersound = false;
	}
}

void M_Keydown (int key)
{
	switch (m_state)
	{
	case m_none:
		return;
	case m_main:
		M_Main_Key (key);
		return;
	case m_demo:
		M_Demo_Key (key);
		return;
	case m_singleplayer:
		M_SinglePlayer_Key (key);
		return;
	case m_load:
		M_Load_Key (key);
		return;
	case m_save:
		M_Save_Key (key);
		return;
	case m_multiplayer:
		M_MultiPlayer_Key (key);
		return;
	case m_setup:
		M_Setup_Key (key);
		return;
	case m_net:
		M_Net_Key (key);
		return;
	case m_options:
		M_Options_Key (key);
		return;
	case m_keys:
		M_Keys_Key (key);
		return;
	case m_video:
		M_Video_Key (key);
		return;
	case m_help:
		M_Help_Key (key);
		return;
#if 0
	case m_credits:
		M_Credits_Key (key);
		return;
#endif
	case m_quit:
		M_Quit_Key (key);
		return;
	case m_serialconfig:
		M_SerialConfig_Key (key);
		return;
	case m_modemconfig:
		M_ModemConfig_Key (key);
		return;
	case m_lanconfig:
		M_LanConfig_Key (key);
		return;
	case m_gameoptions:
		M_GameOptions_Key (key);
		return;
	case m_search:
		M_Search_Key (key);
		break;
	case m_slist:
		M_ServerList_Key (key);
		return;
	case m_extended: /* FS: Extended options unique to QDOS */
		M_Extended_Key (key);
		return;
#ifdef GAMESPY
	case m_gamespy:
	case m_gamespy_pages:
		M_Gamespy_Key (key);
		return;
#endif
	}
}


void M_ConfigureNetSubsystem(void)
{
// enable/disable net systems to match desired config

	Cbuf_AddText ("stopdemo\n");
	if (SerialConfig || DirectConfig)
	{
		Cbuf_AddText ("com1 enable\n");
	}

	if (IPXConfig || TCPIPConfig)
		net_hostport = lanConfig_port;
}

/* FS: Extended options unique to QDOS */
#define Y_SPACE 8
#define EXTENDED_OPTIONS 13

int extended_cursor;

extern cvar_t *r_waterwarp;
extern cvar_t *m_filter;
extern cvar_t *sv_autosave;

int M_Extended_Get_Vsync(void);
void M_Extended_Set_Vsync(int dir);
void M_Extended_Set_Sound_KHz (int dir, int khz); /* FS: Extended options unique to QDOS */

void M_Menu_Extended_f(void)
{
	key_dest = key_menu;
	m_state = m_extended;
	m_entersound = true;

}

void M_Extended_Draw (void)
{
	int		y = 32;
	float	r;
	qpic_t  *p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	M_Print (16, y, "      Content Blending");
	M_DrawCheckbox (220, y, v_contentblend->intValue);

	M_Print (16, y = y + Y_SPACE, "            Full Pitch");
	M_DrawCheckbox (220, y, pq_fullpitch->intValue);

	M_Print (16, y = y + Y_SPACE, "         Startup Demos");
	M_DrawCheckbox (220, y, cl_demos->intValue);

	M_Print (16, y = y + Y_SPACE, "     Unbindall Protect");
	M_DrawCheckbox (220, y, cl_unbindall_protection->intValue);

	M_Print (16, y = y + Y_SPACE, "           Show Uptime");
	if (show_uptime->intValue < 1 )
		M_Print (220, y, "off");
	else if (show_uptime->intValue == 1)
		M_Print (220, y, "Server");
	else
		M_Print (220, y, "Total");

	M_Print (16, y = y + Y_SPACE, "             Show Time");
	if (show_time->intValue < 1 )
		M_Print (220, y, "off");
	else if (show_time->intValue == 1)
		M_Print (220, y, "Military");
	else
		M_Print (220, y, "AM/PM");

	M_Print (16, y = y + Y_SPACE, "        Show Framerate");
	M_DrawCheckbox (220, y, show_fps->intValue);

	M_Print (16, y = y + Y_SPACE, "        Mouse Freelook");
	M_DrawCheckbox (220, y, in_freelook->intValue);

	M_Print (16, y = y + Y_SPACE, "       Water View-warp");
	M_DrawCheckbox (220, y, r_waterwarp->intValue);

	M_Print (16, y = y + Y_SPACE, "       Field of Vision");
	r = (scr_fov->value - 30) / (175 - 30);
	M_DrawSlider (220, y, r);

	M_Print (16, y = y + Y_SPACE, "            Sound Rate");
	if (s_khz->intValue < 22050)
	{
		if (havegus)
		{
			if(havegus == GUS_CLASSIC)
				Cvar_SetValue("s_khz", 19293);
			else
				Cvar_SetValue("s_khz", 11025);
		}
		else
			Cvar_SetValue("s_khz", 11025);
	}
	M_Print (220, y, s_khz->string);

	M_Print (16, y = y + Y_SPACE, "                V-Sync");
#ifdef GLQUAKE
	M_Print (220, y, "DISABLED");
#else
	M_DrawCheckbox (220, y, M_Extended_Get_Vsync());
#endif
	M_Print (16, y = y + Y_SPACE, "       Mouse Filtering");
	M_DrawCheckbox (220, y, m_filter->intValue);

	M_Print (16, y = y + Y_SPACE,  "             Auto-Save");
	M_DrawCheckbox (220, y, sv_autosave->intValue);

	M_DrawCharacter (200, 32 + extended_cursor*8, 12+((int)(realtime*4)&1));
}

void M_AdjustSliders_Extended (int dir)
{
	switch(extended_cursor)
	{
	case 0:
		Cvar_SetValue ("v_contentblend", !v_contentblend->intValue);
		break;
	case 1:
		Cvar_SetValue ("pq_fullpitch", !pq_fullpitch->intValue);
		Cvar_SetValue ("cl_fullpitch", pq_fullpitch->intValue);
		break;
	case 2:
		Cvar_SetValue ("cl_demos", !cl_demos->intValue);
		break;
	case 3:
		Cvar_SetValue ("cl_unbindall_protection", !cl_unbindall_protection->intValue);
		break;
	case 4:
		if (show_uptime->value >= 2)
			Cvar_SetValue ("show_uptime", 0);
		else if (show_uptime->value <= 0)
			Cvar_SetValue ("show_uptime", 1);
		else
			Cvar_SetValue ("show_uptime", 2);
		break;
	case 5:
		if (show_time->value >= 2)
			Cvar_SetValue ("show_time", 0);
		else if (show_time->value <= 0)
			Cvar_SetValue ("show_time", 1);
		else
			Cvar_SetValue ("show_time", 2);
		break;
	case 6:
		Cvar_SetValue ("show_fps", !show_fps->intValue);
		break;
	case 7:
		Cvar_SetValue ("in_freelook", !in_freelook->intValue);
		break;
	case 8:
		Cvar_SetValue ("r_waterwarp", !r_waterwarp->intValue);
		break;
	case 9:
		scr_fov->value += dir * 5;
		if (scr_fov->value < 0)
			scr_fov->value = 0;
		if (scr_fov->value > 170)
			scr_fov->value = 170;
		Cvar_SetValue ("fov", scr_fov->value);
		break;
	case 10:
		M_Extended_Set_Sound_KHz(dir, s_khz->intValue);
		break;
	case 11:
		M_Extended_Set_Vsync(dir);
		break;
	case 12:
		Cvar_SetValue ("m_filter", !m_filter->intValue);
		break;
	case 13:
		Cvar_SetValue ("sv_autosave", !sv_autosave->intValue);
		break;
	default:
		break;
	}
	S_LocalSound ("misc/menu3.wav");
}

void M_Extended_Key(int k)
{
	switch (k)
	{
	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		extended_cursor--;
		if (extended_cursor < 0)
			extended_cursor = EXTENDED_OPTIONS;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		extended_cursor++;
		if (extended_cursor > EXTENDED_OPTIONS)
			extended_cursor = 0;
		break;

	case K_ESCAPE:
		M_Menu_Options_f ();
		break;

	case K_LEFTARROW:
		M_AdjustSliders_Extended(-1);
		break;

	case K_RIGHTARROW:
		M_AdjustSliders_Extended(1);
		break;

	case K_ENTER:
		m_entersound = true;

		switch (extended_cursor)
		{
		case 0:
			Cvar_SetValue ("v_contentblend", !v_contentblend->intValue);
			break;
		case 1:
			Cvar_SetValue ("pq_fullpitch", !pq_fullpitch->intValue);
			Cvar_SetValue ("cl_fullpitch", pq_fullpitch->intValue);
			break;
		case 2:
			Cvar_SetValue ("cl_demos", !cl_demos->intValue);
			break;
		case 3:
			Cvar_SetValue ("cl_unbindall_protection", !cl_unbindall_protection->intValue);
			break;
		case 4:
			if (show_uptime->intValue >= 2)
				Cvar_SetValue ("show_uptime", 0);
			else if (show_uptime->intValue <= 0)
				Cvar_SetValue ("show_uptime", 1);
			else
				Cvar_SetValue ("show_uptime", 2);
			break;
		case 5:
			if (show_time->intValue >= 2)
				Cvar_SetValue ("show_time", 0);
			else if (show_time->intValue <= 0)
				Cvar_SetValue ("show_time", 1);
			else
				Cvar_SetValue ("show_time", 2);
			break;
		case 6:
			Cvar_SetValue ("show_fps", !show_fps->intValue);
			break;
		case 7:
			Cvar_SetValue ("in_freelook", !in_freelook->intValue);
			break;
		case 8:
			Cvar_SetValue ("r_waterwarp", !r_waterwarp->intValue);
			break;
		case 9:
			M_AdjustSliders_Extended(1);
			break;
		case 10:
			M_AdjustSliders_Extended(1);
			break;
		case 11:
			M_Extended_Set_Vsync( !M_Extended_Get_Vsync() );
			break;
		default:
			break;
		}
	}
}

int M_Extended_Get_Vsync(void)
{
	if((!_vid_wait_override->intValue) && (vid_wait->intValue != 1))
		return 0;

	return 1;
}

void M_Extended_Set_Vsync(int dir)
{
#ifdef GLQUAKE
	return;
#else
	if (dir > 0)
	{
		Cvar_SetValue("_vid_wait_override", 1.0f);
		Cvar_SetValue("vid_wait", 1.0f);
	}
	else
	{
		Cvar_SetValue("_vid_wait_override", 0.0f);
		Cvar_SetValue("vid_wait", 0.0f);
	}
#endif
}

void M_Extended_Set_Sound_KHz (int dir, int khz)
{
	switch(khz)
	{
		case 48000:
			if (dir > 0)
				break;
			else
			{
				Cvar_SetValue("s_khz", 44100);
				Cbuf_AddText("snd_restart\n");
			}
			break;
		case 44100:
			if (dir > 0)
			{
				if (havegus != GUS_CLASSIC)
					Cvar_SetValue("s_khz", 48000);
				else
					break;
			}
			else
			{
				Cvar_SetValue("s_khz", 22050);
			}
			Cbuf_AddText("snd_restart\n");
			break;
		case 22050:
			if (dir > 0)
			{
				Cvar_SetValue("s_khz", 44100);
			}
			else
			{
				if(havegus)
				{
					if (havegus == GUS_CLASSIC)
						Cvar_SetValue("s_khz", 19293);
					else
						Cvar_SetValue("s_khz", 11025);
				}
				else
					Cvar_SetValue("s_khz", 11025);
			}
			Cbuf_AddText("snd_restart\n");
			break;
		case 19293:
			if (dir > 0)
			{
				Cvar_SetValue("s_khz", 22050);
			}
			else
			{
				if(havegus != GUS_CLASSIC)
					Cvar_SetValue("s_khz", 11025);
				else
					break;
			}
			Cbuf_AddText("snd_restart\n");
			break;
		case 11025:
			if (dir > 0)
			{
				if(havegus)
				{
					if(havegus == GUS_CLASSIC)
						Cvar_SetValue("s_khz", 19293);
					else
						Cvar_SetValue("s_khz", 22050);
				}
				else
					Cvar_SetValue("s_khz", 22050);
				Cbuf_AddText("snd_restart\n");
			}
			else
			{
				break;
			}
			break;
		default:
			if (dir > 0)
			{
				if(khz < 11025)
					Cvar_SetValue("s_khz", 11025);
				else if(khz > 11025 && khz < 22050)
				{
					if(havegus)
					{
						if(havegus == GUS_CLASSIC)
							Cvar_SetValue("s_khz", 19293);
						else
							Cvar_SetValue("s_khz", 22050);
					}
					else
						Cvar_SetValue("s_khz", 22050);
				}
				else if(khz > 22050 && khz < 44100)
					Cvar_SetValue("s_khz", 44100);
				else if(khz > 48000)
					Cvar_SetValue("s_khz", 48000);
				Cbuf_AddText("snd_restart\n");
			}
			else
			{
				if(khz < 11025)
					Cvar_SetValue("s_khz", 11025);
				else if(khz > 11025 && khz < 22050)
				{
					if(havegus)
					{
						if(havegus == GUS_CLASSIC)
							Cvar_SetValue("s_khz", 19293);
						else
							Cvar_SetValue("s_khz", 11025);
					}
					else
						Cvar_SetValue("s_khz", 11025);
				}
				else if(khz > 22050 && khz < 44100)
					Cvar_SetValue("s_khz", 22050);
				else if(khz > 48000)
					Cvar_SetValue("s_khz", 44100);
				Cbuf_AddText("snd_restart\n");
			}
	}
}

#ifdef GAMESPY /* FS: Gamespy stuff */
#define	NO_SERVER_STRING	"<no server>"
#define MAX_GAMESPY_MENU_SERVERS MAX_SERVERS /* FS: Maximum number of servers to show in the browser */
static char gamespy_server_names[MAX_GAMESPY_MENU_SERVERS][80]; /* FS: GameSpy Browser */
static char gamespy_connect_string[MAX_GAMESPY_MENU_SERVERS][128]; /* FS: GameSpy Browser: Connect string */
static int	m_num_gamespy_servers;
static int	m_num_active_gamespy_servers;
static int	curPageScale;
static int	gspyCurPage;
static int	totalAllowedBrowserPages;
static void JoinGamespyServer_NextPageFunc (void);
static void JoinGamespyServer_PrevPageFunc (void);
static void ConnectGamespyServerFunc(int key);
static void FormatGamespyList (void);
static int Get_Vidscale(void);
static qboolean gamespyInit = true;
static int breakPoint = 0;
int gamespy_cursor;

void Update_Gamespy_Menu(void)
{
	FormatGamespyList();
}

void M_Gamespy_Key(int k)
{
	int vidscale = Get_Vidscale() + 2;
	extern qboolean	keydown[256];

	if(breakPoint > 0)
		vidscale = breakPoint;

	if( k == 'c' ) /* FS: Disconnect */
	{
		if ( keydown[K_CTRL] )
		{
			if(cls.gamespyupdate)
			{
				Cbuf_AddText("gspystop\n");
				return;
			}
		}
	}

	switch (k)
	{
	case K_PGUP:
	case K_KP_PGUP:
		gamespy_cursor = 0;
		JoinGamespyServer_PrevPageFunc();
		break;
	case K_PGDN:
	case K_KP_PGDN:
		gamespy_cursor = 0;
		JoinGamespyServer_NextPageFunc();
		break;
	case K_END:
	case K_KP_END:
		gamespy_cursor = 0;
		gspyCurPage = totalAllowedBrowserPages-1;
		JoinGamespyServer_NextPageFunc();
		break;
	case K_HOME:
	case K_KP_HOME:
		gamespy_cursor = 0;
		gamespyInit = true;
		M_Menu_Gamespy_f();
		break;
	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		gamespy_cursor--;
		if (gamespy_cursor < 0)
			gamespy_cursor = vidscale;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		gamespy_cursor++;
		if (gamespy_cursor > vidscale)
			gamespy_cursor = 0;
		break;

	case K_ESCAPE:
		gamespyInit = true;
		gamespy_cursor = 0;
		M_Menu_Main_f ();
		break;

	case K_ENTER:
		m_entersound = true;

		if(breakPoint > 0) /* FS: Blah, special case hack for the final page, even though not even that many servers exist... */
		{
			if(gamespy_cursor == breakPoint)
				JoinGamespyServer_PrevPageFunc();
			else if (gamespy_cursor == 0)
				SearchGamespyGames();
			else
				ConnectGamespyServerFunc(gamespy_cursor-1);
		}
		else if(gamespy_cursor == 0)
		{
			SearchGamespyGames();
		}
		else if(gamespy_cursor == vidscale - 1)
		{
			JoinGamespyServer_PrevPageFunc();
		}
		else if(gamespy_cursor == vidscale)
		{
			JoinGamespyServer_NextPageFunc();
		}
		else
		{
			ConnectGamespyServerFunc(gamespy_cursor-1);
		}
	}
}

static void ConnectGamespyServerFunc(int key)
{
	char	buffer[128];
	int		index;

	index = key + curPageScale;

	if ( stricmp( gamespy_server_names[index], NO_SERVER_STRING ) == 0 )
	{
		return;
	}

	if (index >= m_num_gamespy_servers)
	{
		return;
	}

	Com_sprintf (buffer, sizeof(buffer), "%s\n", gamespy_connect_string[index]);
	Cbuf_AddText (buffer);
	key_dest = key_console;
	m_state = m_none;
	gamespy_cursor = 0;
	gamespyInit = true;
}

static void FormatGamespyList (void)
{
	int j;
	int skip = 0;

	m_num_gamespy_servers = 0;
	m_num_active_gamespy_servers = 0;

	for (j = 0; j< MAX_SERVERS; j++)
	{
		if (m_num_gamespy_servers == MAX_GAMESPY_MENU_SERVERS)
			break;

		if ((browserList[j].hostname[0] != 0)) /* FS: Only show populated servers first */
		{
			if(browserList[j].curPlayers > 0)
			{
				if(vid.height <= 300) /* FS: Special formatting for low res. */
				{
					char buffer[80];

					Q_strlcpy(gamespy_server_names[m_num_gamespy_servers], browserList[j].hostname, 20);

					if(Q_strlen(browserList[j].hostname) >= 20)
						Q_strlcat(gamespy_server_names[m_num_gamespy_servers], "...", sizeof(gamespy_server_names[0]));

					Com_sprintf(buffer, sizeof(buffer), " [%d] %d/%d", browserList[j].ping, browserList[j].curPlayers, browserList[j].maxPlayers);
					Q_strlcat(gamespy_server_names[m_num_gamespy_servers], buffer, sizeof(gamespy_server_names[0]));
				}
				else
				{
					Com_sprintf(gamespy_server_names[m_num_gamespy_servers], sizeof(gamespy_server_names[0]), "%s [%d] %d/%d", browserList[j].hostname, browserList[j].ping, browserList[j].curPlayers, browserList[j].maxPlayers);
				}
				Com_sprintf(gamespy_connect_string[m_num_gamespy_servers], sizeof(gamespy_connect_string[0]), "connect %s:%d", browserList[j].ip, browserList[j].port);
				m_num_gamespy_servers++;
				m_num_active_gamespy_servers++;
			}
		}
		else
		{
			break;
		}
	}

	for(j = 0; j< MAX_SERVERS; j++)
	{
		if (m_num_gamespy_servers == MAX_GAMESPY_MENU_SERVERS)
			break;

		if ((browserListAll[j].hostname[0] != 0))
		{
			if(vid.height <= 300) /* FS: Special formatting for low res. */
			{
				char buffer[80];

				Q_strlcpy(gamespy_server_names[m_num_gamespy_servers-skip], browserListAll[j].hostname, 20);

				if(Q_strlen(browserListAll[j].hostname) >= 20)
					Q_strlcat(gamespy_server_names[m_num_gamespy_servers-skip], "...", sizeof(gamespy_server_names[0]));

				Com_sprintf(buffer, sizeof(buffer), " [%d] %d/%d", browserListAll[j].ping, browserListAll[j].curPlayers, browserListAll[j].maxPlayers);
				Q_strlcat(gamespy_server_names[m_num_gamespy_servers-skip], buffer, sizeof(gamespy_server_names[0]));
			}
			else
			{
				Com_sprintf(gamespy_server_names[m_num_gamespy_servers-skip], sizeof(gamespy_server_names[0]), "%s [%d] %d/%d", browserListAll[j].hostname, browserListAll[j].ping, browserListAll[j].curPlayers, browserListAll[j].maxPlayers);
			}
			Com_sprintf(gamespy_connect_string[m_num_gamespy_servers-skip], sizeof(gamespy_connect_string[0]), "connect %s:%d", browserListAll[j].ip, browserListAll[j].port);
			m_num_gamespy_servers++;
		}
		else
		{
			skip++;
			continue;
		}
	}
}

static void SearchGamespyGames (void)
{
	int		i;

	m_num_gamespy_servers = 0;

	for (i = 0 ; i < MAX_GAMESPY_MENU_SERVERS; i++)
	{
		Q_strlcpy (gamespy_server_names[i], NO_SERVER_STRING, sizeof(gamespy_server_names[i]));
	}

	// send out info packets
	CL_PingNetServers_f ();
}

static int Get_Vidscale(void)
{
	/* FS: Special function for some what scaling of the server browser depending on video resolution height. */
	if (vid.height <= 240)
		return 18;
	else if (vid.height <= 300)
		return 21;
	else if (vid.height <= 400)
		return 25;
	else if (vid.height <= 480)
		return 30;
	else if (vid.height <= 800)
		return 36;

	return 38;
}

static void JoinGamespyServer_NextPageFunc (void)
{
	int vidscale = Get_Vidscale();

	if((gspyCurPage + 1) > totalAllowedBrowserPages)
	{
		return;
	}

	gamespy_cursor = 0;
	m_state = m_gamespy_pages;
	gspyCurPage++;
	serverscale = vidscale*gspyCurPage;

	curPageScale = serverscale;
}

static void JoinGamespyServer_PrevPageFunc (void)
{
	int vidscale = Get_Vidscale();

	if((gspyCurPage - 1) < 0)
	{
		return;
	}

	gamespy_cursor = 0;
	m_state = m_gamespy_pages;
	gspyCurPage--;
	serverscale = (vidscale*gspyCurPage);

	curPageScale = serverscale;
}

void M_Menu_Gamespy_f(void)
{
	key_dest = key_menu;
	m_state = m_gamespy;
	m_entersound = true;
	gamespyInit = true;
}

void M_Gamespy_Draw(void)
{
	int i, vidscale;
	qpic_t  *p;

	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	M_Print (40, 32, "Query Server List");

	// cursor
	M_DrawCharacter (30, 32 + gamespy_cursor*8, 12+((int)(realtime*4)&1));

	vidscale = Get_Vidscale();

	if(gamespyInit)
	{
		for (i = 0; i < MAX_GAMESPY_MENU_SERVERS; i++)
		{
			Q_strlcpy (gamespy_server_names[i], NO_SERVER_STRING, sizeof(gamespy_server_names[i]));
			memset (&gamespy_connect_string, 0, sizeof(gamespy_connect_string));
		}

		gamespy_cursor = 0;
		gamespyInit = false;
		FormatGamespyList(); /* FS: Incase we changed resolution or ran slist2 in the console and went back to this menu... */
	}

	totalAllowedBrowserPages = (MAX_GAMESPY_MENU_SERVERS/vidscale);

	i = 0;

	for ( i = 0; i < vidscale; i++ )
	{
		M_PrintWhite(40, 40 + i*8, gamespy_server_names[i]);
	}

	i++;

	M_PrintWhite(40, 40 + i*8, "<Next Page>");

	curPageScale = 0;
	gspyCurPage = 0;
	breakPoint = 0;
}

static void JoinGamespyServer_Redraw (int scale)
{
	int i, vidscale;
	qpic_t	*p;
	qboolean didBreak = false;

	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	M_Print (40, 32, "Query Server List");

	// cursor
	M_DrawCharacter (30, 32 + gamespy_cursor*8, 12+((int)(realtime*4)&1));

	vidscale = Get_Vidscale();

	for ( i = 0; i < vidscale; i++ )
	{
		if (i+scale >= MAX_GAMESPY_MENU_SERVERS)
		{
			didBreak = true;
			breakPoint = i+1;
			break;
		}

		M_PrintWhite(40, 40 + i*8, gamespy_server_names[i+scale]);
	}

	if(scale)
	{
		M_PrintWhite(40, 40 + i*8, "<Previous Page>");
	}

	i++;

	if(!didBreak)
	{
		M_PrintWhite(40, 40 + i*8, "<Next Page>");
		breakPoint = 0;
	}
}
#endif

