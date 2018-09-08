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
// gl_vidnt.c -- NT GL vid component

#include "quakedef.h"
#include "winquake.h"
#include "resource.h"
#include "glw_win.h"

#define MAX_MODE_LIST   30
#define VID_ROW_SIZE    3
#define WARP_WIDTH      320
#define WARP_HEIGHT     200
#define MAXWIDTH        10000
#define MAXHEIGHT       10000
#define BASEWIDTH       320
#define BASEHEIGHT      200

#define WINDOW_CLASS_NAME   "GLQDOS for Windows"

#define MODE_WINDOWED           0
#define NO_MODE                 (MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT (MODE_WINDOWED + 1)

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

qboolean DDActive;
qboolean scr_skipupdate;

static DEVMODE gdevmode;
static qboolean windowed, leavecurrentmode;
static qboolean vid_canalttab = false;
static qboolean vid_wassuspended = false;
static int windowed_mouse;
extern qboolean mouseactive;  // from in_win.c
static HICON hIcon;

int texture_extension_number;
glwstate_t glw_state;

int DIBWidth, DIBHeight;

RECT WindowRect;
HWND mainwindow;

int vid_modenum = NO_MODE;
int vid_realmode;
int vid_default = MODE_WINDOWED;
static int windowed_default;
unsigned char vid_curpal[256*3];
static qboolean fullsbardraw = false;

static float vid_gamma = 1.0;

cvar_t *gl_ztrick;
cvar_t *gl_conscale; /* FS */
cvar_t *vid_fullscreen;
cvar_t *vid_xpos;
cvar_t *vid_ypos;

LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

viddef_t vid;                   // global video state

unsigned short d_8to16table[256];
unsigned d_8to24table[256];
unsigned char d_15to8table[65536];

float gldepthmin, gldepthmax;

modestate_t modestate = MS_UNINIT;

void VID_MenuDraw(void);
void VID_MenuKey(int key);

LONG WINAPI MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void AppActivate(BOOL fActive, BOOL minimize);
void ClearAllStates(void);
void VID_UpdateWindowStatus(void);
void GL_Init(void);
static void Check_Gamma(unsigned char *pal);

qboolean is8bit = false;
qboolean gl_mtexable = false;

//====================================

cvar_t      *vid_mode;
cvar_t      *_vid_default_mode;
cvar_t      *_vid_default_mode_win;
cvar_t      *vid_wait;
cvar_t      *vid_nopageflip;
cvar_t      *_vid_wait_override;
cvar_t      *vid_config_x;
cvar_t      *vid_config_y;
cvar_t      *vid_stretch_by_2;
cvar_t      *_windowed_mouse;
cvar_t      *gl_displayrefresh; /* FS: From KMQ2 */

int window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT window_rect;

byte scantokey[128] =
{
//  0           1       2       3       4       5       6       7
//  8           9       A       B       C       D       E       F
	0, 27, '1', '2', '3', '4', '5', '6',
	'7', '8', '9', '0', '-', '=', K_BACKSPACE, 9,                   // 0
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
	'o', 'p', '[', ']', 13, K_CTRL, 'a', 's',                      // 1
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
	'\'', '`', K_SHIFT, '\\', 'z', 'x', 'c', 'v',                      // 2
	'b', 'n', 'm', ',', '.', '/', K_SHIFT, '*',
	K_ALT, ' ', 0, K_F1, K_F2, K_F3, K_F4, K_F5,         // 3
	K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE, 0, K_HOME,
	K_UPARROW, K_PGUP, '-', K_LEFTARROW, '5', K_RIGHTARROW, '+', K_END, //4
	K_DOWNARROW, K_PGDN, K_INS, K_DEL, 0, 0, 0, K_F11,
	K_F12, 0, 0, 0, 0, 0, 0, 0,                                     // 5
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,                                           // 6
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0                                            // 7
};

byte shiftscantokey[128] =
{
//  0           1       2       3       4       5       6       7
//  8           9       A       B       C       D       E       F
	0, 27, '!', '@', '#', '$', '%', '^',
	'&', '*', '(', ')', '_', '+', K_BACKSPACE, 9,                   // 0
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
	'O', 'P', '{', '}', 13, K_CTRL, 'A', 'S',                      // 1
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
	'"', '~', K_SHIFT, '|', 'Z', 'X', 'C', 'V',                      // 2
	'B', 'N', 'M', '<', '>', '?', K_SHIFT, '*',
	K_ALT, ' ', 0, K_F1, K_F2, K_F3, K_F4, K_F5,         // 3
	K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE, 0, K_HOME,
	K_UPARROW, K_PGUP, '_', K_LEFTARROW, '%', K_RIGHTARROW, '+', K_END, //4
	K_DOWNARROW, K_PGDN, K_INS, K_DEL, 0, 0, 0, K_F11,
	K_F12, 0, 0, 0, 0, 0, 0, 0,                                     // 5
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,                                           // 6
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0                                            // 7
};

int MapKey(int key)
{
	key = (key>>16)&255;
	if (key > 127)
		return 0;
	if (scantokey[key] == 0)
		Con_DPrintf(DEVELOPER_MSG_STANDARD, "key 0x%02x has no translation\n", key);
	return scantokey[key];
}

// direct draw software compatability stuff
void VID_HandlePause(qboolean pause) {}
void VID_ForceLockState(int lk) {}
void VID_LockBuffer(void) {}
void VID_UnlockBuffer(void) {}
int VID_ForceUnlockedAndReturnState(void)
{
	return 0;
}
void D_BeginDirectRect(int x, int y, byte *pbitmap, int width, int height) {}
void D_EndDirectRect(int x, int y, int width, int height) {}

/*-----------------------------------------------------------------------*/

void CenterWindow(HWND hWndCenter, int width, int height, BOOL lefttopjustify)
{
	int CenterX, CenterY;

	CenterX = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
	CenterY = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	if (CenterX > CenterY*2)
		CenterX >>= 1;  // dual screens
	CenterX = (CenterX < 0) ? 0 : CenterX;
	CenterY = (CenterY < 0) ? 0 : CenterY;
	SetWindowPos (hWndCenter, NULL, CenterX, CenterY, 0, 0,
	              SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
}

qboolean VID_CreateWindow(int modenum)
{
	WNDCLASS wc;
	HDC hdc;
	int lastmodestate, width, height;
	RECT r;
	unsigned long stylebits, exstyle;

	if (vid_fullscreen->intValue == 1)
	{
		DEVMODE dm;
		memset(&dm, 0, sizeof( dm ) );
		dm.dmSize = sizeof( dm );
		dm.dmPelsWidth  = 640;
		dm.dmPelsHeight = 480;
		dm.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT;
		if (!ChangeDisplaySettings(&dm, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL )
			Sys_Error("Couldn't set CDS");
	}
	else
	{
		ChangeDisplaySettings(0, 0);
	}

	/* Register the frame class */
	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC)MainWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = global_hInstance;
	wc.hIcon         = 0;
	wc.hCursor       = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = (void *)COLOR_GRAYTEXT;
	wc.lpszMenuName  = 0;
	wc.lpszClassName = WINDOW_CLASS_NAME;

	if (!RegisterClass (&wc) )
		Sys_Error ("Couldn't register window class");

	r.top = r.left = 0;

	r.right = 640;
	r.bottom = 480;
	DIBWidth = 640;
	DIBHeight = 480;

	exstyle = 0;
	stylebits = WS_POPUP|WS_VISIBLE;
	if (vid_fullscreen->intValue == 1)
	{
		exstyle = WS_EX_TOPMOST;
	}
	else if (!vid_fullscreen->intValue)
	{
		stylebits = WS_OVERLAPPED|WS_BORDER|WS_CAPTION|WS_VISIBLE|WS_SYSMENU;
	}

	AdjustWindowRect(&r, stylebits, FALSE);

	width = r.right - r.left;
	height = r.bottom - r.top;

	WindowRect = r;

	// Create the DIB window
	glw_state.hWnd = CreateWindowEx (
		exstyle,
		WINDOW_CLASS_NAME,
		WINDOW_CLASS_NAME,
		stylebits,
		0, 0,
		width,
		height,
		NULL,
		NULL,
		global_hInstance,
		NULL);

	if (!glw_state.hWnd)
		Sys_Error ("Couldn't create DIB window2 %d", GetLastError());

	ShowWindow (glw_state.hWnd, SW_SHOWDEFAULT);
	UpdateWindow (glw_state.hWnd);

	// Because we have set the background brush for the window to NULL
	// (to avoid flickering when re-sizing the window on the desktop), we
	// clear the window to black when created, otherwise it will be
	// empty while Quake starts up.
	glw_state.hDC = GetDC(glw_state.hWnd);
	PatBlt(glw_state.hDC, 0, 0, r.right, r.bottom, BLACKNESS);
	ReleaseDC(glw_state.hWnd, glw_state.hDC);

	vid.width = vid.conwidth = width;
	vid.height = vid.conheight = height;

	vid.numpages = 2;
	mainwindow = glw_state.hWnd;

	return true;
}

int VID_SetMode(int modenum, unsigned char *palette)
{
	int original_mode, temp;
	MSG msg;

	// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause ();

	original_mode = windowed_default;

	// Set either the fullscreen or windowed mode
	if (!VID_CreateWindow(modenum))
	{
		Sys_Error ("Couldn't set video mode");
	}

	IN_ActivateMouse ();
	IN_HideMouse ();

	window_width = DIBWidth;
	window_height = DIBHeight;
	VID_UpdateWindowStatus ();

	CDAudio_Resume ();

	scr_disabled_for_loading = temp;
	VID_SetPalette (palette);
	vid_modenum = modenum;
	Cvar_SetValue ("vid_mode", (float)vid_modenum);

	vid.recalc_refdef = 1;

	return true;
}

void VID_UpdateWindowStatus(void)
{
	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	IN_UpdateClipCursor ();
}


//====================================

void GL_Strings_f(void)  /* FS: Print the extensions string */
{
	char seperators[] = " \n";
	char *extString, *p;
	char *savedExtStrings;

	Con_Printf("GL_EXTENSIONS: ");

	savedExtStrings = strdup((char *)gl_extensions);
	extString = strtok_r(savedExtStrings, seperators, &p);

	while(extString != NULL)
	{
		Con_Printf("%s\n", extString);
		extString = strtok_r(NULL, seperators, &p);
	}
	free((void *)savedExtStrings);
}

/*
===============
GL_SetupState -- johnfitz

does all the stuff from GL_Init that needs to be done every time a new GL render context is created
GL_Init will still do the stuff that only needs to be done once
===============
*/
void GL_SetupState(void)
{
	qglClearColor (0.15, 0.15, 0.15, 0); //johnfitz -- originally 1,0,0,0
	qglCullFace(GL_FRONT);
	qglEnable(GL_TEXTURE_2D);

	qglEnable(GL_ALPHA_TEST);
	qglAlphaFunc(GL_GREATER, 0.666);

	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	qglShadeModel (GL_FLAT);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

void GL_Init(void)
{
	gl_vendor = qglGetString (GL_VENDOR);
	Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = (const char *)qglGetString (GL_RENDERER);
	Con_Printf ("GL_RENDERER: %s\n", gl_renderer);

	gl_version = (const char *)qglGetString (GL_VERSION);
	Con_Printf ("GL_VERSION: %s\n", gl_version);
	gl_extensions = (const char *)qglGetString (GL_EXTENSIONS);
	Con_SafeDPrintf (DEVELOPER_MSG_VIDEO, "GL_EXTENSIONS: %s\n", gl_extensions);

	if(strstr(gl_vendor, "Intel") && !COM_CheckParm("-overrideintel")) /* FS: Intel integrated graphics come to a complete crawl with any dlights + multitexturing.  Therefore, explicitly request it. */
		Con_Warning("Intel Graphics detected.  Skipping Mulitexture initialization.\n");

	GL_SetupState(); // johnfitz
}

/*
=================
GL_BeginRendering -- sets values of glx, gly, glwidth, glheight
=================
*/
extern void GL_ClearTextureCache (void);
void GL_BeginRendering(int *x, int *y, int *width, int *height)
{
	if (vid_fullscreen->modified)
	{
		vid_fullscreen->modified = false;

		VID_Shutdown();
		R_Shutdown();

		VID_SetMode (vid_default, host_basepal);

		glw_state.hDC = GetDC(glw_state.hWnd);
		bSetupPixelFormat(glw_state.hDC);

		glw_state.hGLRC = qwglCreateContext(glw_state.hDC);
		if (!glw_state.hGLRC)
			Sys_Error ("Could not initialize GL (wglCreateContext failed).");
		if (!qwglMakeCurrent(glw_state.hDC, glw_state.hGLRC))
			Sys_Error ("qwglMakeCurrent failed");

		R_Restart();
	}

	*x = *y = 0;
	*width = WindowRect.right - WindowRect.left;
	*height = WindowRect.bottom - WindowRect.top;
}

void GL_EndRendering(void)
{
	if (!scr_skipupdate || block_drawing)
		SwapBuffers(glw_state.hDC);

// handle the mouse state when windowed if that's changed
	if (modestate == MS_WINDOWED)
	{
		if (!_windowed_mouse->value) {
			if (windowed_mouse) {
				IN_DeactivateMouse ();
				IN_ShowMouse ();
				windowed_mouse = false;
			}
		} else {
			windowed_mouse = true;
			if (key_dest == key_game && !mouseactive && ActiveApp) {
				IN_ActivateMouse ();
				IN_HideMouse ();
			} else if (mouseactive && key_dest != key_game) {
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}
		}
	}
	if (fullsbardraw)
		Sbar_Changed();
}

void VID_SetPalette(unsigned char *palette)
{
	byte    *pal;
	unsigned r, g, b;
	unsigned v;
	int r1, g1, b1;
	int j, k, l;
	unsigned short i;
	unsigned    *table;

//
// 8 8 8 encoding
//
	pal = palette;
	table = d_8to24table;
	for (i=0; i<256; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;

		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		*table++ = v;
	}
	d_8to24table[255] &= 0xffffff;  // 255 is transparent

	// JACK: 3D distance calcs - k is last closest, l is the distance.
	// FIXME: Precalculate this and cache to disk.
	for (i=0; i < (1<<15); i++) {
		/* Maps
		    000000000000000
		    000000000011111 = Red  = 0x1F
		    000001111100000 = Blue = 0x03E0
		    111110000000000 = Grn  = 0x7C00
		 */
		r = ((i & 0x1F) << 3)+4;
		g = ((i & 0x03E0) >> 2)+4;
		b = ((i & 0x7C00) >> 7)+4;
		pal = (unsigned char *)d_8to24table;
		for (v=0, k=0, l=10000*10000; v<256; v++, pal+=4) {
			r1 = r-pal[0];
			g1 = g-pal[1];
			b1 = b-pal[2];
			j = (r1*r1)+(g1*g1)+(b1*b1);
			if (j<l) {
				k=v;
				l=j;
			}
		}
		d_15to8table[i]=k;
	}
}

BOOL gammaworks;

void VID_ShiftPalette(unsigned char *palette)
{
	extern byte ramps[3][256];

//	VID_SetPalette (palette);

//	gammaworks = SetDeviceGammaRamp (maindc, ramps);
}

void VID_SetDefaultMode(void)
{
	IN_DeactivateMouse ();
}

void VID_Shutdown(void)
{
	if ( qwglMakeCurrent && !qwglMakeCurrent( NULL, NULL ) )
	{
		Con_Printf("VID_Shutdown - wglMakeCurrent failed\n");
	}

	if ( glw_state.hGLRC )
	{
		if (  qwglDeleteContext && !qwglDeleteContext( glw_state.hGLRC ) )
		{
			Con_Printf("VID_Shutdown - wglDeleteContext failed\n");
		}
		glw_state.hGLRC = NULL;
	}
	if (glw_state.hDC)
	{
		if ( !ReleaseDC( glw_state.hWnd, glw_state.hDC ) )
			Con_Printf("VID_Shutdown - ReleaseDC failed\n" );
		glw_state.hDC   = NULL;
	}
	if (glw_state.hWnd)
	{
		ShowWindow( glw_state.hWnd, SW_HIDE );
		DestroyWindow (	glw_state.hWnd );
		glw_state.hWnd = NULL;
	}

	if ( glw_state.log_fp )
	{
		fclose( glw_state.log_fp );
		glw_state.log_fp = 0;
	}

	UnregisterClass (WINDOW_CLASS_NAME, glw_state.hInstance);

	AppActivate(false, false);
}

//==========================================================================

qboolean bSetupPixelFormat(HDC hDC)
{
	int pixelformat;
	memset(&glw_state.pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));

	glw_state.pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	glw_state.pfd.nVersion = 1;
	glw_state.pfd.dwFlags = PFD_DRAW_TO_WINDOW |            // support window
	                        PFD_SUPPORT_OPENGL | // support OpenGL
	                        PFD_DOUBLEBUFFER;   // double buffered

	glw_state.pfd.iPixelType = PFD_TYPE_RGBA;
	glw_state.pfd.cDepthBits = 24;  // Knightmare changed 2/22/13, was 32
	glw_state.pfd.iLayerType = PFD_MAIN_PLANE;

	if ((pixelformat = ChoosePixelFormat(hDC, &glw_state.pfd)) == 0)
	{
		MessageBox(NULL, "ChoosePixelFormat failed", "Error", MB_OK);
		return FALSE;
	}

	if (SetPixelFormat(hDC, pixelformat, &glw_state.pfd) == FALSE)
	{
		MessageBox(NULL, "SetPixelFormat failed", "Error", MB_OK);
		return FALSE;
	}

	return TRUE;
}

void ClearAllStates(void)
{
	int i;

// send an up event for each key, to make sure the server clears them all
	for (i=0; i<256; i++)
	{
		Key_Event (i, false);
	}

	Key_ClearStates ();
	IN_ClearStates ();
}

void AppActivate(BOOL fActive, BOOL minimize)
{
	static BOOL sound_active;

	ActiveApp = fActive;
	Minimized = minimize;

// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active)
	{
		S_BlockSound ();
		sound_active = false;
	}
	else if (ActiveApp && !sound_active)
	{
		S_UnblockSound ();
		sound_active = true;
	}

	if (fActive)
	{
		if (modestate == MS_FULLDIB)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
			if (vid_canalttab && vid_wassuspended) {
				vid_wassuspended = false;
				ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN);
				ShowWindow(mainwindow, SW_SHOWNORMAL);
			}
		}
		else if ((modestate == MS_WINDOWED) && _windowed_mouse->value && key_dest == key_game)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
	}

	if (!fActive)
	{
		if (modestate == MS_FULLDIB)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			if (vid_canalttab) {
				ChangeDisplaySettings (NULL, 0);
				vid_wassuspended = true;
			}
		}
		else if ((modestate == MS_WINDOWED) && _windowed_mouse->value)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
	}
}

/* main window procedure */
LONG WINAPI MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int fActive, fMinimized, temp;
//	extern unsigned int uiWheelMessage;

//	if ( uMsg == uiWheelMessage )
//		uMsg = WM_MOUSEWHEEL;

	switch (uMsg)
	{
	case WM_CREATE:
		mainwindow = hWnd;

//		MSH_MOUSEWHEEL = RegisterWindowMessage("MSWHEEL_ROLLMSG"); 
        return DefWindowProc (hWnd, uMsg, wParam, lParam);

	case WM_MOVE:
		{
			int		xPos, yPos;
			RECT r;
			int		style;

			if (!vid_fullscreen->value)
			{
				xPos = (short) LOWORD(lParam);    // horizontal position 
				yPos = (short) HIWORD(lParam);    // vertical position 

				r.left   = 0;
				r.top    = 0;
				r.right  = 1;
				r.bottom = 1;

				style = GetWindowLong( hWnd, GWL_STYLE );
				AdjustWindowRect( &r, style, FALSE );

				Cvar_SetValue( "vid_xpos", xPos + r.left);
				Cvar_SetValue( "vid_ypos", yPos + r.top);
				vid_xpos->modified = false;
				vid_ypos->modified = false;
				if (ActiveApp)
					IN_ActivateMouse ();
			}
		}
        return DefWindowProc (hWnd, uMsg, wParam, lParam);

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		Key_Event (MapKey(lParam), true);
		break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		Key_Event (MapKey(lParam), false);
		break;

	case WM_SYSCHAR:
		// keep Alt-Space from happening
		break;

	// this is complicated because Win32 seems to pack multiple mouse events into
	// one update sometimes, so we always check all states and look for events
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEMOVE:
		temp = 0;

		if (wParam & MK_LBUTTON)
			temp |= 1;

		if (wParam & MK_RBUTTON)
			temp |= 2;

		if (wParam & MK_MBUTTON)
			temp |= 4;

		IN_MouseEvent (temp);

		break;

	// JACK: This is the mouse wheel with the Intellimouse
	// Its delta is either positive or neg, and we generate the proper
	// Event.
	case WM_MOUSEWHEEL:
		if ((short) HIWORD(wParam) > 0) {
			Key_Event(K_MWHEELUP, true);
			Key_Event(K_MWHEELUP, false);
		} else {
			Key_Event(K_MWHEELDOWN, true);
			Key_Event(K_MWHEELDOWN, false);
		}
		break;

	case WM_SIZE:
		break;

	case WM_CLOSE:
		if (MessageBox (mainwindow, "Are you sure you want to quit?", "Confirm Exit",
		                MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
		{
			Sys_Quit ();
		}

		break;

	case WM_ACTIVATE:
		fActive = LOWORD(wParam);
		fMinimized = (BOOL) HIWORD(wParam);
		AppActivate(!(fActive == WA_INACTIVE), fMinimized);

		// fix the leftover Alt from any Alt-Tab or the like that switched us away
		ClearAllStates ();

		break;

	case WM_DESTROY:
		// let sound and input know about this?
		mainwindow = NULL;
        return DefWindowProc (hWnd, uMsg, wParam, lParam);

	case MM_MCINOTIFY:
		return CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);

	default:
		/* pass all unhandled messages to DefWindowProc */
		return DefWindowProc (hWnd, uMsg, wParam, lParam);
	}

    /* return 0 if handled message, 1 if not */
    return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

static void Check_Gamma(unsigned char *pal)
{
	float f, inf;
	unsigned char palette[768];
	int i;

	if ((i = COM_CheckParm("-gamma")) == 0) {
		if ((gl_renderer && strstr(gl_renderer, "Voodoo")) ||
		    (gl_vendor && strstr(gl_vendor, "3Dfx")))
			vid_gamma = 1;
		else
			vid_gamma = 0.7; // default to 0.7 on non-3dfx hardware
	} else
		vid_gamma = Q_atof(com_argv[i+1]);

	for (i=0; i<768; i++)
	{
		f = pow ( (pal[i]+1)/256.0, vid_gamma);
		inf = f*255 + 0.5;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		palette[i] = inf;
	}

	memcpy (pal, palette, sizeof(palette));
}

void VID_Init(unsigned char *palette)
{
	int i, existingmode;
	int basenummodes, width, height, bpp, findbpp, done;
	char gldir[MAX_OSPATH];
	HDC hdc;
	DEVMODE devmode;

	width = 640;
	height = 480;

	memset(&devmode, 0, sizeof(devmode));

	vid_mode = Cvar_Get("vid_mode", "0", CVAR_ARCHIVE);
	_vid_default_mode = Cvar_Get("_vid_default_mode", "0", CVAR_ARCHIVE); // Note that 0 is MODE_WINDOWED
	_vid_default_mode_win = Cvar_Get("_vid_default_mode_win", "3", CVAR_ARCHIVE); // Note that 3 is MODE_FULLSCREEN_DEFAULT
	vid_wait = Cvar_Get("vid_wait", "0", 0);
	vid_nopageflip = Cvar_Get("vid_nopageflip", "0", CVAR_ARCHIVE);
	_vid_wait_override = Cvar_Get("_vid_wait_override", "0", CVAR_ARCHIVE);
	vid_config_x = Cvar_Get("vid_config_x", "800", CVAR_ARCHIVE);
	vid_config_y = Cvar_Get("vid_config_y", "600", CVAR_ARCHIVE);
	vid_stretch_by_2 = Cvar_Get("vid_stretch_by_2", "1", CVAR_ARCHIVE);
	_windowed_mouse = Cvar_Get("_windowed_mouse", "1", CVAR_ARCHIVE);
	gl_ztrick = Cvar_Get("gl_ztrick", "1", 0);
	gl_ztrick->description = "Toggles the use of a trick to prevent the clearing of the z-buffer between frames. When this variable is set to 1 the game will not clear the z-buffer between frames. This will result in increased performance but might cause problems for some display hardware.";
	gl_displayrefresh = Cvar_Get("gl_displayrefresh", "0", CVAR_ARCHIVE);
	gl_displayrefresh->description = "Refresh rate for fullscreen modes.  Set to 0 to disable.";
	gl_conscale = Cvar_Get("gl_conscale", "1", CVAR_ARCHIVE);
	gl_conscale->description = "Set to 0 to make the console width and height equal to the current resolution.  Set to 1 to control it with conwidth and conheight cmdline.  Requires game restart.";
	vid_fullscreen = Cvar_Get("vid_fullscreen", "2", CVAR_ARCHIVE);
	vid_fullscreen->modified = false;
	vid_xpos = Cvar_Get("vid_xpos", "0", CVAR_ARCHIVE);
	vid_ypos = Cvar_Get("vid_ypos", "0", CVAR_ARCHIVE);

	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_ICON2));

	if (!QGL_Init("opengl32"))
		Sys_Error("Opengl32 init failed!");

	if (!gl_conscale->intValue) /* FS */
	{
		vid.conwidth = width;
		vid.conwidth &= 0xfff8; // make it a multiple of eight

		vid.conheight = height;
		vid.conheight = vid.conwidth*3 / 4;
	}
	else
	{
		if ((i = COM_CheckParm("-conwidth")) != 0)
			vid.conwidth = Q_atoi(com_argv[i+1]);
		else
			vid.conwidth = width; /* FS: Was 640 */

		vid.conwidth &= 0xfff8; // make it a multiple of eight

		if (vid.conwidth < 320)
			vid.conwidth = 320;

		// pick a conheight that matches with correct aspect
		vid.conheight = vid.conwidth*3 / 4;

		if ((i = COM_CheckParm("-conheight")) != 0)
			vid.conheight = Q_atoi(com_argv[i+1]);
		if (vid.conheight < 200)
			vid.conheight = 200;
	}

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	DestroyWindow (hwnd_dialog);

	Check_Gamma(palette);
	VID_SetPalette (palette);

	VID_SetMode (vid_default, palette);

	glw_state.hDC = GetDC(glw_state.hWnd);
	bSetupPixelFormat(glw_state.hDC);

	glw_state.hGLRC = qwglCreateContext(glw_state.hDC);
	if (!glw_state.hGLRC)
		Sys_Error ("Could not initialize GL (wglCreateContext failed).");

	if (!qwglMakeCurrent)
		Sys_Error("Init wrong dumbass");

	if (!qwglMakeCurrent(glw_state.hDC, glw_state.hGLRC))
		Sys_Error ("qwglMakeCurrent failed");

	GL_Init();

	sprintf (gldir, "%s/glquake", com_gamedir);
	Sys_mkdir (gldir);

	vid_realmode = vid_modenum;

	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	vid_canalttab = true;
	vid_fullscreen->modified = false;
}


//========================================================
// Video menu stuff
//========================================================

extern void M_Menu_Options_f(void);
extern void M_Print(int cx, int cy, char *str);
extern void M_PrintWhite(int cx, int cy, char *str);
extern void M_DrawCharacter(int cx, int line, int num);
extern void M_DrawTransPic(int x, int y, qpic_t *pic);
extern void M_DrawPic(int x, int y, qpic_t *pic);

#define MAX_COLUMN_SIZE     9
#define MODE_AREA_HEIGHT    (MAX_COLUMN_SIZE + 2)
#define MAX_MODEDESCS       (MAX_COLUMN_SIZE*3)

void VID_MenuDraw(void)
{
	qpic_t      *p;

	p = Draw_CachePic ("gfx/vidmodes.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	M_Print (3*8, 36 + MODE_AREA_HEIGHT * 8 + 8*2,
	         "Video modes must be set from the");
	M_Print (3*8, 36 + MODE_AREA_HEIGHT * 8 + 8*3,
	         "command line with -width <width>");
	M_Print (3*8, 36 + MODE_AREA_HEIGHT * 8 + 8*4,
	         "and -bpp <bits-per-pixel>");
	M_Print (3*8, 36 + MODE_AREA_HEIGHT * 8 + 8*6,
	         "Select windowed mode with -window");
}

void VID_MenuKey(int key)
{
	switch (key)
	{
	case K_ESCAPE:
		S_LocalSound ("misc/menu1.wav");
		M_Menu_Options_f ();
		break;

	default:
		break;
	}
}

qboolean VID_Is8bit(void)
{
	return is8bit;
}
