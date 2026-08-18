#->=========================================<-
#->= MorphUAE - MUI'fied E-UAE for MorphOS =<-
#->=========================================<-
#->= Version  : 1.0                        =<-
#->= File     : Makefile                   =<-
#->= Author   : Stefan Blixth              =<-
#->= Compiled : 2026-04-23                 =<-
#->=========================================<-

#
# Application specific stuff
#
APPNAME		= MorphUAE
APP_MORPHOS	= $(APPNAME)
APP_MORPHOS_DB	= $(APP_MORPHOS)_db


#
# Compiler tools assign
#
CC_MORPHOS	= ppc-morphos-gcc-9
C++_MORPHOS	= ppc-morphos-g++-9
STRIP_MORPHOS	= ppc-morphos-strip
AR_MORPHOS	= ppc-morphos-ar
RANLIB_MORPHOS	= ppc-morphos-ranlib

#
# Libs and Objects
#
OBJ_MACHDEP	= support.o
LIB_MACHDEP	= libmachdep.a

OBJ_THREADDEP	= thread.o
LIB_THREADDEP	= libthreaddep.a

OBJ_GFXDEP	= morphos-win.o LEDmcc.o
LIB_GFXDEP	= libgfxdep.a

OBJ_SOUNDDEP	= sound.o
LIB_SOUNDDEP	= libsounddep.a

OBJ_JOYDEP	= joystick.o
LIB_JOYDEP	= libjoydep.a

OBJ_GUIDEP	= morphos-gui.o
LIB_GUIDEP	= libguidep.a

OBJ_OSDEP	= od-main.o od-memory.o od-support.o ami-disk.o blkdev-amiga.o
LIB_OSDEP	= libosdep.a

OBJ_KEYMAP	= amiga_rawkeys.o
LIB_KEYMAP	= libkeymap.a

OBJ_DMS		= crc_csum.o getbits.o maketbl.o pfile.o tables.o u_deep.o u_heavy.o u_init.o u_medium.o u_quick.o u_rle.o
LIB_DMS		= libdms.a

OBJ_CAPS	= caps.o
LIB_CAPS	= libcaps.a

OBJ_CPUEMU	= cpuemu_0.o cpuemu_5.o cpuemu_6.o fpp.o compstbl.o compemu_macroblocks_ppc.o compemu_compiler_ppc.o compemu_support.o
LIB_CPUEMU	= libcpuemu.a

OBJ_MAIN	= main.o newcpu.o memory.o events.o custom.o serial.o cia.o blitter.o autoconf.o traps.o ersatz.o keybuf.o expansion.o zfile.o cfgfile.o picasso96.o inputdevice.o gfxutil.o audio.o sinctable.o drawing.o native2amiga.o disk.o crc32.o savestate.o unzip.o uaeexe.o uaelib.o fdi2raw.o hotkeys.o ar.o driveclick.o enforcer.o misc.o missing.o readcpu.o blitfunc.o blittable.o cpustbl.o cpudefs.o writelog.o filesys.o fsdb.o fsusage.o hardfile.o filesys_unix.o fsdb_unix.o hardfile_unix.o bsdsocket.o scsi-none.o debug.o ppc_disasm.o identify.o


INCDIR		= ./include

#
# Platform specific compiler and linker flags 
#
#CFLG_MOS	= -DHAVE_CONFIG_H -g -O2 -noixemul -Wa,--execstack  -fomit-frame-pointer -Wno-unused -Wno-format -W -Wmissing-prototypes -Wstrict-prototypes -Wimplicit-function-declaration -Wno-implicit
CFLG_MOS	= -DHAVE_CONFIG_H -DUSE_SAVESTATE -O2 -noixemul -Wno-unused -Wimplicit-function-declaration
#CPPFLG_MOS	= -DUSEDEBUG -D__AMIGADATE__=\"$(shell date "+%d.%m.%y")\"
#LFLG_MOS	= -lz -lm -ldebug
CPPFLG_MOS	= -D__AMIGADATE__=\"$(shell date "+%d.%m.%y")\"
LFLG_MOS	= -lz -lm
#LFLG_MOS	= -lz -lm -maltivec -mabi=altivec

.PHONY:	clean usage


usage:
	@echo ""
	@echo "  Application - $(APPNAME)"
	@echo " +-------------------------------------------------------------------+"
	@echo " | clean       - Deletes all files in the obj & release-directories. |"
	@echo " | all         - Make all of the options below                       |"
	@echo " +-------------------------------------------------------------------+"
	@echo " | morphos     - Compiles a binary for MorphOS.                      |"
	@echo " | debug       - Compiles a debug-enabled binary for MorphOS.        |"
	@echo " +-------------------------------------------------------------------+"
	@echo ""

all:	morphos morphos_db

clean:
	@echo "Cleaning up..."
	@echo ""
	rm -f *.a
	rm -f *.o
	@echo ""
	@echo "Done."

debug: 	$(LIB_MACHDEP) $(LIB_THREADDEP) $(LIB_GFXDEP) $(LIB_SOUNDDEP) $(LIB_JOYDEP) $(LIB_GUIDEP) $(LIB_OSDEP) $(LIB_KEYMAP) $(LIB_DMS) $(LIB_CAPS) $(LIB_CPUEMU) $(OBJ_MAIN)
	@echo ""
	@echo "Debug-enabled MorphOS binary sucessfully built..."
	@echo ""

morphos: $(LIB_MACHDEP) $(LIB_THREADDEP) $(LIB_GFXDEP) $(LIB_SOUNDDEP) $(LIB_JOYDEP) $(LIB_GUIDEP) $(LIB_OSDEP) $(LIB_KEYMAP) $(LIB_DMS) $(LIB_CAPS) $(LIB_CPUEMU) $(OBJ_MAIN)
	@echo ""

	$(CC_MORPHOS) $(CFLG_MOS) -o $(APPNAME) main.o newcpu.o memory.o events.o custom.o serial.o cia.o blitter.o autoconf.o traps.o ersatz.o keybuf.o expansion.o zfile.o cfgfile.o picasso96.o inputdevice.o gfxutil.o audio.o sinctable.o drawing.o native2amiga.o disk.o crc32.o savestate.o unzip.o uaeexe.o uaelib.o fdi2raw.o hotkeys.o ar.o driveclick.o enforcer.o misc.o missing.o readcpu.o libmachdep.a libjoydep.a libsounddep.a libgfxdep.a libguidep.a libkeymap.a libdms.a libcaps.a blitfunc.o blittable.o cpustbl.o cpudefs.o libcpuemu.a writelog.o filesys.o fsdb.o fsusage.o hardfile.o filesys_unix.o fsdb_unix.o hardfile_unix.o bsdsocket.o scsi-none.o debug.o ppc_disasm.o identify.o libthreaddep.a libosdep.a $(LFLG_MOS)
	$(STRIP_MORPHOS) $(APPNAME)
	@echo "MorphOS binary sucessfully built..."
	@echo ""


#OBJ_MAIN
main.o: main.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
newcpu.o: newcpu.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
memory.o: memory.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
events.o: events.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
custom.o: custom.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
serial.o: serial.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
cia.o: cia.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
blitter.o: blitter.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
autoconf.o: autoconf.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
traps.o: traps.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
ersatz.o: ersatz.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
keybuf.o: keybuf.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
expansion.o: expansion.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
zfile.o: zfile.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
cfgfile.o: cfgfile.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
picasso96.o: picasso96.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
inputdevice.o: inputdevice.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
gfxutil.o: gfxutil.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
audio.o: audio.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
sinctable.o: sinctable.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
drawing.o: drawing.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
native2amiga.o: native2amiga.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
disk.o: disk.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
crc32.o: crc32.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
savestate.o: savestate.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
unzip.o: unzip.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
uaeexe.o: uaeexe.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
uaelib.o: uaelib.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
fdi2raw.o: fdi2raw.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
hotkeys.o: hotkeys.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
ar.o: ar.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
driveclick.o: driveclick.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
enforcer.o: enforcer.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
misc.o: misc.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
missing.o: missing.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
readcpu.o: readcpu.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
blitfunc.o: blitfunc.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
blittable.o: blittable.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
cpustbl.o: cpustbl.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
cpudefs.o: cpudefs.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
writelog.o: writelog.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
filesys.o: filesys.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
fsdb.o: fsdb.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
fsusage.o: fsusage.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
hardfile.o: hardfile.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
filesys_unix.o: filesys_unix.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
fsdb_unix.o: fsdb_unix.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
hardfile_unix.o: hardfile_unix.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
bsdsocket.o: bsdsocket.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
scsi-none.o: scsi-none.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
debug.o: debug.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
ppc_disasm.o: ppc_disasm.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
identify.o: identify.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@


$(LIB_CPUEMU): $(OBJ_CPUEMU)
	$(AR_MORPHOS) cru $(LIB_CPUEMU) $(OBJ_CPUEMU)
	$(RANLIB_MORPHOS) $(LIB_CPUEMU)


#OBJ_CPUEMU
cpuemu_0.o: cpuemu_0.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
cpuemu_5.o: cpuemu_5.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
cpuemu_6.o: cpuemu_6.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
fpp.o: fpp.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
compstbl.o: compstbl.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
compemu_macroblocks_ppc.o: compemu_macroblocks_ppc.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
compemu_compiler_ppc.o: compemu_compiler_ppc.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
compemu_support.o: compemu_support.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@


$(LIB_CAPS): $(OBJ_CAPS)
	$(AR_MORPHOS) cru $(LIB_CAPS) $(OBJ_CAPS)
	$(RANLIB_MORPHOS) $(LIB_CAPS)

$(OBJ_CAPS): caps.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@


$(LIB_DMS): $(OBJ_DMS)
	$(AR_MORPHOS) cru $(LIB_DMS) $(OBJ_DMS)
	$(RANLIB_MORPHOS) $(LIB_DMS)

#OBJ_DMS

crc_csum.o: ./dms/crc_csum.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
getbits.o: ./dms/getbits.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
maketbl.o: ./dms/maketbl.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
pfile.o: ./dms/pfile.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
tables.o: ./dms/tables.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
u_deep.o: ./dms/u_deep.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
u_heavy.o: ./dms/u_heavy.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
u_init.o: ./dms/u_init.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
u_medium.o: ./dms/u_medium.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
u_quick.o: ./dms/u_quick.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
u_rle.o: ./dms/u_rle.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@


$(LIB_KEYMAP): $(OBJ_KEYMAP)
	$(AR_MORPHOS) cru $(LIB_KEYMAP) $(OBJ_KEYMAP)
	$(RANLIB_MORPHOS) $(LIB_KEYMAP)

$(OBJ_KEYMAP): amiga_rawkeys.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@


$(LIB_OSDEP): $(OBJ_OSDEP)
	$(AR_MORPHOS) cru $(LIB_OSDEP) $(OBJ_OSDEP)
	$(RANLIB_MORPHOS) $(LIB_OSDEP)

#OBJ_OSDEP
od-main.o: od-main.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
od-memory.o: od-memory.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
od-support.o: od-support.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
ami-disk.o: ami-disk.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@
blkdev-amiga.o: blkdev-amiga.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@


$(LIB_GUIDEP): $(OBJ_GUIDEP)
	$(AR_MORPHOS) cru $(LIB_GUIDEP) $(OBJ_GUIDEP)
	$(RANLIB_MORPHOS) $(LIB_GUIDEP)

$(OBJ_GUIDEP):	morphos-gui.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@


$(LIB_JOYDEP): $(OBJ_JOYDEP)
	$(AR_MORPHOS) cru $(LIB_JOYDEP) $(OBJ_JOYDEP)
	$(RANLIB_MORPHOS) $(LIB_JOYDEP)

$(OBJ_JOYDEP):	joystick.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@


$(LIB_SOUNDDEP): $(OBJ_SOUNDDEP)
	$(AR_MORPHOS) cru $(LIB_SOUNDDEP) $(OBJ_SOUNDDEP)
	$(RANLIB_MORPHOS) $(LIB_SOUNDDEP)

$(OBJ_SOUNDDEP): sound.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@


$(LIB_GFXDEP): $(OBJ_GFXDEP)
	$(AR_MORPHOS) cru $(LIB_GFXDEP) $(OBJ_GFXDEP)
	$(RANLIB_MORPHOS) $(LIB_GFXDEP)


#$OBJ_GFXDEP
morphos-win.o: morphos-win.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@

LEDmcc.o: LEDmcc.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@


$(LIB_THREADDEP): $(OBJ_THREADDEP)
	$(AR_MORPHOS) cru $(LIB_THREADDEP) $(OBJ_THREADDEP)
	$(RANLIB_MORPHOS) $(LIB_THREADDEP)

$(OBJ_THREADDEP): thread.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@


$(LIB_MACHDEP): $(OBJ_MACHDEP)
	$(AR_MORPHOS) cru $(LIB_MACHDEP) $(OBJ_MACHDEP)
	$(RANLIB_MORPHOS) $(LIB_MACHDEP)

$(OBJ_MACHDEP): support.c
	$(CC_MORPHOS) -I$(INCDIR) $(CPPFLG_MOS) $(CFLG_MOS) -c $^ -o $@


