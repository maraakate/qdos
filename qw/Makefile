CC   = gcc
STRIP= strip

# libcurl/http downloads:
USE_CURL=1
# gamespy server browser
USE_GAMESPY=1
# whether to use OGG VORBIS
USE_OGG=yes
# use tremor library for ogg/vorbis:
USE_TREMOR=1
# pci sound card support:
USE_SNDPCI=1
# pci sound card support as dxe3:
SNDPCI_DXE=1
# use SSE optimizations for P3 (could be dangerous)
USE_SSE=0
# compile Quake with fxMesa for 3DFX Voodoo Cards
GLQUAKE=0

# common objects shared by Quake 1 and QuakeWorld
COMMON=../common

ifeq ($(DEBUG),1)
CFLAGS  = -g -Wall -Werror -O0 -DDEBUG -DQUAKEWORLD -fno-strict-aliasing
CFLAGS += -fomit-frame-pointer
else
CFLAGS  = -g -Wall -Werror -O2 -DNDEBUG -DQUAKEWORLD -fno-strict-aliasing
CFLAGS += -fomit-frame-pointer
endif

CFLAGS += -I. -I../common
CFLAGS += -DWATT32_NO_OLDIES -I../3rdparty/watt32/inc
LDFLAGS =
LIBS    =
EXE     = qw.exe

CPU ?= pentium

ifeq ($(USE_SSE),1)
CFLAGS += -march=pentium3 -DSSE_AWARE
else
CFLAGS += -mtune=$(CPU)
endif

ifeq ($(GLQUAKE),1)
EXE     = qwdosfx.exe
CFLAGS += -I../3rdparty/include -I../3rdparty/include/glide3 -DGLQUAKE
CFLAGS += -DUSE_3DFXGAMMA
CFLAGS += -DGL_DLSYM
#LIBS   += -L../3rdparty/lib -lgl
SYSOBJ_DXE=dxe.o
# choose which dosgl api(s) to support. (at least one.)
CFLAGS += -DREFGL_FXMESA
CFLAGS += -DREFGL_MESA
CFLAGS += -DREFGL_SAGE
else
SYSOBJ_DXE=no_dxe.o
endif

ifeq ($(USE_SNDPCI),1)
CFLAGS += -DUSE_SNDPCI
CFLAGS += -I../3rdparty/include
ifeq ($(SNDPCI_DXE),1)
SYSOBJ_DXE=dxe.o
CFLAGS += -DSNDPCI_DXE
else
LIBS   += -L../3rdparty/lib -lau
endif
endif

ifeq ($(USE_OGG),yes)
CFLAGS += -DOGG_SUPPORT
CFLAGS += -I../3rdparty/include
ifneq ($(USE_TREMOR),1)
LIBS   += -L../3rdparty/lib -lvorbisfile -lvorbis -logg
else
CFLAGS += -DVORBIS_USE_TREMOR
LIBS   += -L../3rdparty/lib -lvorbisidec -logg
endif
endif

ifeq ($(USE_GAMESPY),1)
CFLAGS += -DGAMESPY
GAMESPY = darray.o    \
	gserver.o     \
	gserverlist.o \
	gutil.o       \
	hashtable.o   \
	nonport.o
endif

ifeq ($(USE_CURL),1)
CFLAGS += -DUSE_CURL -DCURL_NO_OLDIES
CFLAGS += -I../3rdparty/libcurl/include
LIBS   += -L../3rdparty/libcurl/lib -lcurl
endif
LIBS   += -L../3rdparty/watt32/lib -lwatt

LIBS   += -lc -lgcc
#LIBS   += -lm

# not too sophisticated dependency
CLIENT = $(SYSOBJ_DXE)       \
	$(GAMESPY)           \
	cd_audio.o \
	cl_cam.o   \
	cl_demo.o  \
	cl_ents.o  \
	cl_http.o  \
	cl_input.o \
	cl_main.o  \
	cl_parse.o \
	cl_pred.o  \
	cl_tent.o  \
	cmd.o      \
	cmd_auto.o \
	common.o   \
	console.o  \
	crc.o      \
	cvar.o     \
	dos_v2.o   \
	in_dos.o   \
	keys.o     \
	math.o     \
	mathlib.o  \
	md4.o      \
	menu.o     \
	net_chan.o \
	net_udp.o  \
	nonintel.o \
	pmove.o    \
	pmovetst.o \
	q_shdos.o  \
	r_part.o   \
	sbar.o     \
	skin.o     \
	snd_dma.o  \
	snd_dos.o  \
	snd_gus.o  \
	snd_mem.o  \
	snd_mix.o  \
	snd_mixa.o \
	snd_ogg.o \
	snd_pci.o  \
	snd_sb.o   \
	snd_strm.o \
	sys_dos.o  \
	sys_dosa.o \
	sys_find.o \
	view.o     \
	wad.o      \
	zone.o

ifeq ($(GLQUAKE),0)
REFSOFT = d_copy.o \
	d_draw.o   \
	d_draw16.o \
	d_edge.o   \
	d_fill.o   \
	d_init.o   \
	d_modech.o \
	d_part.o   \
	d_parta.o  \
	d_polysa.o \
	d_polyse.o \
	d_scan.o   \
	d_scana.o  \
	d_sky.o    \
	d_spr8.o   \
	d_sprite.o \
	d_surf.o   \
	d_vars.o   \
	d_varsa.o  \
	d_zpoint.o \
	draw.o     \
	model.o    \
	r_aclip.o  \
	r_aclipa.o \
	r_alias.o  \
	r_aliasa.o \
	r_bsp.o    \
	r_draw.o   \
	r_drawa.o  \
	r_edge.o   \
	r_edgea.o  \
	r_efrag.o  \
	r_light.o  \
	r_main.o   \
	r_misc.o   \
	r_screen.o \
	r_sky.o    \
	r_sprite.o \
	r_surf.o   \
	r_vars.o   \
	r_varsa.o  \
	surf16.o   \
	surf8.o    \
	vid_dos.o  \
	vid_ext.o  \
	vid_vga.o  \
	vregset.o
endif

ifeq ($(GLQUAKE),1)
REFGL = dos_dmesa.o  \
	dos_fxmesa.o \
	dos_sage.o   \
	fx_gamma.o   \
	gl_draw.o    \
	gl_mesh.o    \
	gl_model.o   \
	gl_ngraph.o  \
	gl_refrag.o  \
	gl_rlight.o  \
	gl_rmain.o   \
	gl_rmisc.o   \
	gl_rsurf.o   \
	gl_screen.o  \
	gl_test.o    \
	gl_warp.o    \
	gl_viddos.o
endif

REMOVEOBJS = rm -f *.o

OBJECTS = $(CLIENT) $(REFSOFT) $(REFGL)

all:     qw

clean:
	$(REMOVEOBJS)

realclean:
	$(REMOVEOBJS)
	rm -f *.exe

qw: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) $(LIBS) -o $(EXE)

DO_AS=$(CC) -x assembler-with-cpp

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@
%.o : $(COMMON)/%.c
	$(CC) $(CFLAGS) -c $< -o $@
%.o: %.S
	$(DO_AS) $(CFLAGS) -c $< -o $@
%.o: $(COMMON)/%.S
	$(DO_AS) $(CFLAGS) -c $< -o $@
%.o: %.s
	$(DO_AS) $(CFLAGS) -c $< -o $@
%.o: $(COMMON)/%.s
	$(DO_AS) $(CFLAGS) -c $< -o $@
%.o : $(COMMON)/Goa/%.c
	$(CC) $(CFLAGS) -c $< -o $@
%.o : $(COMMON)/Goa/CEngine/%.c
	$(CC) $(CFLAGS) -c $< -o $@
%.o : $(COMMON)/Goa/Sdk/%.c
	$(CC) $(CFLAGS) -c $< -o $@
