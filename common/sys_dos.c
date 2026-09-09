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
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dos.h>
#include <dir.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <dpmi.h>
#include <crt0.h> /* FS: Fake Mem Fix for Win9x (QIP) */
#include <sys/nearptr.h>
#include <conio.h>

int _crt0_startup_flags = _CRT0_FLAG_UNIX_SBRK; /* FS: Fake Mem Fix for Win9x (QIP) */
unsigned int _stklen = 1048576; /* need a 1MB stack */

#include "quakedef.h"
#include "dosisms.h"
#include "sys_dxe.h"

#ifdef QUAKE1
#define MINIMUM_WIN_MEMORY                      0x800000
#else
#define MINIMUM_WIN_MEMORY                      0xf00000
#endif // QUAKE1

#define MINIMUM_WIN_MEMORY_LEVELPAK     (MINIMUM_WIN_MEMORY + 0x100000)

#define STDOUT  1

#define	KEYBUF_SIZE	256
static unsigned char	keybuf[KEYBUF_SIZE];
static int	keybuf_head = 0;
static int	keybuf_tail = 0;

static quakeparms_t	quakeparms;
static int	minmem;

float			fptest_temp;

extern char	start_of_memory __asm__("start");

static byte scantokey[128] =
{
//	0        1       2       3       4       5       6       7
//	8        9       A       B       C       D       E       F
	0  ,    27,     '1',    '2',    '3',    '4',    '5',    '6',
	'7',    '8',    '9',    '0',    '-',    '=', K_BACKSPACE, 9,	// 0
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
	'o',    'p',    '[',    ']',     13,   K_CTRL,  'a',    's',	// 1
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';',
	'\'',   '`',  K_SHIFT,  '\\',   'z',    'x',    'c',    'v',	// 2
	'b',    'n',    'm',    ',',    '.',    '/',  K_SHIFT,  '*',
	K_ALT,  ' ',     0 ,    K_F1,   K_F2,   K_F3,   K_F4,  K_F5,	// 3
	K_F6,  K_F7,   K_F8,    K_F9,  K_F10,    0 ,     0 , K_HOME,
	K_UPARROW,K_PGUP,'-',K_LEFTARROW,'5',K_RIGHTARROW,'+',K_END,	// 4
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,   0 ,    0 ,     0 ,  K_F11,
	K_F12,   0 ,     0 ,     0 ,      0 ,    0 ,     0 ,     0 ,	// 5
	0  ,     0 ,     0 ,     0 ,      0 ,    0 ,     0 ,     0 ,
	0  ,     0 ,     0 ,     0 ,      0 ,    0 ,     0 ,     0 ,	// 6
	0  ,     0 ,     0 ,     0 ,      0 ,    0 ,     0 ,     0 ,
	0  ,     0 ,     0 ,     0 ,      0 ,    0 ,     0 ,     0	// 7
};

static void TrapKey(void)
{
	keybuf[keybuf_head] = dos_inportb(0x60);
	dos_outportb(0x20, 0x20);

	keybuf_head = (keybuf_head + 1) & (KEYBUF_SIZE-1);
}

int	sys_checksum;

int		end_of_memory;
static qboolean	lockmem, lockunlockmem, unlockmem;
static qboolean	skipwincheck, skiplfncheck, win95;

static __dpmi_meminfo	info; /* FS: Sigh, moved this here because everyone wants me to free this shit at exit.  Again, I'm pretty sure CWSDPMI is already taking care of this... */

/* FS: Stuff for /memstats */
static int physicalMemStart;
static unsigned long virtualMemStart;

void MaskExceptions (void);
void Sys_PushFPCW_SetHigh (void);
void Sys_PopFPCW (void);

#define LEAVE_FOR_CACHE (512*1024)		//FIXME: tune
#define LOCKED_FOR_MALLOC (128*1024)	//FIXME: tune

/* FS: QW needs it badly -- See http://www.delorie.com/djgpp/doc/libc/libc_380.html for more information

   ATTENTION FORKERS
   DO NOT REMOVE THE SLEEP OR WARNING!
   THIS IS SERIOUS, NO LFN AND SOME SKIN NAMES GET TRUNCATED
   WEIRD SHIT HAPPENS
   DON'T SEND ME BUG REPORTS FROM A SESSION WITH NO LFN DRIVER LOADED!
*/
static void Sys_DetectLFN (void)
{
	if(skiplfncheck)
		return;
	if(!(_get_volume_info(NULL, 0, 0, NULL) & _FILESYS_LFN_SUPPORTED))
	{
		printf("WARNING: Long file name support not detected!  Grab a copy of DOSLFN!\n");
		sleep(2);
		printf("Continuing to load %s. . .\n", QUAKEGAME);
	}
}

static qboolean Sys_DetectWinNT (void) /* FS: Wisdom from Gisle Vanem */
{
	/* FS: Might sound crazy, but you could use that swsvpkt driver in NTVDM... */
	if(_get_dos_version(1) == 0x0532)
		return true;
	return false;
}

static void Sys_DetectWin95 (void)
{
	__dpmi_regs r;

	r.x.ax = 0x160a; /* Get Windows Version */
	__dpmi_int(0x2f, &r);

	if (((r.x.ax || r.h.bh < 4) && !Sys_DetectWinNT())) /* Not windows or earlier than Win95 */
	{
		win95 = 0;
		lockmem = true;
		lockunlockmem = false;
		unlockmem = true;
	}
	else
	{
		printf("Microsoft Windows detected.  Please run Quake in pure DOS for best stability.\n"); /* FS: Warning */
		win95 = 1;
		lockunlockmem = COM_CheckParm ("-winlockunlock");
		if (lockunlockmem)
			lockmem = true;
		else
			lockmem = COM_CheckParm ("-winlock");
		unlockmem = lockmem && !lockunlockmem;
	}
}


void *dos_getmaxlockedmem(int *size)
{
	__dpmi_free_mem_info	meminfo;
	__dpmi_meminfo			info;
	int						working_size;
	void					*working_memory;
	int						last_locked;
	int						i, j, extra, allocsize; /* FS: 2GB Fix */
	static char				*msg = "Locking data...";
	byte					*x;
	unsigned long			ul; /* FS: 2GB Fix */

// first lock all the current executing image so the locked count will
// be accurate.  It doesn't hurt to lock the memory multiple times
	last_locked = __djgpp_selector_limit + 1;
	info.size = last_locked - 4096;
	info.address = __djgpp_base_address + 4096;

	if (lockmem)
	{
		if(__dpmi_lock_linear_region(&info))
		{
			Sys_Error ("Lock of current memory at 0x%lx for %ldKb failed!\n",
						info.address, info.size/1024);
		}
	}

	__dpmi_get_free_memory_information(&meminfo);

	if (!win95)             /* Not windows or earlier than Win95 */
	{
		ul = meminfo.maximum_locked_page_allocation_in_pages * 4096; /* FS: 2GB Fix */
	}
	else
	{
		ul = meminfo.largest_available_free_block_in_bytes -
		LEAVE_FOR_CACHE; /* FS: 2GB Fix */
	}

	if (ul > 0x7fffffff)
		ul = 0x7fffffff; /* limit to 2GB */
	working_size = (int) ul;
	working_size &= ~0xffff;                /* Round down to 64K */
	working_size += 0x10000;

	do
	{
		working_size -= 0x10000;                /* Decrease 64K and try again */
		working_memory = sbrk(working_size);
	} while (working_memory == (void *)-1);

	extra = 0xfffc - ((unsigned)sbrk(0) & 0xffff);

	if (extra > 0)
	{
		sbrk(extra);
		working_size += extra;
	}

// now grab the memory
	info.address = last_locked + __djgpp_base_address;

	if (!win95)
	{
	    info.size = __djgpp_selector_limit + 1 - last_locked;

		while (info.size > 0 && __dpmi_lock_linear_region(&info))
		{
			info.size -= 0x1000;
			working_size -= 0x1000;
			sbrk(-0x1000);
		}
	}
	else
	{                       /* Win95 section */
		j = COM_CheckParm("-winmem");

		if (standard_quake)
			minmem = MINIMUM_WIN_MEMORY;
		else
			minmem = MINIMUM_WIN_MEMORY_LEVELPAK;

		if (j)
		{
			allocsize = ((int)(atoi(com_argv[j+1]))) * 0x100000 +
					LOCKED_FOR_MALLOC;

			if (allocsize < (minmem + LOCKED_FOR_MALLOC))
				allocsize = minmem + LOCKED_FOR_MALLOC;
		}
		else
		{
			allocsize = minmem + LOCKED_FOR_MALLOC;
		}

		if (!lockmem)
		{
		// we won't lock, just sbrk the memory
			info.size = allocsize;
			goto UpdateSbrk;
		}

		// lock the memory down
		write (STDOUT, msg, strlen (msg));

		for (j=allocsize ; j>(minmem + LOCKED_FOR_MALLOC) ;
			 j -= 0x100000)
		{
			info.size = j;
	
			if (!__dpmi_lock_linear_region(&info))
				goto Locked;
	
			write (STDOUT, ".", 1);
		}

	// finally, try with the absolute minimum amount
		for (i=0 ; i<10 ; i++)
		{
			info.size = minmem + LOCKED_FOR_MALLOC;

			if (!__dpmi_lock_linear_region(&info))
				goto Locked;
		}

		Sys_Error ("Can't lock memory; %ld Mb lockable RAM required. "
					"Try shrinking smartdrv.", info.size / 0x100000);

Locked:

UpdateSbrk:

		info.address += info.size;
		info.address -= __djgpp_base_address + 4; // ending point, malloc align
		working_size = info.address - (int)working_memory;
		sbrk(info.address-(int)sbrk(0));                // negative adjustment
	}


	if (lockunlockmem)
	{
		__dpmi_unlock_linear_region (&info);
		printf ("Locked and unlocked %d Mb data\n", working_size / 0x100000);
	}
	else if (lockmem)
	{
		printf ("Locked %d Mb data\n", working_size / 0x100000);
	}
	else
	{
		printf ("Allocated %d Mb data\n", working_size / 0x100000);
	}

// touch all the memory to make sure it's there. The 16-page skip is to
// keep Win 95 from thinking we're trying to page ourselves in (we are
// doing that, of course, but there's no reason we shouldn't)
	x = (byte *)working_memory;

	for (j=0 ; j<4 ; j++) /* FS: 2GB Fix */
	{
		for (i=0 ; i<(working_size - 16 * 0x1000) ; i += 4)
		{
			sys_checksum += *(int *)&x[i];
			sys_checksum += *(int *)&x[i + 16 * 0x1000];
		}
	}

// give some of what we locked back for malloc before returning.  Done
// by cheating and passing a negative value to sbrk
	working_size -= LOCKED_FOR_MALLOC;
	sbrk( -(LOCKED_FOR_MALLOC));
	*size = working_size;
	return working_memory;
}


/*
============
Sys_FileTime

returns -1 if not present
============
*/
int     Sys_FileTime (char *path)
{
	return __file_exists(path) ? 1 : -1;
}

void Sys_mkdir (char *path)
{
	mkdir (path, 0777);
}

#ifdef QUAKE1
char *Sys_ConsoleInput(void)
{
	static char     text[256];
	static int      len = 0;
	char            ch;

	if (!dedicated || !dedicated->value)
		return NULL;

	if (! kbhit())
		return NULL;

	ch = getche();

	switch (ch)
	{
		case '\r':
			putch('\n');
			if (len)
			{
				text[len] = 0;
				len = 0;
				return text;
			}
			break;

		case '\b':
			putch(' ');
			if (len)
			{
				len--;
				putch('\b');
			}
			break;

		default:
			text[len] = ch;
			len = (len + 1) & 0xff;
			break;
	}

	return NULL;
}
#endif

void Sys_Init(void)
{
	MaskExceptions ();

	Sys_SetFPCW ();

	_go32_interrupt_stack_size = 4 * 1024;
	_go32_rmcb_stack_size = 4 * 1024;

	Sys_InitDXE3();
}

void Sys_Shutdown(void)
{
	if (!dedicated || !dedicated->value)
		dos_restoreintr(9);

	if (unlockmem)
	{
		dos_unlockmem (&start_of_memory,
					   end_of_memory - (int)&start_of_memory);
		dos_unlockmem (quakeparms.membase, quakeparms.memsize);
	}
}

// Knightmare- added this to fix CPU usage
void Sys_Sleep (unsigned msec)
{
	usleep (msec*1000);
}

#define	SC_UPARROW	0x48
#define	SC_DOWNARROW	0x50
#define	SC_LEFTARROW	0x4b
#define	SC_RIGHTARROW	0x4d
#define	SC_LEFTSHIFT	0x2a
#define	SC_RIGHTSHIFT	0x36

void Sys_SendKeyEvents (void)
{
	int k, next;
	int outkey;

// get key events

	while (keybuf_head != keybuf_tail)
	{

		k = keybuf[keybuf_tail++];
		keybuf_tail &= (KEYBUF_SIZE-1);

		if (k==0xe0)
			continue;               // special / pause keys
		next = keybuf[(keybuf_tail-2)&(KEYBUF_SIZE-1)];
		if (next == 0xe1)
			continue;                               // pause key bullshit
		if (k==0xc5 && next == 0x9d) 
		{ 
			Key_Event (K_PAUSE, true);
			continue; 
		} 

		// extended keyboard shift key bullshit 
		if ( (k&0x7f)==SC_LEFTSHIFT || (k&0x7f)==SC_RIGHTSHIFT ) 
		{ 
			if ( keybuf[(keybuf_tail-2)&(KEYBUF_SIZE-1)]==0xe0 ) 
				continue; 
			k &= 0x80; 
			k |= SC_RIGHTSHIFT; 
		} 

		if (k==0xc5 && keybuf[(keybuf_tail-2)&(KEYBUF_SIZE-1)] == 0x9d)
			continue; // more pause bullshit

		outkey = scantokey[k & 0x7f];

		if (k & 0x80)
			Key_Event (outkey, false);
		else
			Key_Event (outkey, true);

	}

}


// =======================================================================
// General routines
// =======================================================================

/*
================
Sys_Printf
================
*/

void Sys_Printf (const char *fmt, ...)
{
	va_list	argptr;
	char text[MAXPRINTMSG];

	va_start (argptr, fmt);
	Q_vsnprintf (text, sizeof(text), fmt,argptr);
	va_end (argptr);
}

void Sys_AtExit (void)
{

// shutdown only once (so Sys_Error can call this function to shutdown, then
// print the error message, then call exit without exit calling this function
// again)
	Sys_Shutdown();
}

#ifndef GLQUAKE
static byte *sellscreen = NULL;
#endif

void Sys_Quit (void)
{
#ifndef GLQUAKE
	byte    screen[80 * 25 * 2];
	char                    ver[6];
	int                     i;

	// load the sell screen before shutting everything down
	if (sellscreen)
		memcpy (screen, sellscreen, sizeof(screen));

// write the version number directly to the end screen
	Com_sprintf (ver, sizeof(ver), " v%4.2f", VERSION);
	for (i=0 ; i<6 ; i++)
		screen[0*80*2 + 72*2 + i*2] = ver[i];
#endif

	Host_Shutdown();

#ifndef GLQUAKE
// do the text mode sell screen
	if (sellscreen)
	{
		memcpy ((void *)real2ptr(0xb8000), screen,80*25*2); 
	
	// set text pos
		regs.x.ax = 0x0200; 
		regs.h.bh = 0; 
		regs.h.dl = 0; 
		regs.h.dh = 22;
		dos_int86 (0x10);
	}
	else
	{
		printf ("couldn't load endscreen.\n");
	}
#endif

	__dpmi_free_physical_address_mapping(&info);
	__djgpp_nearptr_disable(); /* FS: Everyone else is a master DOS DPMI programmer.  Pretty sure CWSDPMI is already taking care of this... */

	exit(0);
}

void Sys_Error (const char *error, ...)
{ 
    va_list     argptr;
    char    string[MAXPRINTMSG];

    va_start (argptr,error);
    Q_vsnprintf (string, sizeof(string), error,argptr);
    va_end (argptr);

	Host_Shutdown();
	fprintf(stderr, "Error: %s\n", string);

	__dpmi_free_physical_address_mapping(&info);
	__djgpp_nearptr_disable(); /* FS: Everyone else is a master DOS DPMI programmer.  Pretty sure CWSDPMI is already taking care of this... */

	// Sys_AtExit is called by exit to shutdown the system
	exit(1);
} 

/*
================
Sys_MakeCodeWriteable
================
*/
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
	// it's always writeable
}

/*
================
Sys_DoubleTime
================
*/
double Sys_DoubleTime (void)
{
	return (double) uclock() / (double) UCLOCKS_PER_SEC; /* FS: Accurate Clock (QIP) */
}

/*
================
Sys_GetMemory
================
*/
void Sys_GetMemory(void)
{
	int j;

	quakeparms.memsize = 0x2000000;
#ifdef QUAKE1
	if (extended_mod)  /* FS: For big boy mods */
		quakeparms.memsize = 0x4000000;
#endif

	if ((j = COM_CheckParm("-mem")) != 0 && j < com_argc-1)
		quakeparms.memsize = atoi(com_argv[j+1]) * 1024 * 1024;

	if ((j = COM_CheckParm ("-heapsize")) != 0 && j < com_argc-1)
		quakeparms.memsize = atoi(com_argv[j+1]) * 1024;

	quakeparms.membase = malloc (quakeparms.memsize);

	printf("malloc'd: %ld\n", quakeparms.memsize);

	if (!COM_CheckParm ("-noclear")) /* FS: Wanted the option */
	{
		printf("Clearing allocated memory...\n");
		memset(quakeparms.membase,0x0,quakeparms.memsize); // JASON: Clear memory on startup
		printf("Done!  Continuing to load Quake.\n");
	}
}

static int Sys_Get_Physical_Memory(void) /* FS: From DJGPP tutorial */
{
	_go32_dpmi_meminfo meminfo;

	_go32_dpmi_get_free_memory_information(&meminfo);
	if (meminfo.available_physical_pages != -1)
		return meminfo.available_physical_pages * 4096;

	return meminfo.available_memory;
}

/*
================
Sys_PageInProgram

walks the text, data, and bss to make sure it's all paged in so that the
actual physical memory detected by Sys_GetMemory is correct.
================
*/
static void Sys_PageInProgram(void)
{
	int		i, j;

	end_of_memory = (int)sbrk(0);

	if (lockmem)
	{
		if (dos_lockmem ((void *)&start_of_memory,
						 end_of_memory - (int)&start_of_memory))
			Sys_Error ("Couldn't lock text and data");
	}

	if (lockunlockmem)
	{
		dos_unlockmem((void *)&start_of_memory,
						 end_of_memory - (int)&start_of_memory);
		printf ("Locked and unlocked %d Mb image\n",
				(end_of_memory - (int)&start_of_memory) / 0x100000);
	}
	else if (lockmem)
	{
		printf ("Locked %d Mb image\n",
				(end_of_memory - (int)&start_of_memory) / 0x100000);
	}
	else
	{
		printf ("Loaded %d Mb image\n",
				(end_of_memory - (int)&start_of_memory) / 0x100000);
	}

// touch the entire image, doing the 16-page skip so Win95 doesn't think we're
// trying to page ourselves in
	for (j=0 ; j<4 ; j++)
	{
		for(i=(int)&start_of_memory ; i<(end_of_memory - 16 * 0x1000) ; i += 4)
		{
			sys_checksum += *(int *)i;
			sys_checksum += *(int *)(i + 16 * 0x1000);
		}
	}

	/* FS: Report total amount available and save it for later if we run /memstats */
	physicalMemStart = (Sys_Get_Physical_Memory() / 0x100000);
	virtualMemStart = (_go32_dpmi_remaining_virtual_memory() / 0x100000);

	quakeparms.memsize = Sys_Get_Physical_Memory();

	printf("%d Mb available for QDOS.\n", physicalMemStart);
	printf("%lu Virtual Mb available for QDOS.\n", virtualMemStart);
}

/* FS: For /memstats */
void Sys_Memory_Stats_f (void)
{
	Com_Printf("%d Mb available for QDOS.  Started with %d.\n", (Sys_Get_Physical_Memory() / 0x100000), physicalMemStart);
	Com_Printf("%lu Virtual Mb available for QDOS. Started with %lu.\n", (_go32_dpmi_remaining_virtual_memory() / 0x100000), virtualMemStart);
}

static void Sys_ParseEarlyArgs(int argc, char **argv) /* FS: Parse some very specific args before Qcommon_Init */
{
	int i;
	for (i = 1; i < argc; i++)
	{
		if(stricmp(argv[i],"-skipwincheck") == 0)
			skipwincheck = true;
		if(stricmp(argv[i],"-skiplfncheck") == 0)
			skiplfncheck = true;
	}
}

/*
================
Sys_NoFPUExceptionHandler
================
*/
static void Sys_NoFPUExceptionHandler(int whatever)
{
	printf ("\nError: %s requires a floating-point processor\n", QUAKEGAME);
	exit (0);
}

/*
================
Sys_DefaultExceptionHandler
================
*/
static void Sys_DefaultExceptionHandler(int whatever)
{
}

void Sys_DebugLog(const char *file, const char *fmt, ...)
{
	va_list argptr;
	char data[MAXPRINTMSG];
	int fd;

	va_start(argptr, fmt);
	Q_vsnprintf(data, sizeof(data), fmt, argptr);
	va_end(argptr);

	fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0666);
	write(fd, data, strlen(data) - 1);
	close(fd);
}

//=============================================================================

int main (int c, char **v)
{
	double time, oldtime, newtime;
	static char cwd[1024];

	printf ("%s DOS v%4.2f\n", QUAKEGAME, VERSION);
	
// make sure there's an FPU
	signal(SIGNOFP, Sys_NoFPUExceptionHandler);
	signal(SIGABRT, Sys_DefaultExceptionHandler);
	signal(SIGALRM, Sys_DefaultExceptionHandler);
	signal(SIGKILL, Sys_DefaultExceptionHandler);
	signal(SIGQUIT, Sys_DefaultExceptionHandler);
	signal(SIGINT, Sys_DefaultExceptionHandler);

	if (fptest_temp >= 0.0)
		fptest_temp += 0.1;

	Sys_ParseEarlyArgs(c, v);

	COM_InitArgv (c, v);

	quakeparms.argc = com_argc;
	quakeparms.argv = com_argv;

	Sys_DetectLFN ();
	Sys_DetectWin95 ();
	Sys_PageInProgram ();
	Sys_GetMemory ();

	atexit (Sys_AtExit);    // in case we crash

	getwd (cwd);
	if (cwd[Q_strlen(cwd)-1] == '/')
		cwd[Q_strlen(cwd)-1] = 0;
	quakeparms.basedir = cwd; //"f:/quake";

	_crt0_startup_flags &= ~_CRT0_FLAG_UNIX_SBRK; /* FS: We walked through all the data, now remove the sbrk flag so Win9x doesn't barf. */

	Sys_Init ();

	Host_Init(&quakeparms);

#ifndef GLQUAKE
	/* FS: Load sellscreen now in case something fucks up later. */
	if (registered && registered->intValue)
		sellscreen = COM_LoadFile("end2.bin", COM_LOADFILE_CALLOC);
	else
		sellscreen = COM_LoadFile("end1.bin", COM_LOADFILE_CALLOC);
#endif

	oldtime = Sys_DoubleTime();

	if (!dedicated || !dedicated->value)
		dos_registerintr(9, TrapKey);

	while (1)
	{
		newtime = Sys_DoubleTime();
		time = newtime - oldtime;

#ifdef QUAKE1
		if (dedicated->intValue && (time<sys_ticrate->value))
			continue;
#endif

		Host_Frame (time);

		oldtime = newtime;
	}
}
