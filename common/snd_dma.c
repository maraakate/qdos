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
// snd_dma.c -- main control for any streaming sound output device

#include "quakedef.h"

#ifdef _WIN32
#include "winquake.h"
#endif

#ifdef QUAKEWORLD
// QuakeWorld hack...
#define viewentity	playernum+1
#endif // QUAKEWORLD

void S_Play(void);
void S_Play2(void); /* FS: For Nehahra */
void S_PlayVol(void);
void S_SoundList(void);
void S_Update_();
void S_StopAllSounds(void);


// =======================================================================
// Internal sound data & structures
// =======================================================================

channel_t	channels[MAX_CHANNELS];
int			total_channels;

static qboolean		snd_ambient = 1;
qboolean		snd_initialized = false;

dma_t	dma;

vec3_t		listener_origin;
vec3_t		listener_forward;
vec3_t		listener_right;
vec3_t		listener_up;
vec_t		sound_nominal_clip_dist=1000.0;

int		soundtime;	// sample PAIRS
int		paintedtime;	// sample PAIRS

/* FS: Quake 2 raw samples for music streaming */
int		s_rawend;
portable_samplepair_t	*s_rawsamples;
size_t	s_rawsamples_size;

#define MAX_SFX		512
sfx_t		known_sfx[MAX_SFX];
int		num_sfx;

sfx_t		*ambient_sfx[NUM_AMBIENTS];

int		desired_speed = 11025;
int		desired_bits = 16;

int		sound_started = 0;

int		havegus; /* FS: Is GUS our sound card? */

cvar_t	*s_nosound;
cvar_t	*s_volume;
cvar_t	*s_precache;
cvar_t	*s_loadas8bit;
cvar_t	*s_ambient_level;
cvar_t	*s_ambient_fade;
cvar_t	*snd_show;
cvar_t	*s_mixahead;
cvar_t	*s_primary;


/* FS: New stuff */
cvar_t	*s_khz;
cvar_t	*s_musicvolume;
cvar_t	*s_mastervolume;
cvar_t	*s_rawsamples_size_cvar;

byte *s_streamDataPtr;

// ====================================================================
// User-setable variables
// ====================================================================

void S_AmbientOff (void)
{
	snd_ambient = false;
}


void S_AmbientOn (void)
{
	snd_ambient = true;
}


void S_SoundInfo_f(void)
{
	if (!sound_started)
	{
		Com_Printf ("sound system not started\n");
		return;
	}

	Com_Printf("%5d stereo\n", dma.channels - 1);
	Com_Printf("%5d samples\n", dma.samples);
	Com_Printf("%5d samplepos\n", dma.samplepos);
	Com_Printf("%5d samplebits\n", dma.samplebits);
	Com_Printf("%5d submission_chunk\n", dma.submission_chunk);
	Com_Printf("%5d speed\n", dma.speed);
	Com_Printf("%p dma buffer\n", dma.buffer);
	Com_Printf("%5d total_channels\n", total_channels);
}

/*
================
S_Init
================
*/
void S_Init (void)
{
	cvar_t	*cv;

	Com_Printf("\n------- sound initialization -------\n");

	cv = Cvar_Get ("s_initsound", "1", 0);
	if (!cv->intValue)
	{
		dma.buffer = NULL;/* just in case */
		//paintbuffer = NULL;
		s_rawsamples = NULL;
		s_streamDataPtr = NULL;

		Com_Printf ("not initializing.\n");
		return;
	}
	
	s_nosound = Cvar_Get("s_nosound", "0", 0);
	s_volume = Cvar_Get("s_volume", "0.7", CVAR_ARCHIVE);
	s_precache = Cvar_Get("s_precache", "1", 0);
	s_loadas8bit = Cvar_Get("s_loadas8bit", "0", 0);
	s_ambient_level = Cvar_Get("s_ambient_level", "0.3", 0);
	s_ambient_fade = Cvar_Get("s_ambient_fade", "100", 0);
	snd_show = Cvar_Get("snd_show", "0", 0);
	s_mixahead = Cvar_Get("s_mixahead", "0.2", CVAR_ARCHIVE);
	s_primary = Cvar_Get ("s_primary", "0", CVAR_ARCHIVE);	// win32 specific

	/* FS: New stuff */
	s_khz = Cvar_Get("s_khz","", CVAR_ARCHIVE);
	Cvar_Set_Description("s_khz", "Sound sampling rate.");
	s_musicvolume = Cvar_Get("s_musicvolume", "1.0", CVAR_ARCHIVE);
	Cvar_Set_Description("s_musicvolume", "Music volume for wav and ogg streaming.");
	s_mastervolume = Cvar_Get("s_mastervolume", "1.0", CVAR_ARCHIVE);
	s_rawsamples_size_cvar = Cvar_Get("s_rawsamples_size", va("%d", MAX_RAW_SAMPLES), 0);

	if (COM_CheckParm("-nosound"))
		return;

	Cmd_AddCommand("play", S_Play);
	Cmd_AddCommand("play2", S_Play2); /* FS: For Nehara */
	Cmd_AddCommand("playvol", S_PlayVol);
	Cmd_AddCommand("stopsound", S_StopAllSounds);
	Cmd_AddCommand("soundlist", S_SoundList);
	Cmd_AddCommand("soundinfo", S_SoundInfo_f);
#ifdef OGG_SUPPORT
	Cmd_AddCommand("ogg_restart", S_OGG_Restart); /* Knightmare added */
#endif
	Cmd_AddCommand("stream_restart", S_StreamRestart); /* FS */

	if (host_parms.memsize < 0x800000)
	{
		Cvar_Set ("s_loadas8bit", "1");
		Com_Printf ("loading all sounds as 8bit\n");
	}

	if (s_volume->value < 0.0f)
		Cvar_Set("s_volume", "0");
	else if (s_volume->value > 1.0f)
		Cvar_Set("s_volume", "1.0");

	if (s_musicvolume->value < 0.0f)
		Cvar_Set("s_musicvolume", "0");
	else if (s_musicvolume->value > 1.0f)
		Cvar_Set("s_musicvolume", "1.0");

	if (s_mastervolume->value < 0.0f)
		Cvar_Set("s_mastervolume", "0");
	else if (s_mastervolume->value > 1.0f)
		Cvar_Set("s_mastervolume", "1.0");

	snd_initialized = true;

	if (!SNDDMA_Init()) {
		dma.buffer = NULL;/* just in case */
		return;
	}

	S_InitScaletable ();

	sound_started = 1;
	memset(known_sfx, 0, sizeof(sfx_t) * MAX_SFX);
	num_sfx = 0;

	soundtime = 0;
	paintedtime = 0;

	s_rawsamples_size = bound(128, s_rawsamples_size_cvar->intValue, SND_BUFFER_SIZE);
	s_rawsamples = (portable_samplepair_t *)calloc(1, s_rawsamples_size * sizeof(portable_samplepair_t));
	if (!s_rawsamples)
	{
		dma.buffer = NULL;
		return;
	}

	s_streamDataPtr = (byte *)calloc(1, s_rawsamples_size * sizeof(byte));
	if (!s_streamDataPtr)
	{
		free(s_rawsamples);
		s_rawsamples = NULL;
		dma.buffer = NULL;
		return;
	}

	Com_Printf("Channels: %d, Bits: %d, Rate: %d\nRaw Samples Buffer Size: %d\n", dma.channels, dma.samplebits, dma.speed, (int)s_rawsamples_size);

//	if (dma.buffer)
//		dma.buffer[4] = dma.buffer[5] = 0x7f; // force a pop for debugging

	ambient_sfx[AMBIENT_WATER] = S_PrecacheSound ("ambience/water1.wav");
	ambient_sfx[AMBIENT_SKY] = S_PrecacheSound ("ambience/wind2.wav");

#ifdef OGG_SUPPORT
	if(!COM_CheckParm("-noogg"))
		S_OGG_Init(); /* Knightmare added */
#endif
	if (!COM_CheckParm("-nostream"))
		S_StreamInit();

	S_StopAllSounds ();

	Com_Printf("------------------------------------\n");
}


// =======================================================================
// Shutdown sound engine
// =======================================================================

void S_Shutdown(void)
{
	sfx_t *sfx;
	int i;

	if (!sound_started)
		return;

	if (snd_initialized)
	{
		Cmd_RemoveCommand("play");
		Cmd_RemoveCommand("play2"); /* FS: For Nehara */
		Cmd_RemoveCommand("playvol");
		Cmd_RemoveCommand("stopsound");
		Cmd_RemoveCommand("soundlist");
		Cmd_RemoveCommand("soundinfo");
#ifdef OGG_SUPPORT
		Cmd_RemoveCommand("ogg_restart"); /* Knightmare added */
#endif
		Cmd_RemoveCommand("stream_restart"); /* FS */
	}

#ifdef OGG_SUPPORT
	S_OGG_Shutdown(); /* Knightmare added */
#endif
	S_StreamShutdown(); /* FS */

	SNDDMA_Shutdown();

	for (i = 0; i < num_sfx; i++)
	{
		sfx = &known_sfx[i];
		if (sfx && sfx->cache.data)
		{
			Cache_Free(&sfx->cache);
			sfx->cache.data = NULL;
		}
	}

	dma.buffer = NULL;
	sound_started = 0;
	snd_initialized = 0;
}


// =======================================================================
// Load a sound
// =======================================================================

/*
==================
S_FindName

==================
*/
sfx_t *S_FindName (char *name)
{
	int		i;
	sfx_t	*sfx;

	if (!name)
		Sys_Error ("S_FindName: NULL\n");
	if (!name[0])
		Sys_Error ("S_FindName: empty name\n");

	if (Q_strlen(name) >= MAX_QPATH)
		Sys_Error ("Sound name too long: %s", name);

// see if already loaded
	for (i=0 ; i < num_sfx ; i++)
	{
		if (!Q_strcmp(known_sfx[i].name, name))
		{
			return &known_sfx[i];
		}
	}

	if (num_sfx == MAX_SFX)
		Sys_Error ("S_FindName: out of sfx_t");

	sfx = &known_sfx[i];
	memset (sfx, 0, sizeof(*sfx));
	Q_strlcpy (sfx->name, name, sizeof(sfx->name));

	num_sfx++;

	return sfx;
}


/*
==================
S_TouchSound

==================
*/
void S_TouchSound (char *name)
{
	sfx_t	*sfx;

	if (!sound_started)
		return;

	sfx = S_FindName (name);
	if (!sfx)
		S_LoadSound(sfx);
}

/*
==================
S_PrecacheSound

==================
*/
sfx_t *S_PrecacheSound (char *name)
{
	sfx_t	*sfx;

	if (!sound_started || s_nosound->intValue)
		return NULL;

	sfx = S_FindName (name);

// cache it in
	if (s_precache->intValue)
		S_LoadSound (sfx);

	return sfx;
}


//=============================================================================

/*
=================
SND_PickChannel
=================
*/
channel_t *S_PickChannel(int entnum, int entchannel)
{
	int ch_idx;
	int first_to_die;
	int life_left;
	channel_t	*ch;

// Check for replacement sound, or find the best one to replace
	first_to_die = -1;
	life_left = 0x7fffffff;
	for (ch_idx=NUM_AMBIENTS ; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS ; ch_idx++)
	{
		if (entchannel != 0		// channel 0 never overrides
				&& channels[ch_idx].entnum == entnum
				&& (channels[ch_idx].entchannel == entchannel || entchannel == -1))
		{       // allways override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (channels[ch_idx].entnum == cl.viewentity && entnum != cl.viewentity && channels[ch_idx].sfx)
		{
			continue;
		}

		if (channels[ch_idx].end - paintedtime < life_left)
		{
			life_left = channels[ch_idx].end - paintedtime;
			first_to_die = ch_idx;
		}
	}

	if (first_to_die == -1)
		return NULL;

	ch = &channels[first_to_die];
	memset (ch, 0, sizeof(*ch));

	return ch;
}

/*
=================
SND_Spatialize
=================
*/
void S_Spatialize(channel_t *ch)
{
	vec_t dot, dist;
	vec_t lscale, rscale, scale;
	vec3_t source_vec;
//	sfx_t *snd;

// anything coming from the view entity will allways be full volume
	if (ch->entnum == cl.viewentity)
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

// calculate stereo seperation and distance attenuation

//	snd = ch->sfx;
	VectorSubtract(ch->origin, listener_origin, source_vec);
	
	dist = VectorNormalize(source_vec) * ch->dist_mult;
	
	dot = DotProduct(listener_right, source_vec);

	if (dma.channels == 1)
	{
		rscale = 1.0;
		lscale = 1.0;
	}
	else
	{
		rscale = 1.0 + dot;
		lscale = 1.0 - dot;
	}

// add in distance effect
	scale = (1.0 - dist) * rscale;
	ch->rightvol = (int) (ch->master_vol * scale);
	if (ch->rightvol < 0)
		ch->rightvol = 0;

	scale = (1.0 - dist) * lscale;
	ch->leftvol = (int) (ch->master_vol * scale);
	if (ch->leftvol < 0)
		ch->leftvol = 0;
}


// =======================================================================
// Start a sound effect
// =======================================================================

void S_StartSound(int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation)
{
	channel_t *target_chan, *check;
	sfxcache_t	*sc;
	int		vol;
	int		ch_idx;
	int		skip;

	if (!sound_started)
		return;

	if (!sfx)
		return;

	if (s_nosound->intValue)
		return;

	vol = fvol*255;

// pick a channel to play on
	target_chan = S_PickChannel(entnum, entchannel);
	if (!target_chan)
		return;

// spatialize
	memset (target_chan, 0, sizeof(*target_chan));
	VectorCopy(origin, target_chan->origin);
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;
	target_chan->master_vol = vol;
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	S_Spatialize(target_chan);

	if (!target_chan->leftvol && !target_chan->rightvol)
		return;         // not audible at all

// new channel
	sc = S_LoadSound (sfx);
	if (!sc)
	{
		target_chan->sfx = NULL;
		return;         // couldn't load the sound's data
	}

	target_chan->sfx = sfx;
	target_chan->pos = 0.0;
	target_chan->end = paintedtime + sc->length;

// if an identical sound has also been started this frame, offset the pos
// a bit to keep it from just making the first one louder
	check = &channels[NUM_AMBIENTS];
	for (ch_idx=NUM_AMBIENTS ; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS ; ch_idx++, check++)
	{
		if (check == target_chan)
			continue;
		if (check->sfx == sfx && !check->pos)
		{
			skip = rand () % (int)(0.1*dma.speed);
			if (skip >= target_chan->end)
				skip = target_chan->end - 1;
			target_chan->pos += skip;
			target_chan->end -= skip;
			break;
		}
		
	}
}

void S_StopSound(int entnum, int entchannel)
{
	int i;

	for (i=0 ; i<MAX_DYNAMIC_CHANNELS ; i++)
	{
		if (channels[i].entnum == entnum
			&& channels[i].entchannel == entchannel)
		{
			channels[i].end = 0;
			channels[i].sfx = NULL;
			return;
		}
	}
}

void S_StopAllSounds(void)
{
	if (!sound_started)
		return;

	s_rawend = 0;

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;   // no statics

	memset(channels, 0, MAX_CHANNELS * sizeof(channel_t));

	S_StopBackgroundTrack (); /* Knightmare added */

	S_ClearBuffer ();
}

void S_ClearBuffer (void)
{
	int		clear;
	int		i;

	if (!sound_started)
		return;

	s_rawend = 0;
	for (i = 0; i < s_rawsamples_size; i++) /* FS: Clear out the s_rawsamples too.  Should not matter, but just in case we read ahead of some garbage data on load. */
	{
		memset(&s_rawsamples[i], 0, sizeof(portable_samplepair_t));
		memset(&s_streamDataPtr[i], 0, sizeof(byte));
	}

	if (dma.samplebits == 8)
		clear = 0x80;
	else
		clear = 0;

	SNDDMA_BeginPainting ();
	if (dma.buffer)
		memset(dma.buffer, clear, dma.samples * dma.samplebits/8);
	SNDDMA_Submit ();
#ifdef __DJGPP__
	if(havegus)
		GUS_ClearDMA(); /* FS: Added */
#endif
}

/*
=================
S_StaticSound
=================
*/
void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
	channel_t	*ss;
	sfxcache_t		*sc;

	if (!sfx)
		return;

	if (total_channels == MAX_CHANNELS)
	{
		Com_DPrintf (DEVELOPER_MSG_SOUND, "total_channels == MAX_CHANNELS\n"); /* FS: Now DPrintf */
		return;
	}

	ss = &channels[total_channels];
	total_channels++;

	sc = S_LoadSound (sfx);
	if (!sc)
		return;

	if (sc->loopstart == -1)
	{
		Com_Printf ("Sound %s not looped\n", sfx->name);
		return;
	}
	
	ss->sfx = sfx;
	VectorCopy (origin, ss->origin);
	ss->master_vol = vol;
	ss->dist_mult = (attenuation/64) / sound_nominal_clip_dist;
	ss->end = paintedtime + sc->length; 

	S_Spatialize (ss);
}


//=============================================================================

/*
===================
S_UpdateAmbientSounds
===================
*/
void S_UpdateAmbientSounds (void)
{
	mleaf_t		*l;
	float		vol;
	int			ambient_channel;
	channel_t	*chan;

	if (!snd_ambient)
		return;

// calc ambient sound levels
	if (!cl.worldmodel || cls.state != ca_active)
		return;

	l = Mod_PointInLeaf (listener_origin, cl.worldmodel);
	if (!l || !s_ambient_level->value)
	{
		for (ambient_channel = 0 ; ambient_channel< NUM_AMBIENTS ; ambient_channel++)
			channels[ambient_channel].sfx = NULL;
		return;
	}

	for (ambient_channel = 0 ; ambient_channel< NUM_AMBIENTS ; ambient_channel++)
	{
		chan = &channels[ambient_channel];      
		chan->sfx = ambient_sfx[ambient_channel];
	
		vol = s_ambient_level->value * l->ambient_sound_level[ambient_channel];
		if (vol < 8)
			vol = 0;

	// don't adjust volume too fast
		if (chan->master_vol < vol)
		{
			chan->master_vol += host_frametime * s_ambient_fade->value;
			if (chan->master_vol > vol)
				chan->master_vol = vol;
		}
		else if (chan->master_vol > vol)
		{
			chan->master_vol -= host_frametime * s_ambient_fade->value;
			if (chan->master_vol < vol)
				chan->master_vol = vol;
		}
		
		chan->leftvol = chan->rightvol = chan->master_vol;
	}
}


/*
============
S_Update

Called once each time through the main loop
============
*/
void S_Update(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
	int			i, j;
	int			total;
	channel_t	*ch;
	channel_t	*combine;

	if (!sound_started || cl.paused)
		return;

	/* FS: Don't allow dumb values. */
	if (s_mastervolume->value < 0.0f)
	{
		Cvar_ForceSet("s_mastervolume", "0.0");
	}
	else if (s_mastervolume->value > 1.0f)
	{
		Cvar_ForceSet("s_mastervolume", "1.0");
	}

	if (s_volume->value < 0.0f)
	{
		Cvar_ForceSet("s_volume", "0.0");
	}
	else if (s_volume->value > 1.0f)
	{
		Cvar_ForceSet("s_volume", "1.0");
	}

	if (s_musicvolume->value < 0.0f)
	{
		Cvar_ForceSet("s_musicvolume", "0.0");
	}
	else if (s_musicvolume->value > 1.0f)
	{
		Cvar_ForceSet("s_musicvolume", "1.0");
	}

	if (s_musicvolume->modified || s_mastervolume->modified)
	{
		s_musicvolume->modified = false;
	}

	// rebuild scale tables if volume is modified
	if (s_mastervolume->modified || s_volume->modified)
	{
		S_InitScaletable ();
	}

	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);
	
// update general area ambient sound sources
	S_UpdateAmbientSounds ();

	combine = NULL;

// update spatialization for static and dynamic sounds  
	ch = channels+NUM_AMBIENTS;
	for (i=NUM_AMBIENTS ; i<total_channels; i++, ch++)
	{
		if (!ch->sfx)
			continue;

#ifdef QUAKE1
		/* FS: FIXME: Use s_rawsamples stuff instead of this crap hack */
		if (warpspasm && (strstr(ch->sfx->name, "music/warp")) ) /* FS: Fucks up tunes if we allow spatial */
		{
			ch->leftvol = ch->master_vol * s_musicvolume->value;
			ch->rightvol = ch->master_vol * s_musicvolume->value;
			continue;
		}
#endif

		S_Spatialize(ch);         // respatialize channel
		if (!ch->leftvol && !ch->rightvol)
			continue;

	// try to combine static sounds with a previous channel of the same
	// sound effect so we don't mix five torches every frame
	
		if (i >= MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS)
		{
		// see if it can just use the last one
			if (combine && combine->sfx == ch->sfx)
			{
				combine->leftvol += ch->leftvol;
				combine->rightvol += ch->rightvol;
				ch->leftvol = ch->rightvol = 0;
				continue;
			}
		// search for one
			combine = channels+MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;
			for (j=MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS ; j<i; j++, combine++)
				if (combine->sfx == ch->sfx)
					break;
					
			if (j == total_channels)
			{
				combine = NULL;
			}
			else
			{
				if (combine != ch)
				{
					combine->leftvol += ch->leftvol;
					combine->rightvol += ch->rightvol;
					ch->leftvol = ch->rightvol = 0;
				}
				continue;
			}
		}
		
		
	}

//
// debugging output
//
	if (snd_show->intValue)
	{
		total = 0;
		ch = channels;
		for (i=0 ; i<total_channels; i++, ch++)
			if (ch->sfx && (ch->leftvol || ch->rightvol) )
			{
				Com_Printf ("%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name);
				total++;
			}
		
		Com_Printf ("----(%i)----\n", total);
	}
#ifdef OGG_SUPPORT
	S_OGGUpdateBackgroundTrack();
#endif
	S_StreamUpdate(); /* FS */

// mix some sound
	S_Update_();
}

void GetSoundtime(void)
{
	int		samplepos;
	static	int		buffers;
	static	int		oldsamplepos;
	int		fullsamples;

	fullsamples = dma.samples / dma.channels;

// it is possible to miscount buffers if it has wrapped twice between
// calls to S_Update.  Oh well.
	samplepos = SNDDMA_GetDMAPos();


	if (samplepos < oldsamplepos)
	{
		buffers++;                                      // buffer wrapped
		
		if (paintedtime > 0x40000000)
		{       // time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds ();
		}
	}
	oldsamplepos = samplepos;

	soundtime = buffers*fullsamples + samplepos/dma.channels;
}


void S_Update_(void)
{
	unsigned        endtime;
	int				samps;

	if (!sound_started)
		return;

	SNDDMA_BeginPainting ();

	if (!dma.buffer)
		return;

// Updates DMA time
	GetSoundtime();

// check to make sure that we haven't overshot
	if (paintedtime < soundtime)
	{
		Com_DPrintf(DEVELOPER_MSG_SOUND, "S_Update_ : overflow\n");
		paintedtime = soundtime;
	}

// mix ahead of current position
	endtime = soundtime + s_mixahead->value * dma.speed;
//endtime = (soundtime + 4096) & ~4095;

	// mix to an even submission block size
	endtime = (endtime + dma.submission_chunk-1)
		& ~(dma.submission_chunk-1);
	samps = dma.samples >> (dma.channels-1);
	if (endtime - soundtime > samps)
		endtime = soundtime + samps;

	S_PaintChannels (endtime);

	SNDDMA_Submit ();
}

/*
===============================================================================

console functions

===============================================================================
*/

void S_Play(void)
{
	static int hash=345;
	int     i;
	char name[256];
	sfx_t   *sfx;
	
	i = 1;
	while (i<Cmd_Argc())
	{
		if (!strrchr(Cmd_Argv(i), '.'))
		{
			Q_strlcpy(name, Cmd_Argv(i), sizeof(name));
			Q_strlcat(name, ".wav", sizeof(name));
		}
		else
		{
			Q_strlcpy(name, Cmd_Argv(i), sizeof(name));
		}
		sfx = S_PrecacheSound(name);
		S_StartSound(hash++, 0, sfx, listener_origin, 1.0, 1.0);
		i++;
	}
}

void S_Play2(void)
{
	static int hash=345;
	int     i;
	char name[256];
	sfx_t   *sfx;
	
	i = 1;
	while (i<Cmd_Argc())
	{
		if (!strrchr(Cmd_Argv(i), '.'))
		{
			Q_strlcpy(name, Cmd_Argv(i), sizeof(name));
			Q_strlcat(name, ".wav", sizeof(name));
		}
		else
		{
			Q_strlcpy(name, Cmd_Argv(i), sizeof(name));
		}
		sfx = S_PrecacheSound(name);
		S_StartSound(hash++, 0, sfx, listener_origin, 1.0, 0.0);
		i++;
	}
}

void S_PlayVol(void)
{
	static int hash=543;
	int i;
	float vol;
	char name[256];
	sfx_t   *sfx;
	
	i = 1;
	while (i<Cmd_Argc())
	{
		if (!strrchr(Cmd_Argv(i), '.'))
		{
			Q_strlcpy(name, Cmd_Argv(i), sizeof(name));
			Q_strlcat(name, ".wav", sizeof(name));
		}
		else
		{
			Q_strlcpy(name, Cmd_Argv(i), sizeof(name));
		}
		sfx = S_PrecacheSound(name);
		vol = atof(Cmd_Argv(i+1));
		S_StartSound(hash++, 0, sfx, listener_origin, vol, 1.0);
		i+=2;
	}
}

void S_SoundList(void)
{
	int		i;
	sfx_t	*sfx;
	sfxcache_t	*sc;
	int		size, total;

	total = 0;
	for (sfx=known_sfx, i=0 ; i<num_sfx ; i++, sfx++)
	{
		sc = Cache_Check(&sfx->cache);
		if (!sc)
			continue;
		size = sc->length*sc->width*(sc->stereo+1);
		total += size;
		if (sc->loopstart >= 0)
			Com_Printf ("L");
		else
			Com_Printf (" ");
		Com_Printf("(%2db) %6i : %s\n",sc->width*8,  size, sfx->name);
	}
	Com_Printf ("%i sounds, %i bytes\n", num_sfx, total); //johnfitz -- added count
}


void S_LocalSound (char *sound)
{
	sfx_t	*sfx;

	if (s_nosound->intValue)
		return;
	if (!sound_started)
		return;
		
	sfx = S_PrecacheSound (sound);
	if (!sfx)
	{
		Com_Printf ("S_LocalSound: can't cache %s\n", sound);
		return;
	}
	S_StartSound (cl.viewentity, -1, sfx, vec3_origin, 1, 1);
}

#ifdef GAMESPY
void S_GamespySound (char *sound) /* FS: Added */
{
	if (snd_gamespy_sounds->intValue)
		S_LocalSound(sound);
}
#endif

void S_MusicPause (void) /* FS */
{
	S_AmbientOff();
	CDAudio_Pause();
#ifdef OGG_SUPPORT
	S_PauseOGGBackgroundTrack();
#endif
	S_PauseStreamBackgroundTrack();
}

void S_MusicResume (void) /* FS */
{
	S_AmbientOn();
	CDAudio_Resume();
#ifdef OGG_SUPPORT
	S_ResumeOGGBackgroundTrack();
#endif
	S_ResumeStreamBackgroundTrack();
}

/* FS: So we can support both */
void S_StopBackgroundTrack(void)
{
	CDAudio_Stop();
#ifdef OGG_SUPPORT
	S_StopOGGBackgroundTrack();
#endif
	S_StopStreamBackgroundTrack();
}
