/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#include "Menu.h"
#include "Manager.h"
#include "Client.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <X11/keysym.h>

Boolean       Menu::m_initialised = False;
GC           *Menu::m_menuGC;
#ifdef CONFIG_USE_XFT
XftFont      *Menu::m_font;
XftColor     *Menu::m_xftColour;
XftDraw     **Menu::m_xftDraw;
#else
XFontSet      Menu::m_fontset;
XFontStruct **Menu::m_font;
#endif
unsigned long Menu::m_foreground;
unsigned long Menu::m_background;
unsigned long Menu::m_border;
Window       *Menu::m_window;

#if MENU_ENTRY_MAXLENGTH > 0
#define STRLEN_MITEMS(i) ((strlen(m_items[(i)]) > MENU_ENTRY_MAXLENGTH) ? \
                          MENU_ENTRY_MAXLENGTH : strlen(m_items[(i)]))
#else
#define STRLEN_MITEMS(i) (strlen(m_items[(i)]))
#endif


Menu::Menu(WindowManager *manager, XEvent *e)
    : m_items(0), m_nItems(0), m_nHidden(0),
      m_hasSubmenus(False),
      m_windowManager(manager),
      m_event(e)
{
    if (!m_initialised)
    {
	XGCValues *values;
	XSetWindowAttributes *attr;

        m_menuGC = (GC *) malloc(m_windowManager->screensTotal() * sizeof(GC));
        m_window = (Window *) malloc(m_windowManager->screensTotal() *
				     sizeof(Window));
#ifdef CONFIG_USE_XFT
	char *fi = strdup(CONFIG_MENU_FONT);
	char *ffi = fi, *tokstr = fi;
	while ((fi = strtok(tokstr, ","))) {
		
	    fprintf(stderr, "fi = \"%s\"\n", fi);
	    tokstr = 0;
	    
	    FcPattern *pattern = FcPatternCreate();
	    FcPatternAddString(pattern, FC_FAMILY, (FcChar8 *)fi);
	    FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);

#ifndef FC_WEIGHT_REGULAR
#define FC_WEIGHT_REGULAR FC_WEIGHT_MEDIUM
#endif
	    FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_REGULAR);
	    FcPatternAddInteger(pattern, FC_PIXEL_SIZE, CONFIG_MENU_FONT_SIZE);
	    FcConfigSubstitute(FcConfigGetCurrent(), pattern, FcMatchPattern);

	    FcResult result = FcResultMatch;
	    FcPattern *match = FcFontMatch(FcConfigGetCurrent(), pattern, &result);
	    FcPatternDestroy(pattern);

	    if (!match || result != FcResultMatch) {
		if (match) FcPatternDestroy(match);
		continue;
	    }

	    m_font = XftFontOpenPattern(display(), match);
	    if (m_font) break;
	    FcPatternDestroy(match);
	}
	free(ffi);
	if (!m_font) {
	    m_windowManager->fatal("couldn't load menu Xft font");
	}
	m_xftColour = (XftColor *) malloc(m_windowManager->screensTotal() *
					  sizeof(XftColor));
	m_xftDraw = (XftDraw **) malloc(m_windowManager->screensTotal() *
					sizeof(XftDraw *));
#else
	m_font = (XFontStruct **) malloc(m_windowManager->screensTotal() *
					 sizeof(XFontStruct *));
#endif
	values = (XGCValues *) malloc(m_windowManager->screensTotal() *
				      sizeof(XGCValues));
	attr = (XSetWindowAttributes *) malloc(m_windowManager->screensTotal() *
				      sizeof(XSetWindowAttributes));
	
        for (int i = 0; i < m_windowManager->screensTotal(); i++)
	{
	    m_foreground = m_windowManager->allocateColour
	      (i, CONFIG_MENU_FOREGROUND, "menu foreground");
	    m_background = m_windowManager->allocateColour
	      (i, CONFIG_MENU_BACKGROUND, "menu background");
	    m_border = m_windowManager->allocateColour
	      (i, CONFIG_MENU_BORDERS, "menu border");

#ifndef CONFIG_USE_XFT
	    char **ml;
	    int mc;
	    char *ds;
	    
	    m_fontset = XCreateFontSet(display(), CONFIG_NICE_MENU_FONT,
				       &ml, &mc, &ds);
	    if (!m_fontset)
	      m_fontset = XCreateFontSet(display(), CONFIG_NASTY_FONT,
					 &ml, &mc, &ds);
	    if (m_fontset) {
		XFontStruct **fs_list;
		XFontsOfFontSet(m_fontset, &fs_list, &ml);
		m_font[i] = fs_list[0];
	    } else {
		m_font[i] = NULL;
	    }
#define XDrawString(t,u,v,w,x,y,z) XmbDrawString(t,u,m_fontset,v,w,x,y,z)
	    if (!m_font[i]) m_windowManager->fatal("couldn't load menu font\n");
#endif
	    
	    values[i].background = m_background;
	    values[i].foreground = m_foreground ^ m_background;
	    values[i].function = GXxor;
	    values[i].line_width = 0;
	    values[i].subwindow_mode = IncludeInferiors;
#ifndef CONFIG_USE_XFT
	    values[i].font = m_font[i]->fid;
#endif
	    m_menuGC[i] = XCreateGC
	      (display(), m_windowManager->mroot(i),
	       GCForeground | GCBackground | GCFunction |
	       GCLineWidth | GCSubwindowMode, &values[i]);

#ifndef CONFIG_USE_XFT
	    XChangeGC(display(), Border::drawGC(m_windowManager, i),
		      GCFont, &values[i]);
#endif

	    m_window[i] = XCreateSimpleWindow
	      (display(), m_windowManager->mroot(i), 0, 0, 1, 1, 1,
	       m_border, m_background);

	    
#if ( CONFIG_USE_PIXMAPS != False ) && ( CONFIG_USE_PIXMAP_MENUS != False )
	    attr[i].background_pixmap = Border::backgroundPixmap(manager);
#endif
	    attr[i].save_under =
	      (DoesSaveUnders(ScreenOfDisplay(display(), i)) ?
	     True : False);

#if ( CONFIG_USE_PIXMAPS != False ) && ( CONFIG_USE_PIXMAP_MENUS != False )
	    XChangeWindowAttributes
	      (display(), m_window[i], CWBackPixmap, &attr[i]);
#endif
	    XChangeWindowAttributes
	      (display(), m_window[i], CWSaveUnder, &attr[i]);

#ifdef CONFIG_USE_XFT
	    XftColorAllocName
		(display(),
		 XDefaultVisual(display(), i),
		 XDefaultColormap(display(), i),
		 CONFIG_MENU_FOREGROUND,
		 &m_xftColour[i]);

	    m_xftDraw[i] = XftDrawCreate(display(), m_window[i],
					 XDefaultVisual(display(), i),
					 XDefaultColormap(display(), i));
#endif
	}
	m_initialised = True;
    }
}

Menu::~Menu()
{
    XUnmapWindow(display(), m_window[screen()]);
}

int Menu::getTextWidth(char *text, unsigned int len)
{
#ifdef CONFIG_USE_XFT
    XGlyphInfo extents;
    XftTextExtentsUtf8(display(), m_font, (FcChar8 *)text, len, &extents);
    return extents.width;
#else
    return XmbTextEscapement(m_fontset, text, len);
#endif
}

void Menu::cleanup(WindowManager *const wm)
{
    if (m_initialised) { // fix due to Eric Marsden
#ifdef CONFIG_USE_XFT
	XftFontClose(wm->display(), m_font);
#endif
        for (int i = 0; i < wm->screensTotal(); i++) {
#ifdef CONFIG_USE_XFT
	    XftDrawDestroy(m_xftDraw[i]);
#else
	    XFreeFontSet(wm->display(), m_fontset);
#endif
	    XFreeGC(wm->display(), m_menuGC[i]);
	}
    }
}

int nobuttons(XButtonEvent *e) // straight outta 9wm
{
    int state;
    state = (e->state & AllButtonMask);
    return (e->type == ButtonRelease) && (state & (state - 1)) == 0;
}

int Menu::getSelection()
{
    m_items = getItems(&m_nItems, &m_nHidden);
    XButtonEvent *xbev = (XButtonEvent *)m_event; // KeyEvent is similar enough

    if (xbev->window == m_window[screen()] || m_nItems == 0) return -1;

    int width, maxWidth = 10;
    for (int i = 0; i < m_nItems; i++) {
	width = getTextWidth(m_items[i], STRLEN_MITEMS(i));
	if (width > maxWidth) maxWidth = width;
    }
    maxWidth += 32;

    Boolean isKeyboardMenu = isKeyboardMenuEvent(m_event);
    int selecting = isKeyboardMenu ? 0 : -1, prev = -1;
#ifdef CONFIG_USE_XFT
    int entryHeight = m_font->ascent + m_font->descent + 4;
#else
    int entryHeight = m_font[screen()]->ascent + m_font[screen()]->descent + 4;
#endif
    int totalHeight = entryHeight * m_nItems + 13;

    int mx = DisplayWidth (display(), screen()) - 1;
    int my = DisplayHeight(display(), screen()) - 1;

    int x, y;

    if (isKeyboardMenu) {
	x = mx / 2 - maxWidth / 2;
	y = my / 2 - totalHeight / 2;
    } else {

	x = xbev->x - maxWidth/2;
	y = xbev->y - 2;

	Boolean warp = False;

	if (x < 0) {
	    xbev->x -= x;
	    x = 0;
	    warp = True;
	} else if (x + maxWidth >= mx) {
	    xbev->x -= x + maxWidth - mx;
	    x = mx - maxWidth;
	    warp = True;
	}
    
	if (y < 0) {
	    xbev->y -= y;
	    y = 0;
	    warp = True;
	} else if (y + totalHeight >= my) {
	    xbev->y -= y + totalHeight - my;
	    y = my - totalHeight;
	    warp = True;
	}

	if (warp) XWarpPointer(display(), None, root(), None, None,
			       None, None, xbev->x, xbev->y);
    }

    XMoveResizeWindow(display(), m_window[screen()], x, y, maxWidth, totalHeight);
    XSelectInput(display(), m_window[screen()], MenuMask);
    XMapRaised(display(), m_window[screen()]);

    if (m_windowManager->attemptGrab(m_window[screen()], None,
				     MenuGrabMask, xbev->time)
	!= GrabSuccess) {
	XUnmapWindow(display(), m_window[screen()]);
	return -1;
    }
    
    if (isKeyboardMenu) {
	if (m_windowManager->attemptGrabKey(m_window[screen()], xbev->time)
	    != GrabSuccess) {
	    XUnmapWindow(display(), m_window[screen()]);
	    return -1;
	}
    }	    

    Boolean done = False;
    Boolean drawn = False;
    XEvent event;
    struct timeval sleepval;
    unsigned long tdiff = 0L;
    Boolean speculating = False;
    Boolean foundEvent;

    while (!done)
    {
	int i;
	foundEvent = False;

	if (CONFIG_FEEDBACK_DELAY >= 0 &&
	    tdiff > (unsigned long)CONFIG_FEEDBACK_DELAY &&
	    !isKeyboardMenu && // removeFeedback didn't seem to work for it
	    !speculating) {

	    if (selecting >= 0 && selecting < m_nItems) {
		raiseFeedbackLevel(selecting);
		XRaiseWindow(display(), m_window[screen()]);
	    }

	    speculating = True;
	}

	//!!! MenuMask | ??? suggests MenuMask is wrong
	while (XCheckMaskEvent
	       (display(), MenuMask | StructureNotifyMask |
		KeyPressMask | KeyReleaseMask, &event)) {
	    foundEvent = True;
	    if (event.type != MotionNotify) break;
	}

	if (!foundEvent) {
	    sleepval.tv_sec = 0;
	    sleepval.tv_usec = 10000;
	    select(0, 0, 0, 0, &sleepval);
	    tdiff += 10;
	    continue;
	}
	
	switch (event.type)
	{
	case ButtonPress:
	    break;
	    
	case ButtonRelease:
	    if (isKeyboardMenu) break;

	    if (drawn) {

		if (event.xbutton.button != xbev->button) break;
		x = event.xbutton.x;
		y = event.xbutton.y - 11;
		i = y / entryHeight;
		
		if (selecting >= 0 && y >= selecting * entryHeight - 3 &&
		    y <= (selecting + 1) * entryHeight - 3) i = selecting;

		if (m_hasSubmenus && (i >= 0 && i < m_nHidden)) i = -i;
		
		if (x < 0 || x > maxWidth || y < -3) i = -1;
		else if (i < 0 || i >= m_nItems) i = -1;
		
	    } else {
		i = -1;
	    }
	    
	    if (!nobuttons(&event.xbutton)) i = -1;
	    m_windowManager->releaseGrab(&event.xbutton);
	    XUnmapWindow(display(), m_window[screen()]);
	    selecting = i;
	    done = True;
	    break;

	case MotionNotify:
	    if (!drawn || isKeyboardMenu) break;

	    x = event.xbutton.x;
	    y = event.xbutton.y - 11;
	    prev = selecting;
	    selecting = y / entryHeight;

	    if (prev >= 0 && y >= prev * entryHeight - 3 &&
		y <= (prev+1) * entryHeight - 3) selecting = prev;

	    if (m_hasSubmenus && (selecting >= 0 && selecting < m_nHidden) &&
		x >= maxWidth-32 && x < maxWidth)
	    {
		xbev->x += event.xbutton.x - 32;
		xbev->y += event.xbutton.y;
		
		createSubmenu ((XEvent *)xbev, selecting);
		done = True;
		break;
	    }
	    
	    if (x < 0 || x > maxWidth || y < -3) selecting = -1;
	    else if (selecting < 0 || selecting > m_nItems) selecting = -1;

	    if (selecting == prev) break;
	    tdiff = 0; speculating = False;

	    if (prev >= 0 && prev < m_nItems) {
		removeFeedback(prev, speculating);
		XFillRectangle(display(), m_window[screen()], m_menuGC[screen()],
			       4, prev * entryHeight + 9,
			       maxWidth - 8, entryHeight);
	    }

	    if (selecting >= 0 && selecting < m_nItems) {
		showFeedback(selecting);
		XRaiseWindow(display(), m_window[screen()]);
		XFillRectangle(display(), m_window[screen()], m_menuGC[screen()],
			       4, selecting * entryHeight + 9,
			       maxWidth - 8, entryHeight);
	    }
	    
	    break;
			
	case Expose:

	    if (CONFIG_MAD_FEEDBACK && event.xexpose.window != m_window[screen()]) {
		m_windowManager->dispatchEvent(&event);
		break;
	    }

	    XClearWindow(display(), m_window[screen()]);
			
	    XDrawRectangle(display(), m_window[screen()], m_menuGC[screen()], 2, 7,
			   maxWidth - 5, totalHeight - 10);

	    for (i = 0; i < m_nItems; i++) {

		int dx = getTextWidth(m_items[i], STRLEN_MITEMS(i));
#ifdef CONFIG_USE_XFT
		int dy = i * entryHeight + m_font->ascent + 10;
#else
		int dy = i * entryHeight + m_font[screen()]->ascent + 10;
#endif

		if (i >= m_nHidden) {
#ifdef CONFIG_USE_XFT
		    XftDrawStringUtf8(m_xftDraw[screen()],
				      &m_xftColour[screen()],
				      m_font,
				      maxWidth - 8 - dx, dy,
				      (FcChar8 *)m_items[i], STRLEN_MITEMS(i));
#else
		    XDrawString(display(), m_window[screen()],
				Border::drawGC(m_windowManager,screen()),
				maxWidth - 8 - dx, dy,
				m_items[i], STRLEN_MITEMS(i));
#endif
		} else {
#ifdef CONFIG_USE_XFT
		    XftDrawStringUtf8(m_xftDraw[screen()],
				      &m_xftColour[screen()],
				      m_font,
				      8, dy,
				      (FcChar8 *)m_items[i], STRLEN_MITEMS(i));
#else
		    XDrawString(display(), m_window[screen()],
				Border::drawGC(m_windowManager,screen()),
				8, dy, m_items[i], STRLEN_MITEMS(i));
#endif
		}
	    }

	    if (selecting >= 0 && selecting < m_nItems) {
		XFillRectangle(display(), m_window[screen()], m_menuGC[screen()],
			       4, selecting * entryHeight + 9,
			       maxWidth - 8, entryHeight);
	    }

	    drawn = True;
	    break;

	case KeyPress:
	{
	    if (!isKeyboardMenu) break;

	    KeySym key = XKeycodeToKeysym(display(), event.xkey.keycode, 0);

	    if (key == CONFIG_MENU_SELECT_KEY) {

		if (!drawn) selecting = -1;

		if (m_hasSubmenus && selecting >= 0 && selecting < m_nHidden)
		{
		    createSubmenu((XEvent *)xbev, selecting);
		    selecting = -1;
		}
		m_windowManager->releaseGrabKeyMode(&event.xkey);
		XUnmapWindow(display(), m_window[screen()]);
		done = True;
		break;

	    } else if (key == CONFIG_MENU_CANCEL_KEY) {

		m_windowManager->releaseGrabKeyMode(&event.xkey);
		XUnmapWindow(display(), m_window[screen()]);
		if (selecting >= 0) removeFeedback(selecting, speculating);
		selecting = -1;
		done = True;
		break;

	    } else if (key != CONFIG_MENU_UP_KEY &&
		       key != CONFIG_MENU_DOWN_KEY) {
		break;
	    }

	    if (!drawn) break;
	    prev = selecting;

	    if (key == CONFIG_MENU_UP_KEY) {

		if (prev <= 0) selecting = m_nItems - 1;
		else selecting = prev - 1;

	    } else {

		if (prev == m_nItems - 1 || prev < 0) selecting = 0;
		else selecting = prev + 1;
	    }

	    tdiff = 0; speculating = False;
	    
	    if (prev >= 0 && prev < m_nItems) {
	    	removeFeedback(prev, speculating);
		XFillRectangle(display(), m_window[screen()], m_menuGC[screen()],
			       4, prev * entryHeight + 9,
			       maxWidth - 8, entryHeight);
	    }

	    if (selecting >= 0 && selecting < m_nItems) {
		showFeedback(selecting);
		XRaiseWindow(display(), m_window[screen()]);
		XFillRectangle(display(), m_window[screen()], m_menuGC[screen()],
			       4, selecting * entryHeight + 9,
			       maxWidth - 8, entryHeight);
	    }

	    break;
	}

	case KeyRelease:
	    break;

	default:
	    if (event.xmap.window == m_window[screen()]) break;
	    m_windowManager->dispatchEvent(&event);
	}
    }

    if (selecting >= 0) removeFeedback(selecting, speculating);
    return selecting;
}


ClientMenu::ClientMenu(WindowManager *manager, XEvent *e)
    : Menu(manager, e), m_allowExit(False)
{
    //!!! this is effectively calling a pure virtual from a constructor,
    // which is a bit gross
    int selecting = getSelection();

    if (selecting == m_nItems-1 && m_allowExit) { // getItems sets m_allowExit
	m_windowManager->setSignalled();
	return;
    }
    
    if (CONFIG_DISABLE_NEW_WINDOW_COMMAND && selecting >= 0) {
	++selecting; // ha ha ha!
	++m_nHidden;
    }
	
    if (selecting == 0) {
	m_windowManager->spawn((char *)CONFIG_NEW_WINDOW_COMMAND, NULL);
	return;
    }

    if (selecting > 0) {
	
	Client *cl = m_clients.item(selecting - 1);
	    
	if (selecting < m_nHidden) cl->unhide(True);
	else if (selecting < m_nItems) {
            cl->gotoClient();
	}

	if (CONFIG_WANT_KEYBOARD_MENU && e->type == KeyPress)
	    cl->activateAndWarp();
    }

    --m_nHidden; // just in case
}

ClientMenu::~ClientMenu()
{
    m_clients.remove_all();
    free(m_items);
}

char **ClientMenu::getItems(int *niR, int *nhR)
{
    int i;
    XButtonEvent *xbev = (XButtonEvent *)m_event; // KeyEvent is similar enough

    for (i = 0; i < m_windowManager->hiddenClients().count(); ++i) {
        Client *client = m_windowManager->hiddenClients().item(i);
	if (client->root() == m_windowManager->root() &&
	    client->channel() == m_windowManager->channel() &&
            client->type() != DockClient) {
            fprintf(stderr, "wmx: Menu: hidden client %d is \"%s\"\n",
                    i, client->name());
	    m_clients.append(client);
	}
    }

    int nh = m_clients.count();
    if (!CONFIG_DISABLE_NEW_WINDOW_COMMAND) ++nh;

    if (CONFIG_EVERYTHING_ON_ROOT_MENU) {
	for (i = 0; i < m_windowManager->clients().count(); ++i) {
            Client *client = m_windowManager->clients().item(i);
	    if (client->isNormal() &&
		client->root() == m_windowManager->root() &&
		client->channel() == m_windowManager->channel() &&
                client->type() != DockClient) {
		fprintf(stderr, "wmx: Menu: client %d is \"%s\"\n",
			i, client->name());
		m_clients.append(client);
	    }
	}
    }

    int n = m_clients.count();
    if (!CONFIG_DISABLE_NEW_WINDOW_COMMAND) ++n;
	
    int mx = DisplayWidth (display(), screen()) - 1;
    int my = DisplayHeight(display(), screen()) - 1;

    m_allowExit = 1;
    /* m_allowExit =
	((CONFIG_EXIT_CLICK_SIZE_X != 0 ? 
	  (CONFIG_EXIT_CLICK_SIZE_X > 0 ? 
	   (xbev->x < CONFIG_EXIT_CLICK_SIZE_X) : 
	   (xbev->x > mx + CONFIG_EXIT_CLICK_SIZE_X)) : 1) &&
	 (CONFIG_EXIT_CLICK_SIZE_Y != 0 ? 
	  (CONFIG_EXIT_CLICK_SIZE_Y > 0 ? 
	   (xbev->y < CONFIG_EXIT_CLICK_SIZE_Y) : 
	   (xbev->y > my + CONFIG_EXIT_CLICK_SIZE_Y)) : 1)); */

    m_allowExit = m_allowExit ||
	(isKeyboardMenuEvent(m_event) && CONFIG_EXIT_ON_KBD_MENU);

    if (m_allowExit) ++n;

    const char **items = (const char **)malloc(n * sizeof(char *));

    for (i = 0; i < n; ++i) {

	if (CONFIG_DISABLE_NEW_WINDOW_COMMAND == False) {

	    if (i == 0) items[i] = CONFIG_NEW_WINDOW_LABEL;
	    else if (m_allowExit && i > m_clients.count()) items[i] = "[Exit wmx]";
	    else items[i] = m_clients.item(i-1)->label();

	} else {

	    if (m_allowExit && i >= m_clients.count()) items[i] = "[Exit wmx]";
	    else items[i] = m_clients.item(i)->label();
	}
    }
 
    *niR = n;
    *nhR = nh;

    return (char **)items;
}


Client *ClientMenu::checkFeedback(int item)
{
    if (CONFIG_MAD_FEEDBACK == False) return NULL;
    
    if (CONFIG_DISABLE_NEW_WINDOW_COMMAND == False) {

	if (item <= 0 || item > m_clients.count()+1) return NULL;
	if (m_allowExit && item == m_clients.count() + 1) return NULL;
	return m_clients.item(item - 1);

    } else {

	if (item < 0 || item > m_clients.count()) return NULL;
	if (m_allowExit && item == m_clients.count()) return NULL;
	return m_clients.item(item);
    }
}

void ClientMenu::showFeedback(int item)
{
    if (Client *c = checkFeedback(item)) c->showFeedback();
}

void ClientMenu::removeFeedback(int item, Boolean mapped)
{
    if (Client *c = checkFeedback(item)) c->removeFeedback(mapped);
}
 
void ClientMenu::raiseFeedbackLevel(int item)
{
    if (Client *c = checkFeedback(item)) c->raiseFeedbackLevel();
}


CommandMenu::CommandMenu(WindowManager *manager, XEvent *e, char* otherdir)
    : Menu(manager, e)
{
    const char *home = getenv("HOME");
    const char *wmxdir = getenv("WMXDIR");

    m_commandDir = NULL;
    m_hasSubmenus = True;
    
    if (otherdir == NULL)
    {
#if CONFIG_ADD_SCREEN_TO_COMMAND_MENU
	isRootDir = True;
#endif
	if (wmxdir == NULL)
	{
	    if (home == NULL) return;
	    m_commandDir =
		(char *)malloc(strlen(home) + strlen(CONFIG_COMMAND_MENU) + 2);
	    sprintf(m_commandDir, "%s/%s", home, CONFIG_COMMAND_MENU);
	}
	else // wmxdir != NULL
	{
	    if(wmxdir[0] == '/')
	    {
		m_commandDir =
		    (char *)malloc(strlen(wmxdir) + 1);
		strcpy(m_commandDir, wmxdir);
	    }
	    else // wmxdir doesn't start with /
	    {
		m_commandDir =
		    (char *)malloc(strlen(home) + strlen(wmxdir) + 2);
		sprintf(m_commandDir, "%s/%s", home, wmxdir);
	    }
	}
    }
    else // otherdir != NULL
    {
#if CONFIG_ADD_SCREEN_TO_COMMAND_MENU
	isRootDir = False;
#endif
	m_commandDir = (char *)malloc(strlen(otherdir) + 1);
	strcpy(m_commandDir, otherdir);
    }

    int i = getSelection();
	
    if (i >= 0 && i < m_nHidden) {
	// This should never happen.
    }

    if (i >= m_nHidden && i < m_nItems)
    {
	char *commandFile =
	    (char *)malloc(strlen(m_commandDir) + STRLEN_MITEMS(i) + 2);

	sprintf(commandFile, "%s/%s", m_commandDir, m_items[i]);
	m_windowManager->spawn(m_items[i], commandFile);
	free(commandFile);
    }
}

CommandMenu::~CommandMenu()
{
    free(m_commandDir);
    for (int i = 0; i < m_nItems; i++) {
	free(m_items[i]);
    }
    free(m_items);
}

static int sortstrs(const void *va, const void *vb)
{
    char **a = (char **)va;
    char **b = (char **)vb;    
    return strcmp(*a, *b);
}

void CommandMenu::createSubmenu (XEvent *e, int i)
{
    char *new_directory;
    int dirlen = strlen (m_commandDir);
    
    new_directory = (char *)malloc (dirlen + STRLEN_MITEMS(i) + 2);
    strcpy (new_directory, m_commandDir);
    new_directory[dirlen] = '/';
    strcpy (new_directory + dirlen + 1, m_items[i]);
    
    CommandMenu menu (m_windowManager, e, new_directory);
    free(new_directory);
}

#if CONFIG_ADD_SCREEN_TO_COMMAND_MENU
DIR *CommandMenu::screenopendir(char *m_commandDir,int *dirlen,int olddirlen)
{
    char *p;
    DIR *dir;
    
    if(isRootDir)
    {
	for(p=m_commandDir;*p;p++);  // search end of m_commandDir
	sprintf(p,".%d",screen());   // append screen number
	dir = opendir(m_commandDir);
	if (dir == NULL) {
	    m_commandDir[olddirlen] = '\0'; // strip of screen number
	    *dirlen = olddirlen;
	    dir = opendir(m_commandDir);
	}
    }
    else
    {
	*dirlen = olddirlen;
	dir = opendir(m_commandDir);
    }
    return(dir);
}
#endif

char **CommandMenu::getItems(int *niR, int *nhR)
{
    *niR = *nhR = 0;
    DIR *dir;
    const char *home;
    char *dirpath;

    if ((home = getenv("HOME")) == NULL) return NULL;
	
#if CONFIG_ADD_SCREEN_TO_COMMAND_MENU
    int olddirlen = strlen(m_commandDir);
    int dirlen = olddirlen + 1 + m_windowManager->numdigits(screen());
    m_commandDir=(char *)realloc((void *)m_commandDir,(size_t)dirlen);
    dir = screenopendir(m_commandDir,&dirlen,olddirlen);
#else
    int dirlen = strlen(m_commandDir);
    dir = opendir(m_commandDir);
#endif

    if (dir == NULL) {

        free(m_commandDir);
        m_commandDir =
	    (char *)malloc(strlen(CONFIG_SYSTEM_COMMAND_MENU) + 1);
        sprintf(m_commandDir, CONFIG_SYSTEM_COMMAND_MENU);

#if CONFIG_ADD_SCREEN_TO_COMMAND_MENU
        olddirlen = strlen(m_commandDir);
	dirlen = olddirlen + 1 + m_windowManager->numdigits(screen());
	dir = screenopendir(m_commandDir,&dirlen,olddirlen);
#else
        dirlen = strlen(m_commandDir);
        dir = opendir(m_commandDir);
#endif

        if (dir == NULL) {
	    return NULL;
	}
    }

    dirpath = (char *)malloc(dirlen + 1024 + 2); // NAME_MAX guess
    strcpy(dirpath, m_commandDir);

    int count = 0;
    struct dirent *ent;

    // We already assume efficient realloc() in Border.C, so we may as
    // well assume it here too.

    char **items = 0;

    while ((ent = readdir(dir)) != NULL) {

	struct stat st;
	sprintf(dirpath + dirlen, "/%s", ent->d_name);
		
	if (stat(dirpath, &st) == -1) continue;
	if (!S_ISDIR(st.st_mode) || !(st.st_mode & 0444)) continue;
	if (ent->d_name[0] == '.') continue;

	items = (!items ? (char **)malloc(sizeof(char *)) :
		 (char **)realloc(items, (count + 1) * sizeof(char *)));

	items[count++] = NewString(ent->d_name);
    }

    qsort(items, count, sizeof(char *), sortstrs);
    *nhR = count;

    rewinddir(dir);

    while ((ent = readdir(dir)) != NULL) {

	struct stat st;
	sprintf(dirpath + dirlen, "/%s", ent->d_name);
		
	if (stat(dirpath, &st) == -1) continue;
	if (!S_ISREG(st.st_mode) || !(st.st_mode & 0111)) continue;

	items = (!items ? (char **)malloc(sizeof(char *)) :
		 (char **)realloc(items, (count + 1) * sizeof(char *)));

	items[count++] = NewString(ent->d_name);
    }

    *niR = count;
    qsort(items + *nhR, *niR - *nhR, sizeof(char *), sortstrs);
    
    free(dirpath);
    closedir(dir);

    return items;
}

#if CONFIG_USE_CHANNEL_MENU

ChannelMenu::ChannelMenu(WindowManager *manager, XEvent *e)
    : Menu(manager, e), m_channels(manager->channels()),
      m_channel(manager->channel())
{
    XButtonEvent *ev = (XButtonEvent *)e;

    Client *c = m_windowManager->windowToClient(ev->window);
    ev->x = ev->x_root; ev->y = ev->y_root;
	
    int channel = getSelection() + 1;

    m_windowManager->gotoChannel(channel, c);
}

ChannelMenu::~ChannelMenu()
{
    for (int i = 0; i < m_channels; i++) {
	free(m_items[i]);
    }
    free(m_items);
}

char **ChannelMenu::getItems(int *niR, int *nhR)
{
    *niR = m_windowManager->channels();
    *nhR = 0;

    char **items = (char **)malloc(*niR * sizeof(char *));
    for (int i = 0; i < *niR; i++) {
	char temp[50]; // Warning! Works only, if int < 136 bits.
	sprintf(temp, "%sChannel %d", i+1 == m_channel ? ">" : "", i + 1);
	char *item = (char *)malloc(strlen(temp) + 1);
	strcpy (item, temp);
	items[i] = item;
    }
    return m_items = items;
}

#endif

// Fake geometry as a "menu" to save on code

ShowGeometry::ShowGeometry(WindowManager *manager, XEvent *e)
    : Menu(manager, e)
{
    // empty
}

void ShowGeometry::update(int x, int y)
{
    char string[20];

    sprintf(string, "%d %d", x, y);
    int width = getTextWidth(string, strlen(string)) + 8;
#ifdef CONFIG_USE_XFT
    int height = m_font->ascent + m_font->descent + 8;
#else
    int height = m_font[screen()]->ascent + m_font[screen()]->descent + 8;
#endif
    int mx = DisplayWidth (display(), screen()) - 1;
    int my = DisplayHeight(display(), screen()) - 1;
  
    XMoveResizeWindow(display(), m_window[screen()],

#if CONFIG_GEOMETRY_X_POS < 0
		      0
#elif CONFIG_GEOMETRY_X_POS > 0
		      (mx - width)
#else
		      (mx - width) / 2
#endif
		      ,
#if CONFIG_GEOMETRY_X_POS < 0
		      0
#elif CONFIG_GEOMETRY_X_POS > 0
		      (my - height)
#else
                      (my - height) / 2
#endif
		      , width, height);

    XClearWindow(display(), m_window[screen()]);
    XMapRaised(display(), m_window[screen()]);

#ifdef CONFIG_USE_XFT
    XftDrawStringUtf8(m_xftDraw[screen()],
		      &m_xftColour[screen()],
		      m_font,
		      4,
		      4 + m_font->ascent,
		      (FcChar8 *)string,
		      strlen(string));
#else
    XDrawString(display(), m_window[screen()],
		Border::drawGC(m_windowManager,screen()),
		4, 4 + m_font[screen()]->ascent, string, strlen(string));
#endif
}

void ShowGeometry::remove()
{
    XUnmapWindow(display(), m_window[screen()]);
}

ShowGeometry::~ShowGeometry()
{
    // empty
}

char **ShowGeometry::getItems(int *niR, int *nhR)
{
    niR = nhR = 0;
    return NULL;
}


