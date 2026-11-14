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

/* File menu: Open/Save BASIC program. Items post the same F5/F6 key events the main loop already handles. */
@interface E71MenuTarget : NSObject
- (void)openProg:(id)sender;
- (void)saveProg:(id)sender;
@end
static void post_fkey(SDL_Keycode k)
{
	SDL_Event e; SDL_zero(e);
	e.type = SDL_KEYDOWN; e.key.state = SDL_PRESSED; e.key.keysym.sym = k; e.key.keysym.scancode = SDL_GetScancodeFromKey(k);
	SDL_PushEvent(&e);
}
@implementation E71MenuTarget
- (void)openProg:(id)sender { post_fkey(SDLK_F5); }
- (void)saveProg:(id)sender { post_fkey(SDLK_F6); }
@end

void install_file_menu(void)
{
	static E71MenuTarget *tgt;
	NSMenu *bar = [NSApp mainMenu];
	if (!bar) return;
	tgt = [E71MenuTarget new];
	NSMenu *m = [[NSMenu alloc] initWithTitle:@"File"];
	NSMenuItem *o = [m addItemWithTitle:@"Open BASIC Program\u2026 (F5)" action:@selector(openProg:) keyEquivalent:@"o"];
	NSMenuItem *v = [m addItemWithTitle:@"Save BASIC Program\u2026 (F6)" action:@selector(saveProg:) keyEquivalent:@"s"];
	[o setTarget:tgt]; [v setTarget:tgt];
	NSMenuItem *top = [[NSMenuItem alloc] initWithTitle:@"File" action:nil keyEquivalent:@""];
	[top setSubmenu:m];
	[bar insertItem:top atIndex:1];
}
