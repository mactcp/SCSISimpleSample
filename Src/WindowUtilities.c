/*								WindowUtilities.c								*/
/*
 * WindowUtilities.c
 * Copyright � 1993 Apple Computer Inc. All rights reserved.
 * These functions are called when the window is zoomed or resized. They do
 * not require any knowledge of the application itself.
 */
#ifndef THINK_C				/* MPW includes			*/
#include <Errors.h>
#include <Script.h>
#include <Types.h>
#include <Resources.h>
#include <QuickDraw.h>
#include <Fonts.h>
#include <Events.h>
#include <Windows.h>
#include <ToolUtils.h>
#include <Memory.h>
#include <Menus.h>
#include <Lists.h>
#include <Printing.h>
#include <Dialogs.h>
#endif
#pragma segment MainCode

#ifndef TRUE
#define TRUE	1
#define FALSE	0
#endif

void								DoZoomWindow(
		WindowPtr						theWindow,
		short							whichPart
	);
Boolean								DoGrowWindow(
		WindowPtr						theWindow,
		Point							eventWhere,
		short							minimumWidth,
		short							minimumHeight
	);
void								MyDrawGrowIcon(
		WindowPtr						theWindow
	);

#include <Gestalt.h>

#ifndef topLeft
#define topLeft(r)	(((Point *) &(r))[0])
#define botRight(r)	(((Point *) &(r))[1])
#endif
#ifndef width
#define width(r)	((r).right - (r).left)
#define height(r)	((r).bottom - (r).top)
#endif
#define kScrollBarWidth			(16)
#define kScrollBarOffset		(kScrollBarWidth - 1)

/*
 * DoZoomWindow
 * Algorithm from New Inside Mac (Toolbox Essentials) 4-55
 */
void
DoZoomWindow(
		WindowPtr						theWindow,
		short							whichPart
	)
{
		GDHandle				gd;
		GDHandle				gdZoom;
		GrafPtr					savePort;
		Rect					windowRect;
		Rect					zoomRect;
		Rect					intersection;
		long					thisArea;
		long					greatestArea;
		short					windowTitleHeight;
		long					response;
#define PEEK	(*((WindowPeek) theWindow))
#define STATE	(**((WStateDataHandle) PEEK.dataHandle))

		GetPort(&savePort);
		SetPort(theWindow);
		if (whichPart == inZoomOut) {
			if (Gestalt(gestaltQuickdrawVersion, &response) != noErr
		 	 || response < gestalt8BitQD) {
				/*
				 * This shouldn't happen on a modern system, but,
				 * if it does, just force a single screen zoom.
				 */
				zoomRect = qd.screenBits.bounds;
				InsetRect(&zoomRect, 4, 4);
				STATE.stdState = zoomRect;
			}
			else {
				/*
				 * We have color QuickDraw. Locate the screen that contains
				 * the largest area of the window and zoom to that screen.
				 */
				windowRect = theWindow->portRect;
				LocalToGlobal(&topLeft(windowRect));
				LocalToGlobal(&botRight(windowRect));
				windowTitleHeight = windowRect.top
						- 1
						- (**PEEK.strucRgn).rgnBBox.top;
				windowRect.top -= windowTitleHeight;
				greatestArea = 0;
				gdZoom = NULL;
				/*
				 * Look at all graphics devices and find an intersection. Then
				 * select the largest intersecting screen.
				 */
				for (gd = GetDeviceList(); gd != NULL; gd = GetNextDevice(gd)) {
					if (TestDeviceAttribute(gd, screenDevice)
					 && TestDeviceAttribute(gd, screenActive)
					 && SectRect(&windowRect, &(**gd).gdRect, &intersection)) {
						thisArea = ((long) width(intersection))
								 * ((long) height(intersection));
						if (thisArea > greatestArea) {
							greatestArea = thisArea;
							gdZoom = gd;
						}
					}
				}
				/*
				 * If we're zooming to the device with the menu bar,
				 * allow for its height.
				 */
				if (GetMainDevice() == gdZoom)
					windowTitleHeight += GetMBarHeight();
				zoomRect = (**gdZoom).gdRect;
				InsetRect(&zoomRect, 3, 3);
				zoomRect.top += windowTitleHeight;
				STATE.stdState = zoomRect;
			}										/* End if color QuickDraw	*/
		}											/* End if zoom out			*/
		ZoomWindow(theWindow, whichPart, (theWindow == FrontWindow()));
		/*
		 * Zoom redraws the entire window.
		 */
		EraseRect(&theWindow->portRect);
		InvalRect(&theWindow->portRect);
		SetPort(savePort);			
#undef PEEK
#undef STATE
}

/*
 * DoGrowWindow
 * Algorithm from New Inside Mac (Toolbox Essentials) 4-58
 * We assume that the window can cover the entire screen.
 */
Boolean
DoGrowWindow(
		WindowPtr						theWindow,
		Point							eventWhere,
		short							minimumWidth,
		short							minimumHeight
	)
{
		long					growSize;
		Rect					limitRect;
		
		limitRect.left = minimumWidth;
		limitRect.top = minimumHeight;
		limitRect.right = qd.screenBits.bounds.right;
		limitRect.bottom = qd.screenBits.bounds.bottom;
		growSize = GrowWindow(theWindow, eventWhere, &limitRect);
		if (growSize != 0) {
			SizeWindow(theWindow, LoWord(growSize), HiWord(growSize), TRUE);
			/*
			 * Force a redraw of the entire window, but exclude the
			 * intersection of the old view rect and the new view rect.
			 * Invalidate any prior update region in any case.
			 */
			EraseRect(&theWindow->portRect);
			InvalRect(&theWindow->portRect);
		}
		return (growSize != 0);
}


/*
 * Draw the grow box, but don't draw the scroll bar lines.
 */
void
MyDrawGrowIcon(
		WindowPtr						theWindow
	)
{
		RgnHandle			clipRgn;
		Rect				clipRect;
		
		clipRgn = NewRgn();
		GetClip(clipRgn);
		clipRect = theWindow->portRect;
		clipRect.top = clipRect.bottom - kScrollBarOffset;
		clipRect.left = clipRect.right - kScrollBarOffset;
		ClipRect(&clipRect);
		DrawGrowIcon(theWindow);
		ValidRect(&clipRect);
		SetClip(clipRgn);
		DisposeRgn(clipRgn);
}

