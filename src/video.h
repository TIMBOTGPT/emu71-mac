/* video.h - external video device for the Emu71 macOS port (see docs/video-architecture.md) */
#ifndef EMU71_VIDEO_H
#define EMU71_VIDEO_H

#include <SDL.h>

#ifndef TYPE_VIDEO
#define TYPE_VIDEO 5				/* module type value, after TYPE_HPIL in enum MODULETYPE */
#endif

#define VID_NIBBLES 128				/* mailbox window size in nibbles (power of 2) */
#define VID_DEV_ID  0x5				/* MMIO device ID nibble (1, 3-F are unassigned in HP's list) */

/* called from the generated mops.c (MountPorts) */
HANDLE AllocVideoMem(UINT nType);
BOOL   AttachVideoMem(PPORTACC *ppsPort, HANDLE h);

/* called from main.c */
void VideoSetSize(int w, int h);	/* before MountPorts */
void VideoPump(void);				/* main thread, once per loop: window create/hide/redraw */
int  VideoEvent(SDL_Event *e);		/* returns 1 if the event was consumed */
void VideoShutdown(void);
void VideoSnapshot(const char *ppmPath);
void VideoSelfTest(const char *ppmPath);	/* drives the mailbox through the module's own port entry points */

#endif
