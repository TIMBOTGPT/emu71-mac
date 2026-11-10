/* Lock the window's content aspect ratio natively (NSWindow), so resizing
   never letterboxes or pillarboxes. */
#import <Cocoa/Cocoa.h>
#include <SDL.h>
#include <SDL_syswm.h>

void lock_aspect(SDL_Window *win, int w, int h)
{
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if (SDL_GetWindowWMInfo(win, &info) && info.subsystem == SDL_SYSWM_COCOA) {
		NSWindow *nw = info.info.cocoa.window;
		[nw setContentAspectRatio:NSMakeSize(w, h)];
	}
}
