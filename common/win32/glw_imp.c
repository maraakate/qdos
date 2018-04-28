/*
Copyright (C) 1997-2001 Id Software, Inc.

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
/*
** GLW_IMP.C
**
** This file contains ALL Win32 specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_Shutdown
** GLimp_SwitchFullscreen
**
*/
#include <assert.h>
#include <windows.h>
#include "quakedef.h"
#include <GL/wglext.h>
#include "glw_win.h"
#include "winquake.h"

/*static qboolean GLimp_SwitchFullscreen(int width, int height);*/
qboolean GLimp_InitGL (void);

glwstate_t glw_state;

extern cvar_t *vid_fullscreen;
extern cvar_t *vid_ref;

// Knightmare- added Vic's hardware gammaramp
WORD original_ramp[3][256];	// Knightmare- hardware gamma 
WORD gamma_ramp[3][256];

/* FS: Control Anti-Aliasing */
cvar_t *gl_multisample;
cvar_t *gl_multisample_softrestart;
static int glPixelFormatMSAA;
static int glMultisample = GL_MULTISAMPLE;
static qboolean s_win95 = false;

static int Get_MultiSample (void)
{
	if (gl_multisample->intValue)
	{
		Con_Printf("wglChoosePixelFormatARB");
		qwglChoosePixelFormatARB = (BOOL (WINAPI *)(HDC, const int*, const float*, unsigned int, int*, unsigned int*)) qwglGetProcAddress("wglChoosePixelFormatARB");
		if(qwglChoosePixelFormatARB)
		{
			int pixelFormat = 0;
			int msaaMode = 0;
			UINT numFormats = 0;
			BOOL bSuccess = false;

			int pAttribListi[] =
			{
				WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
				WGL_SAMPLES_ARB, 2,
				WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
				WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
				WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
				WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
				WGL_COLOR_BITS_ARB, 32,
				WGL_DEPTH_BITS_ARB, 24,
				WGL_STENCIL_BITS_ARB, 8,
				0, 0
			};

			if (strstr((const char *)qglGetString(GL_RENDERER), "Voodoo")
				&& strstr((const char *)qglGetString(GL_VENDOR), "3Dfx Interactive Inc."))
			{
				pAttribListi[0] = WGL_SAMPLE_BUFFERS_3DFX;
				pAttribListi[2] = WGL_SAMPLES_3DFX;
				/* FS: Don't change the Pixel Type or Color Bits for 3DFX */
				pAttribListi[10] = pAttribListi[11] = pAttribListi[12] = pAttribListi[13] = 0;
				glMultisample = GL_MULTISAMPLE_3DFX;

				switch(gl_multisample->intValue)
				{
				case 2:
					pAttribListi[3] = 2;
					msaaMode = 2;
					break;
				case 4:
					pAttribListi[3] = 4;
					msaaMode = 4;
					break;
				case 8: /* FS: Voodoo 5 6000 and maybe AAlchemy */
					pAttribListi[3] = 8;
					break;
				default:
					goto failed;
					break;
				}
			}
			else
			{
				switch(gl_multisample->intValue)
				{
				case 2:
					pAttribListi[3] = 2;
					msaaMode = 2;
					break;
				case 4:
					pAttribListi[3] = 4;
					msaaMode = 4;
					break;
				case 8:
					pAttribListi[3] = 8;
					msaaMode = 8;
					break;
				case 16:
					pAttribListi[3] = 16;
					msaaMode = 16;
					break;
				default:
					goto failed;
					break;
				}
			}

			bSuccess = qwglChoosePixelFormatARB(glw_state.hDC, pAttribListi, NULL, 1, &pixelFormat, &numFormats);
			if(bSuccess)
			{
				if (pixelFormat)
				{
					Con_Printf(" ...using multisamples %dxAA.\n", msaaMode);
					return pixelFormat;
				}
			}
		}
failed:
		Cvar_Set("gl_multisample", "0");
		gl_multisample->modified = false;
		Con_Printf(" ...Failed.\n");
	}

	return 0;
}

static void HWGamma_Check3dfxGamma (void)
{
	const char *extensions = (const char *) qglGetString(GL_EXTENSIONS);
	if (strstr(extensions, "WGL_3DFX_gamma_control")) {
		if (!r_ignorehwgamma->value) {
			qwglGetDeviceGammaRamp3DFX = (BOOL (WINAPI *)(HDC, LPVOID)) qwglGetProcAddress("wglGetDeviceGammaRamp3DFX");
			qwglSetDeviceGammaRamp3DFX = (BOOL (WINAPI *)(HDC, LPVOID)) qwglGetProcAddress("wglSetDeviceGammaRamp3DFX");
			if (qwglGetDeviceGammaRamp3DFX && qwglSetDeviceGammaRamp3DFX)
				Con_Printf("Found WGL_3DFX_gamma_control\n");
			else {
				qwglGetDeviceGammaRamp3DFX = NULL;
				qwglSetDeviceGammaRamp3DFX = NULL;
				Con_Printf( "Failed loading WGL_3DFX_gamma_control\n");
			}
		}
		else
			Con_Printf( "Ignoring WGL_3DFX_gamma_control\n");
	}
	else
		Con_Printf("WGL_3DFX_gamma_control not found\n");
}

void InitGammaRamp (void)
{
	HWGamma_Check3dfxGamma ();

	if (!r_ignorehwgamma->value)
	{
		if (qwglGetDeviceGammaRamp3DFX)
			gl_state.gammaRamp = qwglGetDeviceGammaRamp3DFX (glw_state.hDC, original_ramp);
		else
			gl_state.gammaRamp = GetDeviceGammaRamp ( glw_state.hDC, original_ramp );
		if (gl_state.gammaRamp)
			vid_gamma->modified = true;
	}
	else
		gl_state.gammaRamp = false;
}

void ShutdownGammaRamp (void)
{
	if (!glw_state.hDC)
		return;
	if (!gl_state.gammaRamp)
		return;
	if (qwglSetDeviceGammaRamp3DFX)
		qwglSetDeviceGammaRamp3DFX (glw_state.hDC, original_ramp);
	else
		SetDeviceGammaRamp (glw_state.hDC, original_ramp);
}

void UpdateGammaRamp (void)
{
	int	i, j, p;
	float	g;

	if (!gl_state.gammaRamp)
		return;
	g = vid_gamma->value/* + 0.3f*/; /* FS: Just use the direct gamma value, this is fucking up hardware gamma and non-hardware gamma being unbalanced. */
	memcpy (gamma_ramp, original_ramp, sizeof(original_ramp));
	for (i = 0; i < 3; i++)
	{
		for (j = 0; j < 256; j++)
		{
			p = 255 * pow(((float)j + 0.5f) / 255.5f, g) + 0.5f;
			if (p < 0) p = 0;
			else if (p > 255)
				p = 255;
			gamma_ramp[i][j] = ((WORD) p) << 8;
		}
	}
	if (qwglSetDeviceGammaRamp3DFX)
		qwglSetDeviceGammaRamp3DFX (glw_state.hDC, gamma_ramp);
	else
		SetDeviceGammaRamp ( glw_state.hDC, gamma_ramp );
}
// end Vic's hardware gammaramp

// Knightmare
void HWGamma_Toggle (qboolean enable)
{
	if (!gl_state.gammaRamp)
		return;

	if(!glw_state.hDC) /* FS: This can actually happen with the 3DFX OpenGL ICD on Win98 during a vid_restart */
		return;

	if (enable)
	{
		if (qwglSetDeviceGammaRamp3DFX)
			qwglSetDeviceGammaRamp3DFX (glw_state.hDC, gamma_ramp);
		else
			SetDeviceGammaRamp (glw_state.hDC, gamma_ramp);
	}
	else
	{
		if (qwglSetDeviceGammaRamp3DFX)
			qwglSetDeviceGammaRamp3DFX (glw_state.hDC, original_ramp);
		else
			SetDeviceGammaRamp (glw_state.hDC, original_ramp);
	}
}
// end Knightmare

static qboolean VerifyDriver( void )
{
	char buffer[1024];

	strcpy( buffer, (const char *) qglGetString( GL_RENDERER ) );
	strlwr( buffer );
	if ( strcmp( buffer, "gdi generic" ) == 0 )
		if ( !glw_state.mcd_accelerated )
			return false;
	return true;
}

/*
** VID_CreateWindow
*/
#define	WINDOW_CLASS_NAME	"GLQuake"

qboolean VID_CreateWindow( int width, int height, qboolean fullscreen )
{
	WNDCLASS		wc;
	RECT			r;
	cvar_t			*vid_xpos, *vid_ypos;
	int				stylebits;
	int				x, y, w, h;
	int				exstyle;

	/* Register the frame class */
    wc.style         = 0;
    wc.lpfnWndProc   = (WNDPROC)glw_state.wndproc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = glw_state.hInstance;
    wc.hIcon         = 0;
    wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = (void *)COLOR_GRAYTEXT;
    wc.lpszMenuName  = 0;
    wc.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClass (&wc) )
		Sys_Error ("Couldn't register window class");

	if (fullscreen)
	{
		exstyle = WS_EX_TOPMOST;
		stylebits = WS_POPUP|WS_VISIBLE;
	}
	else if (vid_fullscreen->intValue > 1) /* FS: Borderless windows */
	{
		exstyle = 0;
		stylebits = WS_POPUP|WS_VISIBLE;
	}
	else
	{
		exstyle = 0;
		stylebits = WINDOW_STYLE;
	}

	r.left = 0;
	r.top = 0;
	r.right  = width;
	r.bottom = height;

	AdjustWindowRect (&r, stylebits, FALSE);

	w = r.right - r.left;
	h = r.bottom - r.top;

	if (fullscreen)
	{
		x = 0;
		y = 0;
	}
	else if(vid_fullscreen->intValue >= 2) /* FS: Borderless windows */
	{
		HDC hDC = GetDC( NULL );
		int nHorzRes = GetDeviceCaps( hDC, HORZRES );
		int nVertRes = GetDeviceCaps( hDC, VERTRES );
		ReleaseDC( 0, hDC );

		if (nHorzRes <= vid.width || nVertRes <= vid.height)
		{
			x = y = 0;
		}
		else
		{
			x = ( nHorzRes - vid.width ) / 2;
			y = ( nVertRes - vid.height ) / 2;
		}
	}
	else
	{
		vid_xpos = Cvar_Get ("vid_xpos", "0", 0);
		vid_ypos = Cvar_Get ("vid_ypos", "0", 0);
		x = vid_xpos->value;
		y = vid_ypos->value;
	}

	mainwindow = CreateWindowEx (
		 exstyle, 
		 WINDOW_CLASS_NAME,
		 "GLQuake",
		 stylebits,
		 x, y, w, h,
		 NULL,
		 NULL,
		 glw_state.hInstance,
		 NULL);

	if (!mainwindow)
		Sys_Error ("Couldn't create window");
	
	ShowWindow( mainwindow, SW_SHOW );
	UpdateWindow( mainwindow );

	// init all the gl stuff for the window
	if (!GLimp_InitGL ())
	{
		Con_Printf("VID_CreateWindow() - GLimp_InitGL failed\n");
		return false;
	}

	SetForegroundWindow( mainwindow );
	SetFocus( mainwindow );

	// let the sound and input subsystems know about the new window
	ri.Vid_NewWindow (width, height);

	return true;
}

/* FS: Yes this is basically VID_CreateWindow and GLimp_Shutdown, except we don't completely destroy the window.
 *     This works around re-init twice on WinNT.
 */
static void VID_SoftRestart (void)
{
    PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
		1,								// version number
		PFD_DRAW_TO_WINDOW |			// support window
		PFD_SUPPORT_OPENGL |			// support OpenGL
		PFD_DOUBLEBUFFER,				// double buffered
		PFD_TYPE_RGBA,					// RGBA type
		// Knightmare- was 24-bit, changed for stencil buffer
		32,								// 32-bit color depth
		0, 0, 0, 0, 0, 0,				// color bits ignored
		0,								// no alpha buffer
		0,								// shift bit ignored
		0,								// no accumulation buffer
		0, 0, 0, 0, 					// accum bits ignored
		// Knightmare- stencil buffer, was 32-bit z-buffer, no stencil buffer
		24,								// 24-bit z-buffer
		8,								// 8-bit stencil buffer
		0,								// no auxiliary buffer
		PFD_MAIN_PLANE,					// main layer
		0,								// reserved
		0, 0, 0							// layer masks ignored
    };
	cvar_t *vid_xpos, *vid_ypos;
	unsigned long exstyle;
	unsigned long stylebits;
	int x, y;

	if (qwglMakeCurrent && !qwglMakeCurrent(NULL, NULL))
		Con_Printf("ref_gl::R_Shutdown() - wglMakeCurrent failed\n");

	if (glw_state.hGLRC)
	{
		if ( qwglDeleteContext && !qwglDeleteContext( glw_state.hGLRC ) )
			Con_Printf("ref_gl::R_Shutdown() - wglDeleteContext failed\n");
		glw_state.hGLRC = NULL;
	}

	if (glw_state.hDC)
	{
		if (!ReleaseDC(mainwindow, glw_state.hDC))
			Con_Printf("ref_gl::R_Shutdown() - ReleaseDC failed\n" );
		glw_state.hDC = NULL;
	}

	if (mainwindow)
	{
		DestroyWindow ( mainwindow );
		mainwindow = NULL;
	}

	if (vid_fullscreen->value)
	{
		if (vid_fullscreen->intValue >= 2) /* FS: Borderless windows */
			exstyle = 0;
		else
			exstyle = WS_EX_TOPMOST;
		stylebits = WS_POPUP|WS_VISIBLE;
	}
	else
	{
		exstyle = 0;
		stylebits = WINDOW_STYLE;
	}

	if (vid_fullscreen->intValue == 1)
	{
		x = 0;
		y = 0;
	}
	else if(vid_fullscreen->intValue >= 2) /* FS: Borderless windows */
	{
		HDC hDC = GetDC( NULL );
		int nHorzRes = GetDeviceCaps( hDC, HORZRES );
		int nVertRes = GetDeviceCaps( hDC, VERTRES );
		ReleaseDC( 0, hDC );

		if (nHorzRes <= vid.width || nVertRes <= vid.height)
		{
			x = y = 0;
		}
		else
		{
			x = ( nHorzRes - vid.width ) / 2;
			y = ( nVertRes - vid.height ) / 2;
		}
	}
	else
	{
		vid_xpos = Cvar_Get ("vid_xpos", "0", 0);
		vid_ypos = Cvar_Get ("vid_ypos", "0", 0);
		x = vid_xpos->value;
		y = vid_ypos->value;
	}

	mainwindow = CreateWindowEx (
		 exstyle,
		 WINDOW_CLASS_NAME,
		 "Quake 2",
		 stylebits,
		 x, y, vid.width, vid.height,
		 NULL,
		 NULL,
		 glw_state.hInstance,
		 NULL);

	if (!mainwindow)
		ri.Sys_Error (ERR_FATAL, "Couldn't create window");

	ShowWindow( mainwindow, SW_SHOW );
	UpdateWindow( mainwindow );

	if ((glw_state.hDC = GetDC(mainwindow)) == NULL)
	{
		ri.Sys_Error( ERR_FATAL, "RImp_Init() - GetDC failed\n" );
	}

	SetPixelFormat(glw_state.hDC, glPixelFormatMSAA, &pfd);
	DescribePixelFormat(glw_state.hDC, glPixelFormatMSAA, sizeof(pfd), &pfd);
	if ((glw_state.hGLRC = qwglCreateContext(glw_state.hDC)) == 0)
	{
		ri.Sys_Error (ERR_FATAL, "RImp_Init() - qwglCreateContext failed\n");
	}

	if (!qwglMakeCurrent(glw_state.hDC, glw_state.hGLRC))
	{
		ri.Sys_Error (ERR_FATAL, "RImp_Init() - qwglMakeCurrent failed\n");
	}

	SetForegroundWindow( mainwindow );
	SetFocus( mainwindow );

	// let the sound and input subsystems know about the new window
	ri.Vid_NewWindow (vid.width, vid.height);
}

/*
** GLimp_SetMode
*/
rserr_t GLimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen )
{
	int width, height;
	const char *win_fs[] = { "W", "FS", "BL" };

	Con_Printf("Initializing OpenGL display\n");

	Con_Printf("...setting mode %d:", mode );

	if ( !ri.Vid_GetModeInfo( &width, &height, mode ) )
	{
		Con_Printf(" invalid mode\n" );
		return rserr_invalid_mode;
	}

	Con_Printf(" %d %d %s\n", width, height, win_fs[fullscreen] );

	// destroy the existing window
	if (mainwindow)
	{
		GLimp_Shutdown ();
	}

	// do a CDS if needed
	if ( fullscreen && vid_fullscreen->intValue == 1) /* FS: Borderless windows */
	{
		DEVMODE dm;

		Con_Printf("...attempting fullscreen\n" );

		memset( &dm, 0, sizeof( dm ) );

		dm.dmSize = sizeof( dm );

		dm.dmPelsWidth  = width;
		dm.dmPelsHeight = height;
		dm.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT;

		// Knightmare- added refresh rate control
		if ( r_displayrefresh->value != 0 )
		{
			dm.dmDisplayFrequency = (int)r_displayrefresh->value;
			dm.dmFields |= DM_DISPLAYFREQUENCY;
			Con_Printf("...using r_displayrefresh of %d\n", (int)r_displayrefresh->value );
		}

		if ( gl_bitdepth->value != 0 )
		{
			dm.dmBitsPerPel = gl_bitdepth->value;
			dm.dmFields |= DM_BITSPERPEL;
			Con_Printf("...using gl_bitdepth of %d\n", ( int ) gl_bitdepth->value );
		}
		else
		{
			HDC hdc = GetDC( NULL );
			int bitspixel = GetDeviceCaps( hdc, BITSPIXEL );

			Con_Printf("...using desktop display depth of %d\n", bitspixel );

			ReleaseDC( 0, hdc );
		}

		Con_Printf("...calling CDS: " );
		if ( ChangeDisplaySettings( &dm, CDS_FULLSCREEN ) == DISP_CHANGE_SUCCESSFUL )
		{
			*pwidth = width;
			*pheight = height;

			gl_state.fullscreen = true;

			Con_Printf("ok\n" );

			if ( !VID_CreateWindow (width, height, true) )
				return rserr_invalid_mode;

			return rserr_ok;
		}
		else
		{
			*pwidth = width;
			*pheight = height;

			Con_Printf("failed\n" );

			Con_Printf("...calling CDS assuming dual monitors:" );

			dm.dmPelsWidth = width * 2;
			dm.dmPelsHeight = height;
			dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

			if ( gl_bitdepth->value != 0 )
			{
				dm.dmBitsPerPel = gl_bitdepth->value;
				dm.dmFields |= DM_BITSPERPEL;
			}

			/*
			** our first CDS failed, so maybe we're running on some weird dual monitor
			** system 
			*/
			if ( ChangeDisplaySettings( &dm, CDS_FULLSCREEN ) != DISP_CHANGE_SUCCESSFUL )
			{
				Con_Printf(" failed\n" );

				Con_Printf("...setting windowed mode\n" );

				ChangeDisplaySettings( 0, 0 );

				*pwidth = width;
				*pheight = height;
				gl_state.fullscreen = false;
				if ( !VID_CreateWindow (width, height, false) )
					return rserr_invalid_mode;
				return rserr_invalid_fullscreen;
			}
			else
			{
				Con_Printf(" ok\n" );
				if ( !VID_CreateWindow (width, height, true) )
					return rserr_invalid_mode;

				gl_state.fullscreen = true;
				return rserr_ok;
			}
		}
	}
	else
	{
		Con_Printf("...setting windowed mode\n" );

		ChangeDisplaySettings( 0, 0 );

		*pwidth = width;
		*pheight = height;
		gl_state.fullscreen = false;
		if ( !VID_CreateWindow (width, height, false) )
			return rserr_invalid_mode;
	}

	return rserr_ok;
}

/*
** GLimp_Shutdown
**
** This routine does all OS specific shutdown procedures for the OpenGL
** subsystem.  Under OpenGL this means NULLing out the current DC and
** HGLRC, deleting the rendering context, and releasing the DC acquired
** for the window.  The state structure is also nulled out.
**
*/

void GLimp_Shutdown( void )
{
	// Knightmare- added Vic's hardware gamma ramp
	ShutdownGammaRamp ();

	if ( qwglMakeCurrent && !qwglMakeCurrent( NULL, NULL ) )
		Con_Printf("ref_gl::R_Shutdown() - wglMakeCurrent failed\n");
	if ( glw_state.hGLRC )
	{
		if (  qwglDeleteContext && !qwglDeleteContext( glw_state.hGLRC ) )
			Con_Printf("ref_gl::R_Shutdown() - wglDeleteContext failed\n");
		glw_state.hGLRC = NULL;
	}
	if (glw_state.hDC)
	{
		if ( !ReleaseDC( mainwindow, glw_state.hDC ) )
			Con_Printf("ref_gl::R_Shutdown() - ReleaseDC failed\n" );
		glw_state.hDC   = NULL;
	}
	if (mainwindow)
	{
		ShowWindow( mainwindow, SW_HIDE );
		DestroyWindow (	mainwindow );
		mainwindow = NULL;
	}

	if ( glw_state.log_fp )
	{
		fclose( glw_state.log_fp );
		glw_state.log_fp = 0;
	}

	UnregisterClass (WINDOW_CLASS_NAME, glw_state.hInstance);

	if ( gl_state.fullscreen )
	{
		ChangeDisplaySettings( 0, 0 );
		gl_state.fullscreen = false;
	}
}


/*
** GLimp_Init
**
** This routine is responsible for initializing the OS specific portions
** of OpenGL.  Under Win32 this means dealing with the pixelformats and
** doing the wgl interface stuff.
*/
qboolean GLimp_Init( void *hinstance, void *wndproc )
{
#define OSR2_BUILD_NUMBER 1111

	OSVERSIONINFO	vinfo;

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	glw_state.allowdisplaydepthchange = false;

	if ( GetVersionEx( &vinfo) )
	{
		if ( vinfo.dwMajorVersion > 4 )
		{
			glw_state.allowdisplaydepthchange = true;
		}
		else if ( vinfo.dwMajorVersion == 4 )
		{
			if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT )
			{
				glw_state.allowdisplaydepthchange = true;
			}
			else if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS )
			{
				s_win95 = true;
				if ( LOWORD( vinfo.dwBuildNumber ) >= OSR2_BUILD_NUMBER )
				{
					glw_state.allowdisplaydepthchange = true;
				}
			}
		}
	}
	else
	{
		Con_Printf("GLimp_Init() - GetVersionEx failed\n" );
		return false;
	}

	glw_state.hInstance = ( HINSTANCE ) hinstance;
	glw_state.wndproc = wndproc;

	return true;
}

qboolean GLimp_InitGL (void)
{
    PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
		1,								// version number
		PFD_DRAW_TO_WINDOW |			// support window
		PFD_SUPPORT_OPENGL |			// support OpenGL
		PFD_DOUBLEBUFFER,				// double buffered
		PFD_TYPE_RGBA,					// RGBA type
		// Knightmare- was 24-bit, changed for stencil buffer
		32,								// 32-bit color depth
		0, 0, 0, 0, 0, 0,				// color bits ignored
		0,								// no alpha buffer
		0,								// shift bit ignored
		0,								// no accumulation buffer
		0, 0, 0, 0, 					// accum bits ignored
		// Knightmare- stencil buffer, was 32-bit z-buffer, no stencil buffer
		24,								// 24-bit z-buffer
		8,								// 8-bit stencil buffer
		0,								// no auxiliary buffer
		PFD_MAIN_PLANE,					// main layer
		0,								// reserved
		0, 0, 0							// layer masks ignored
    };
    int  pixelformat;
	cvar_t *stereo;
	
	stereo = Cvar_Get( "cl_stereo", "0", 0 );
	gl_multisample = Cvar_Get( "gl_multisample", "0", CVAR_ARCHIVE );
	gl_multisample->modified = false; /* FS: Eat it so we don't restart twice */
	gl_multisample_softrestart = Cvar_Get( "gl_multisample_softrestart", "1", CVAR_ARCHIVE );

	/*
	** set PFD_STEREO if necessary
	*/
	if ( stereo->value != 0 )
	{
		Con_Printf("...attempting to use stereo\n" );
		pfd.dwFlags |= PFD_STEREO;
		gl_state.stereo_enabled = true;
	}
	else
	{
		gl_state.stereo_enabled = false;
	}

	/*
	** figure out if we're running on a minidriver or not
	*/
	if ( strstr( gl_driver->string, "opengl32" ) != 0 )
		glw_state.minidriver = false;
	else
		glw_state.minidriver = true;

	/*
	** Get a DC for the specified window
	*/
	if ( glw_state.hDC != NULL )
		Con_Printf("GLimp_Init() - non-NULL DC exists\n" );

    if ( ( glw_state.hDC = GetDC( mainwindow ) ) == NULL )
	{
		Con_Printf("GLimp_Init() - GetDC failed\n" );
		return false;
	}

	if ( glw_state.minidriver )
	{
		if ( (pixelformat = qwglChoosePixelFormat( glw_state.hDC, &pfd)) == 0 )
		{
			Con_Printf("GLimp_Init() - qwglChoosePixelFormat failed\n");
			return false;
		}
		if (glPixelFormatMSAA) /* FS: Control Anti-Aliasing */
		{
			pixelformat = glPixelFormatMSAA;
		}

		if ( qwglSetPixelFormat( glw_state.hDC, pixelformat, &pfd) == FALSE )
		{
			Con_Printf("GLimp_Init() - qwglSetPixelFormat failed\n");
			return false;
		}
		qwglDescribePixelFormat( glw_state.hDC, pixelformat, sizeof( pfd ), &pfd );
	}
	else
	{
		if ( ( pixelformat = ChoosePixelFormat( glw_state.hDC, &pfd)) == 0 )
		{
			Con_Printf("GLimp_Init() - ChoosePixelFormat failed\n");
			return false;
		}
		if (glPixelFormatMSAA) /* FS: Control Anti-Aliasing */
		{
			pixelformat = glPixelFormatMSAA;
		}
		if ( SetPixelFormat( glw_state.hDC, pixelformat, &pfd) == FALSE )
		{
			Con_Printf("GLimp_Init() - SetPixelFormat failed\n");
			return false;
		}
		DescribePixelFormat( glw_state.hDC, pixelformat, sizeof( pfd ), &pfd );

		if ( !( pfd.dwFlags & PFD_GENERIC_ACCELERATED ) )
		{
			extern cvar_t *gl_allow_software;

			if ( gl_allow_software->value )
				glw_state.mcd_accelerated = true;
			else
				glw_state.mcd_accelerated = false;
		}
		else
		{
			glw_state.mcd_accelerated = true;
		}
	}

	/*
	** report if stereo is desired but unavailable
	*/
	if ( !( pfd.dwFlags & PFD_STEREO ) && ( stereo->value != 0 ) ) 
	{
		Con_Printf("...failed to select stereo pixel format\n" );
		Cvar_SetValue( "cl_stereo", 0 );
		gl_state.stereo_enabled = false;
	}

	/*
	** startup the OpenGL subsystem by creating a context and making
	** it current
	*/
	if ( ( glw_state.hGLRC = qwglCreateContext( glw_state.hDC ) ) == 0 )
	{
		Con_Printf("GLimp_Init() - qwglCreateContext failed\n");

		goto fail;
	}

    if ( !qwglMakeCurrent( glw_state.hDC, glw_state.hGLRC ) )
	{
		Con_Printf("GLimp_Init() - qwglMakeCurrent failed\n");

		goto fail;
	}

	if ( !VerifyDriver() )
	{
		Con_Printf("GLimp_Init() - no hardware acceleration detected\n" );
		goto fail;
	}

	/* FS: Control Anti-Aliasing */
	if (!glPixelFormatMSAA)
	{
		glPixelFormatMSAA = Get_MultiSample();
		if (glPixelFormatMSAA)
		{
			if (s_win95 || !gl_multisample_softrestart->intValue)
			{
				/* FS: wglChoosePixelFormatARB will be unavailable until the context is created which is stupid but true: https://www.khronos.org/opengl/wiki/Multisampling */
				GLimp_SetMode((int*)&vid.width, (int*)&vid.height, gl_mode->value, vid_fullscreen->intValue ? true : false);
				qglEnable(glMultisample);
				return true;
			}
			else
			{
				VID_SoftRestart(); /* FS: This hack works on WinNT to avoid destroying the entire window.  Win9x won't let us get away with it. */
				qglEnable(glMultisample);
			}
		}
	}

	/*
	** print out PFD specifics 
	*/
	Con_Printf("GL PFD: color(%d-bits) Z(%d-bit)\n", ( int ) pfd.cColorBits, ( int ) pfd.cDepthBits );

	// Knightmare- Vic's hardware gamma stuff
	InitGammaRamp ();

	// Knightmare- stencil buffer
	if (strstr((const char *)qglGetString(GL_RENDERER), "Voodoo3")) {
		Con_Printf("... Voodoo3 has no stencil buffer\n" );
	} else {
		if (pfd.cStencilBits) {
			Con_Printf("... Using stencil buffer\n" );
			gl_config.have_stencil = true;
		}
		else
			Con_Printf("... Stencil buffer not found\n" );
	}

	return true;

fail:
	if ( glw_state.hGLRC )
	{
		qwglDeleteContext( glw_state.hGLRC );
		glw_state.hGLRC = NULL;
	}

	if ( glw_state.hDC )
	{
		ReleaseDC( mainwindow, glw_state.hDC );
		glw_state.hDC = NULL;
	}
	return false;
}


/*
** GLimp_BeginFrame
*/
void GLimp_BeginFrame( float camera_separation )
{
	if ( gl_bitdepth->modified )
	{
		if ( gl_bitdepth->value != 0 && !glw_state.allowdisplaydepthchange )
		{
			Cvar_SetValue( "gl_bitdepth", 0 );
			Con_Printf("gl_bitdepth requires Win95 OSR2.x or WinNT 4.x\n" );
		}
		gl_bitdepth->modified = false;
	}

	/* FS: Control Anti-Aliasing */
	if (gl_multisample->modified)
	{
		if((gl_config.renderer & GL_RENDERER_VOODOO && strstr(gl_config.vendor_string, "3Dfx Interactive Inc."))
			|| gl_config.renderer & GL_RENDERER_DEFAULT)
		{
			cvar_t *ref = Cvar_Get ("vid_ref", "gl", 0);
			qglDisable(glMultisample);
			ref->modified = true;
		}
		glPixelFormatMSAA = 0;
		gl_multisample->modified = false;
	}

	if ( camera_separation < 0 && gl_state.stereo_enabled )
	{
		qglDrawBuffer( GL_BACK_LEFT );
	}
	else if ( camera_separation > 0 && gl_state.stereo_enabled )
	{
		qglDrawBuffer( GL_BACK_RIGHT );
	}
	else
	{
		qglDrawBuffer( GL_BACK );
	}
}

/*
** GLimp_EndFrame
** 
** Responsible for doing a swapbuffers and possibly for other stuff
** as yet to be determined.  Probably better not to make this a GLimp
** function and instead do a call to GLimp_SwapBuffers.
*/
void GLimp_EndFrame (void)
{
	int		err;

	err = qglGetError();
//	assert( err == GL_NO_ERROR );
	// Knightmare- Output error code instead
	if (err != GL_NO_ERROR)
	//	ri.Con_Printf (PRINT_DEVELOPER, "GLimp_EndFrame: OpenGL Error 0x%x\n", err);
		GL_PrintError (err, "GLimp_EndFrame");
	// end Knightmare

	if ( stricmp( gl_drawbuffer->string, "GL_BACK" ) == 0 )
	{
		if ( !qwglSwapBuffers( glw_state.hDC ) )
			Sys_Error("GLimp_EndFrame() - SwapBuffers() failed!\n" );
	}
}

/*
** GLimp_AppActivate
*/
void GLimp_AppActivate( qboolean active )
{
	if ( active )
	{
		HWGamma_Toggle (true);	// Knightmare
		SetForegroundWindow( mainwindow );
		ShowWindow( mainwindow, SW_RESTORE );
	}
	else
	{
		HWGamma_Toggle (false);	// Knightmare
		if ( vid_fullscreen->value )
			ShowWindow( mainwindow, SW_MINIMIZE );
	}
}
