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

// sound.h -- client sound i/o functions

#ifndef __SOUND_H
#define __SOUND_H

#include "snd_strm.h" /* FS */
#ifdef OGG_SUPPORT
#include "snd_ogg.h"
#endif

#define WAV_FORMAT_PCM	1

#define DEFAULT_SOUND_PACKET_VOLUME 255
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0

#define SND_BUFFER_SIZE	16384 /* FS: Was 4096 */
#define	PAINTBUFFER_SIZE	2048 /* FS: Was 512 */

#define BGMUSIC_WAV (1<<0)
#define BGMUSIC_FLAC (1<<1)
#define BGMUSIC_MP3 (1<<2)
#define BGMUSIC_OGG (1<<3)

// !!! if this is changed, it much be changed in asm_i386.h too !!!
typedef struct
{
	int left;
	int right;
} portable_samplepair_t;

extern portable_samplepair_t paintbuffer[PAINTBUFFER_SIZE];

typedef struct sfx_s
{
	char			name[MAX_QPATH];
	cache_user_t	cache;
} sfx_t;

// !!! if this is changed, it much be changed in asm_i386.h too !!!
typedef struct
{
	int 	length;
	int 	loopstart;
	int 	speed;
	int 	width;
	int 	stereo;
	byte	data[1];		// variable sized
	int		music;			// Knightmare added
} sfxcache_t;



typedef struct
{
	int				channels;
	int				samples;				// mono samples in buffer
	int				submission_chunk;		// don't mix less than this #
	int				samplepos;				// in mono samples
	int				samplebits;
	int				speed;
	unsigned char	*buffer;
} dma_t;

// !!! if this is changed, it much be changed in asm_i386.h too !!!
typedef struct
{
	sfx_t	*sfx;			// sfx number
	int		leftvol;		// 0-255 volume
	int		rightvol;		// 0-255 volume
	int		end;			// end time in global paintsamples
	int 	pos;			// sample position in sfx
	int		looping;		// where to loop, -1 = no looping
	int		entnum;			// to allow overriding a specific sound
	int		entchannel;		//
	vec3_t	origin;			// origin of sound effect
	vec_t	dist_mult;		// distance multiplier (attenuation/clipK)
	int		master_vol;		// 0-255 master volume
} channel_t;

typedef struct
{
	int		rate;
	int		width;
	int		channels;
	int		loopstart;
	int		samples;
	int		dataofs;		// chunk starts this many bytes from file start
} wavinfo_t;

 /* FS: Quake 2 raw samples for music streaming */
#define	MAX_RAW_SAMPLES	8192
extern	portable_samplepair_t	*s_rawsamples;
extern	size_t	s_rawsamples_size;

void S_Init (void);
void S_Shutdown (void);
void S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol,  float attenuation);
void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation);
void S_StopSound (int entnum, int entchannel);
void S_StopAllSounds(void);
void S_ClearBuffer (void);
void S_Update (vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up);
void S_RawSamples (int samples, int rate, int width, int channels, byte *data, qboolean music);
				/* Expects data in signed 16 bit, or unsigned 8 bit format. */

sfx_t *S_PrecacheSound (char *sample);
void S_TouchSound (char *sample);
void S_PaintChannels(int endtime);
void S_InitPaintChannels (void);

// picks a channel based on priorities, empty slots, number of channels
channel_t *S_PickChannel(int entnum, int entchannel);

// spatializes a channel
void S_Spatialize(channel_t *ch);

// initializes cycling through a DMA buffer and returns information on it
qboolean SNDDMA_Init(void);

// gets the current DMA position
int SNDDMA_GetDMAPos(void);

// shutdown the DMA xfer.
void SNDDMA_Shutdown(void);

// ====================================================================
// User-setable variables
// ====================================================================

#define	MAX_CHANNELS			512 //johnfitz -- was 128
#define	MAX_DYNAMIC_CHANNELS	128 //johnfitz -- was 8


extern	channel_t   channels[MAX_CHANNELS];
// 0 to MAX_DYNAMIC_CHANNELS-1	= normal entity sounds
// MAX_DYNAMIC_CHANNELS to MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS -1 = water, etc
// MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS to total_channels = static sounds

extern	int		sound_started;

extern	int		total_channels;

extern int		paintedtime;
extern int		s_rawend; /* FS: Quake 2 raw wav streaming */

extern vec3_t listener_origin;
extern vec3_t listener_forward;
extern vec3_t listener_right;
extern vec3_t listener_up;
extern dma_t dma;
extern vec_t sound_nominal_clip_dist;

extern cvar_t	*s_loadas8bit;
extern cvar_t	*s_volume;
extern cvar_t	*s_mixahead;
extern cvar_t	*s_khz; /* FS: Added */
extern cvar_t	*s_musicvolume; /* FS: Added */
extern cvar_t	*s_mastervolume; /* FS: added */
extern cvar_t	*s_primary;

extern qboolean	snd_initialized;

void S_LocalSound (char *s);
void S_GamespySound (char *s); /* FS: Added */
void S_MusicPause (void);
void S_MusicResume (void);

sfxcache_t *S_LoadSound (sfx_t *s);

wavinfo_t GetWavinfo (char *name, byte *wav, int wavlength);

void S_InitScaletable (void);
void SNDDMA_Submit(void);

void S_AmbientOff (void);
void S_AmbientOn (void);

void S_StopBackgroundTrack(void); /* FS: So we can suport both */

/* FS: Gravis UltraSound specific stuff.  Mostly for the s_khz menu. */
#define GUS_CLASSIC 1
#define GUS_MAXPNP 2
extern int havegus; /* FS: Added */
void GUS_ClearDMA(void); /* FS: Added */

extern void SNDDMA_BeginPainting (void);

extern byte *s_streamDataPtr;

#endif // __SOUND_H
