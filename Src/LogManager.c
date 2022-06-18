/*									LogManager.c								*/
/*
 * LogManager.c
 * Copyright � 1993 Apple Computer Inc. All rights reserved.
 *
 * These functions manage a logging display for error messages and other text.
 * The log is implemented as a ListManager list that can hold nLogItems. This
 * module is intentionally more-or-less self-contained so it can easily be
 * exported to other applications.
 */
#include "LogManager.h"
#ifndef THINK_C				/* MPW includes			*/
#include <Errors.h>
#include <Script.h>
#include <Types.h>
#include <Files.h>
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
#include <StandardFile.h>
#endif
#include <Serial.h>
#include <Printing.h>
#include <Packages.h>
#pragma segment LogManager

/*
 * There is a 32000 byte maximum for the list, so don't
 * make nLogLines too big.
 */
#ifndef nDefaultLogLines
#define nDefaultLogLines	128
#endif
#define width(r)				((r).right - (r).left)
#define height(r)				((r).bottom - (r).top)
#ifndef TRUE
#define TRUE					1
#define FALSE					0
#endif
enum {
	kScrollBarWidth 	= 16,
	kScrollBarOffset	= kScrollBarWidth - 1,
	kActiveControl		= 0,		/* Normal button hilite				*/
	kDisabledControl	= 255		/* Disabled button hilite			*/
};
/*
 * When the horizontal scrollbar is at zero, the list is indented +4 pixels.
 * When the horizontal scrollbar increases, the list indent decreases. This
 * value is "by inspection," but we could extract it from the list record
 * when the list is initially created.
 */
enum {
	kZeroIndent			= 4
};
/*
 * This sets a nominal value for the maximum cell width. This might be
 * better provided as a user-settable parameter.
 */
#define kMaxHorizontalScroll	(CharWidth('M') * 255)
static const char		endOfLine[1] = { 0x0D };	/* <CR>			*/

/*
 * Cheap 'n dirty pascal string copy routine.
 */
#ifndef pstrcpy
#define pstrcpy(dst, src) do {							\
		StringPtr	_src = (src);						\
		BlockMove(_src, dst, _src[0] + 1);				\
	} while (0)
#endif
/*
 * Cheap 'n dirty pascal string concat.
 */
#ifndef pstrcat
#define pstrcat(dst, src) do {							\
		StringPtr		_dst = (dst);					\
		StringPtr		_src = (src);					\
		short			_len;							\
		_len = 255 - _dst[0];							\
		if (_len > _src[0]) _len = _src[0];				\
		BlockMove(&_src[1], &_dst[1] + _dst[0], _len);	\
		_dst[0] += _len;								\
	} while (0)
#endif

static OSErr						AddStringToList(
		ListHandle						logListHandle,
		ConstStr255Param				theString
	);
static pascal void					ScrollLogAction(
		register ControlHandle			theControl,
		short							partcode
	);
static void							ScrollLogList(
		ControlHandle					theControl
	);
static StringHandle					GetLogStringHandle(
		ListHandle						logListHandle,
		short							theRow
	);

#define LIST			(**logListHandle)
#define LOGINFO			(**((LogInfoHdl) (LIST.userHandle)))
#define HSCROLL			(LOGINFO.hScroll)
#define IS_COLOR(port)	(((((CGrafPtr) (port))->portVersion) & 0xC000) != 0)
#define COLOR_LIST		(IS_COLOR(LIST.port))

/*
 * This code sequence is copied into the ListProc handle. It is designed so we
 * don't have to flush the instruction and data caches. Note that this must
 * be revised if you compile for a non-68000 environment.
 */
static const short gDummyLDEF[] = {
		0x207A,				/*		movea.l	procPtr,a0			*/
		0x0004,				/*				<offset to procPtr>	*/
		0x4ED0,				/*		jmp		a0 					*/
		0x0000, 0x0000		/*		dc.l	<Drawing Proc here>	*/
};

static pascal void
LogListDefProc(
		short					listMessage,
		Boolean					listSelect,
		Rect					*listRect,
		Cell					listCell,
		short					listDataOffset,
		short					listDataLen,
		ListHandle				errorLogList
	);

/*
 * Create the data display list.
 */
ListHandle
CreateLog(
		const Rect					*viewRect,
		short						listFontNumber,
		short						listFontSize,
		short						logLines
	)
{
		OSErr						status;
		ListHandle					logListHandle;
		FontInfo					info;
		Point						cellSize;
		short						listHeight;
		Rect						dataBounds;
		Rect						listRect;
		short						listFontHeight;
		Handle						drawProcHdl;
		LogInfoRecord				logInfo;
		Handle						logInfoHdl;
		ProcPtr						listProcPtr;
		short						horizontalMax;
		
		logInfoHdl = NULL;
		drawProcHdl = NULL;
		if (logLines == 0)
			logLines = nDefaultLogLines;
		TextFont(listFontNumber);
		TextSize(listFontSize);
		GetFontInfo(&info);
		listFontHeight = info.ascent + info.descent + info.leading;
		/*
		 * Compute the list drawing area, adjusting the list
		 * area height so an integral number of lines will be drawn. As with
		 * the standard list manager, the scroll bars will be drawn outside
		 * of the list rectangle.
		 */
		listRect = *viewRect;
		/*
		 * Normalize the list area so that an integral number of cells are visible
		 * on the display and define the visual shape of each cell.
		 */
		listHeight = height(listRect);
		listHeight -= (listHeight % listFontHeight);
		listRect.bottom = listRect.top + listHeight;
		SetPt(&cellSize, width(listRect), listFontHeight);
		/*
		 * Note: we create a one-column list with both vertical and horizontal
		 * scrollbars, then we steal the horizontal scrollbar because we're
		 * scrolling within the list cell. Unfortunately, the List Manager only
		 * scrolls from one cell (column in the horizontal direction) to another.
		 */
		SetRect(&dataBounds, 0, 0, 1, 0);
		logListHandle = LNew(
				&listRect,							/* Viewing area				*/
				&dataBounds,						/* Rows and col's			*/
				cellSize,							/* Element size				*/
				0,									/* No defProc yet			*/
				qd.thePort,							/* Display window			*/
				TRUE,								/* Draw it					*/
				FALSE,								/* No grow box				*/
				TRUE,								/* Has horizontal scroll	*/
				TRUE								/* Has vertical scroll		*/
			);
		if (logListHandle == NULL)
			goto failure;
		LIST.selFlags = lOnlyOne;
		LIST.listFlags = lDoVAutoscroll;			/* Vertical autoscroll only	*/
		logInfo.logFileRefNum = 0;
		logInfo.logFileVRefNum = 0;
		logInfo.logFileStatus = 0;
		logInfo.logLines = logLines;
		logInfo.fontNumber = listFontNumber;
		logInfo.fontSize = listFontSize;
		if (COLOR_LIST) {
			GetForeColor(&logInfo.foreColor);
			GetBackColor(&logInfo.backColor);
		}
		status = PtrToHand(&logInfo, &logInfoHdl, sizeof logInfo);
		if (status != noErr)
			goto failure;
		LIST.userHandle = (Handle) logInfoHdl;
		HSCROLL = LIST.hScroll;						/* Grab horizontal scroller	*/
		LIST.hScroll = NULL;						/* Remove it from the list	*/
		SetCRefCon(HSCROLL, (long) logListHandle);	/* Link scrollbar to list	*/
		status = PtrToHand(gDummyLDEF, &drawProcHdl, sizeof gDummyLDEF);
		if (status != noErr)
			goto failure;
		listProcPtr = (ProcPtr) LogListDefProc;
		BlockMove(&listProcPtr, &((short *) *drawProcHdl)[3], sizeof listProcPtr);
		LIST.listDefProc = drawProcHdl;
		horizontalMax = kMaxHorizontalScroll - width((**HSCROLL).contrlRect);
		if (horizontalMax < 0)
			horizontalMax = 0;
		SetCtlMin(HSCROLL, 0);
		SetCtlMax(HSCROLL, horizontalMax);
		SetCtlValue(HSCROLL, 0);
		HiliteControl(
			HSCROLL,
			(horizontalMax == 0) ? kDisabledControl : kActiveControl
		);
		goto success;
failure:
		if (drawProcHdl != NULL) {
			DisposeHandle((Handle) drawProcHdl);
			LIST.listDefProc = NULL;
		}
		DisposeLog(logListHandle);
		logListHandle = NULL;
success:
		return (logListHandle);
}

/*
 * DisposeLog disposes of our private information and then disposes of the list.
 */
void
DisposeLog(
		ListHandle						logListHandle
	)
{
		if (logListHandle != NULL) {
			if (LIST.userHandle != NULL) {
				LIST.hScroll = HSCROLL;			/* The list manager disposes	*/ 
				DisposeHandle(LIST.userHandle);
				LIST.userHandle = NULL;
			}
			LDispose(logListHandle);
		}
}

/*
 * UpdateLogWindow explicitly updates the log's window. It is generally called only
 * after Modal Dialogs or Alerts obscured the window.
 */
void
UpdateLogWindow(
		ListHandle						logListHandle
	)
{
		WindowPtr						theWindow;
		GrafPtr							savePort;
		Rect							viewRect;
		RgnHandle						listRgn;
		RgnHandle						clipRgn;
		
		if (logListHandle != NULL) {
			theWindow = (WindowPtr) LIST.port;
			if (EmptyRgn(((WindowPeek) theWindow)->updateRgn) == FALSE) {
				viewRect = LIST.rView;
				if (LIST.hScroll != NULL)
					viewRect.bottom += kScrollBarWidth;
				if (LIST.hScroll != NULL)
					viewRect.right += kScrollBarWidth;
				listRgn = NewRgn();
				RectRgn(listRgn, &viewRect);
				SectRgn(listRgn, ((WindowPeek) theWindow)->updateRgn, listRgn);
				if (EmptyRgn(listRgn) == FALSE) {
					/*
					 * We have something to redraw. Fake an update event handler.
					 */
					GetPort(&savePort);
					SetPort(theWindow);
					clipRgn = NewRgn();
					GetClip(clipRgn);
					SetClip(listRgn);
					EraseRgn(listRgn);
					UpdateLog(logListHandle);
					SetClip(clipRgn);
					DisposeRgn(clipRgn);
					ValidRgn(listRgn);
					SetPort(savePort);
				}
				DisposeRgn(listRgn);
			}
		}
}
 
/*
 * UpdateLog redraws the list.
 */
void
UpdateLog(
		ListHandle						logListHandle
	)
{
		Rect						viewRect;
		RGBColor					saveForeColor;
		RGBColor					saveBackColor;
		
		if (logListHandle != NULL) {
			/*
			 * Make sure the list is locked down while we draw. Note that we
			 * assume that the application has called UpdateControls, so
			 * the horizontal scrollbar is correctly drawn.
			 */		
			if (COLOR_LIST) {
				GetForeColor(&saveForeColor);
				GetBackColor(&saveBackColor);
				RGBForeColor(&LOGINFO.foreColor);
				RGBBackColor(&LOGINFO.backColor);
			}
			viewRect = LIST.rView;
			/*
			 * Include the scrollbars in the frame.
			 */
			EraseRect(&viewRect);
			InsetRect(&viewRect, -1, -1);
			viewRect.right += kScrollBarOffset;
			viewRect.bottom += kScrollBarOffset;
			FrameRect(&viewRect);
			LUpdate(LIST.port->visRgn, logListHandle);
			if (COLOR_LIST) {
				RGBForeColor(&saveForeColor);
				RGBBackColor(&saveBackColor);
			}
		}
}

/*
 * ActivateLog activates (or deactivates) the log. Call it on activate and
 * suspendResume events.
 */
void
ActivateLog(
		ListHandle						logListHandle,
		Boolean							activating
	)
{
		if (logListHandle != NULL) {
			LActivate(activating, logListHandle);
			HiliteControl(
				HSCROLL,
				(activating) ? kActiveControl : kDisabledControl);
		}
}

/*
 * MoveLog Repositions the log list area.
 */
void
MoveLog(
		ListHandle						logListHandle,
		short							leftEdge,
		short							topEdge
	)
{
		Rect							viewRect;
		
		if (logListHandle != NULL) {
			if (LIST.rView.left != leftEdge || LIST.rView.top != topEdge) {
				viewRect = LIST.rView;
				InsetRect(&viewRect, -1, -1);
				InvalRect(&viewRect);
				OffsetRect(
					&LIST.rView,
					leftEdge - LIST.rView.left,
					topEdge -LIST.rView.top
				);
				viewRect = LIST.rView;
				InsetRect(&viewRect, -1, -1);
				InvalRect(&viewRect);
				MoveControl(
					LIST.vScroll,
					LIST.rView.right - kScrollBarOffset,
					LIST.rView.top - 1
				);
				MoveControl(
					HSCROLL,
					LIST.rView.left - 1,
					LIST.rView.bottom - kScrollBarOffset
				);
			}
		}
}

/*
 * SizeLog: this is the list rectangle size and does not include the
 * horizontal and vertical scrollbars.
 */
void
SizeLog(
		ListHandle						logListHandle,
		short							newWidth,
		short							newHeight
	)
{
		Rect							viewRect;
		Point							cellSize;
		short							horizontalMax;
		
		if (logListHandle != NULL) {
			viewRect = LIST.rView;
			InsetRect(&viewRect, -1, -1);
			InvalRect(&viewRect);
			/*
			 * We put the horizontal scrollbar back into the list record so that
			 * the list manager kindly resizes it. Then we take it out again.
			 */
			LIST.hScroll = HSCROLL;
			LSize(newWidth, newHeight, logListHandle);
			LIST.hScroll = NULL;
			cellSize = LIST.cellSize;
			cellSize.h = width(LIST.rView);
			LCellSize(cellSize, logListHandle);
			horizontalMax = kMaxHorizontalScroll - width((**HSCROLL).contrlRect);
			if (horizontalMax < 0)
				horizontalMax = 0;
			SetCtlMax(HSCROLL, horizontalMax);
			HiliteControl(
				HSCROLL,
				(horizontalMax == 0) ? kDisabledControl : kActiveControl
			);
			viewRect = LIST.rView;
			InsetRect(&viewRect, -1, -1);
			InvalRect(&viewRect);
		}
}

/*
 * DoClickInLog: call this when there is a mouse down in the log area (or in
 * one of the scrollbars.
 */
Boolean
DoClickInLog(
		ListHandle						logListHandle,
		const EventRecord				*eventRecord
	)
{
#define EVENT	(*eventRecord)

		Point							mousePt;
		Boolean							result;
		Rect							viewRect;
		short							part;
		ControlHandle					theControl;

		if (logListHandle == NULL)
			result = FALSE;
		else {
			mousePt = EVENT.where;
			GlobalToLocal(&mousePt);
			/*
			 * Handle clicks in the horizontal scrollbar: Do not pass them through
			 * LClick, as it does not do what we want and besides, the scrollbar
			 * isn't there any more.
			 */
			if (PtInRect(mousePt, &(**HSCROLL).contrlRect)) {
				part = FindControl(mousePt, (**HSCROLL).contrlOwner, &theControl);
				if (part >= 0 && theControl == HSCROLL) {
					if (part == inThumb) {
						if (TrackControl(theControl, mousePt, NULL))
							ScrollLogList(theControl);
					}
					else {
						TrackControl(theControl, mousePt,
									NewControlActionProc(ScrollLogAction));
					}
				}
			}
			else {
				viewRect = LIST.rView;
				viewRect.right += kScrollBarOffset;
				result = PtInRect(mousePt, &viewRect);
				if (result)
					(void) LClick(mousePt, EVENT.modifiers, logListHandle);
			}
		}
		return (result);
#undef EVENT
}

/*
 * Log errors.
 */
void
LogStatus(
		ListHandle						logListHandle,
		OSErr							theError,
		const StringPtr					infoText
	)
{
		Handle							macErrorHdl;
		Str255							msg;
		Str15							errorValue;
		
		if (logListHandle != NULL && theError != noErr) {
			pstrcpy(msg, infoText);
			pstrcat(msg, "\p: ");
			NumToString(theError, errorValue);
			pstrcat(msg, errorValue);
			macErrorHdl = GetResource('Estr', theError);
			if (macErrorHdl != NULL) {
				pstrcat(msg, "\p ");
				pstrcat(msg, (StringPtr) *macErrorHdl);
				ReleaseResource(macErrorHdl);
			}
			DisplayLogString(logListHandle, msg);
		}
}

/*
 * DisplayLogString
 * Call this function to store a string in the list.
 */
void
DisplayLogString(
		ListHandle						logListHandle,
		const StringPtr					theString
	)
{
		short						theRow;
		Boolean						scrollAtBottom;
		
		if (logListHandle != NULL) {
			/*
			 * If there are already logLines in the
			 * list, delete the first row of the list.
			 * Then, in any case, append this datum at the
			 * bottom.
			 *
			 * The scroll bars are managed as follows:
			 * scroll bar is at the bottom, the new datum
			 * is selected and autoscrolled into view.
			 * Otherwise, the current cell is unchanged.
			 */
			theRow = LIST.dataBounds.bottom;
			scrollAtBottom =
				(GetCtlValue(LIST.vScroll) == GetCtlMax(LIST.vScroll));
			if (theRow >= LOGINFO.logLines)
				LDelRow(1, 0, logListHandle);
			if (AddStringToList(logListHandle, theString) == noErr) {
				if (scrollAtBottom)
					LScroll(0, LIST.dataBounds.bottom - GetCtlValue(LIST.vScroll), logListHandle);
				if (LOGINFO.logFileRefNum != 0 && LOGINFO.logFileStatus == noErr)
					WriteLogLine(logListHandle, theString);
			}
		}
failure:
		return;
}

static OSErr
AddStringToList(
		ListHandle						logListHandle,
		ConstStr255Param				theString
	)
{
		StringHandle					stringHandle;
		Cell							theCell;
		
		stringHandle = NewString(theString);
		if (stringHandle != NULL) {
			theCell.h = 0;
			theCell.v = LAddRow(1, LIST.dataBounds.bottom, logListHandle);
			if (MemError() == noErr)
				LSetCell(&stringHandle, sizeof stringHandle, theCell, logListHandle);
		}
		return (MemError());
}

/*
 * ScrollLogAction is called by the Toolbox while executing the TrackControl
 * routine.  It has to take care of scrolling the log when the user clicks on the
 * up/down arrow or page parts of the scroll bar.
 */
static pascal void
ScrollLogAction(
		register ControlHandle			theControl,
		short							partcode
	)
{
		short							delta;
		
		delta = (width((**theControl).contrlRect) * 7) / 8;
		switch (partcode) {
		case inUpButton:	delta = -CharWidth('M');	break;
		case inPageUp:		delta = -(delta);			break;		
		case inDownButton:	delta = CharWidth('M');		break;
		case inPageDown:	/* All set */				break;
		default:			return;		/* Mouse exited control	*/
		}
		SetCtlValue(theControl, GetCtlValue(theControl) + delta);
		ScrollLogList(theControl);
}

/*
 * ScrollLogList scrolls the list rectangle in the proper direction and updates
 * the list's horizontal indentation to match.
 */
static void
ScrollLogList(
		ControlHandle					theControl
	)
{
		ListHandle						logListHandle;
		short							delta;
		RgnHandle						clipRgn;
		RgnHandle						updateRgn;
		Rect							viewRect;
		
		logListHandle = (ListHandle) GetCRefCon(theControl);
		/*
		 * LIST.indent.h is negative when the cell is scrolled left. Get the
		 * amount it's currently scrolled (as a positive value) and set delta
		 * to the amount that must be scrolled. Delta will be positive to
		 * scroll right (which means that the scrollbar has moved left).
		 */
		delta = kZeroIndent - LIST.indent.h - GetCtlValue(theControl);
		if (delta != 0) {
			/*
			 * We need to scroll the list cells. Get a clip rectangle so the
			 * scrolling is limited to the drawing area, scroll it, and update
			 * the stuff that came into view.
			 */
			viewRect = LIST.rView;
			clipRgn = NewRgn();
			updateRgn = NewRgn();
			GetClip(clipRgn);
			ClipRect(&viewRect);
			ScrollRect(&viewRect, delta, 0, updateRgn);
			LIST.indent.h += delta;
			LUpdate(updateRgn, logListHandle);
			SetClip(clipRgn);
			DisposeRgn(updateRgn);
			DisposeRgn(clipRgn);
		}
}
		
/*
 * Draw the string stored in the list. The only difference between this function
 * and a "normal" LDEF is that we don't visually indicate selection.
 */
static pascal void
LogListDefProc(
		short							listMessage,
		Boolean							listSelect,
		Rect							*listRect,
		Cell							listCell,
		short							listDataOffset,
		short							listDataLen,
		ListHandle						logListHandle
	)
{
		char							stringLockState;
		RGBColor						saveForeColor;
		RGBColor						saveBackColor;
		StringHandle					stringHandle;
		
		if (0) {			/* Touch unused variables	*/
			listCell;
		}
		/*
		 * If the userHandle isn't setup, do nothing: this is an initialization
		 * or a spurious command while we're disposing of the list.
		 */
		if (LIST.userHandle != NULL) {
			TextFont(LOGINFO.fontNumber);
			TextSize(LOGINFO.fontSize);
			if (COLOR_LIST) {
				GetForeColor(&saveForeColor);
				GetBackColor(&saveBackColor);
				RGBForeColor(&LOGINFO.foreColor);
				RGBBackColor(&LOGINFO.backColor);
			}
			switch (listMessage) {
			case lInitMsg:
				break;
			case lDrawMsg:
				EraseRect(listRect);
				if (listDataLen == sizeof stringHandle) {
					BlockMove(
						(*LIST.cells) + listDataOffset,
						&stringHandle,
						sizeof stringHandle
					);
					/*
					 * We don't indent in the vertical direction: by default,
					 * it contains the font ascent which is fine for DrawText
					 */
					stringLockState = HGetState((Handle) stringHandle);
					HLock((Handle) stringHandle);
					MoveTo(
						listRect->left + LIST.indent.h,
						listRect->top + LIST.indent.v
					);
					DrawString(*stringHandle);
					HSetState((Handle) stringHandle, stringLockState);
				}
				if (listSelect == FALSE)
					break;
				/* Continue to do hilite */
			case lHiliteMsg:
#if 0		/* Hiliting is disabled */
#ifdef THINK_C
				HiliteMode &= ~(1 << hiliteBit);
#else /* MPW */
				*((char *) HiliteMode) &= ~(1 << hiliteBit); /* Inside Mac V-61	*/
#endif
				InvertRect(listRect);
#endif
				break;
			}
			if (COLOR_LIST) {
				RGBForeColor(&saveForeColor);
				RGBBackColor(&saveBackColor);
			}
		}
}

StringHandle
GetLogStringHandle(
		ListHandle					logListHandle,
		short						theRow
	)
{
		Cell						theCell;
		StringHandle				result;
		short						dataLength;
		
		dataLength = sizeof result;
		theCell.h = 0;
		theCell.v = theRow;
		LGetCell(&result, &dataLength, theCell, logListHandle);
		if (dataLength != sizeof result)
			result = NULL;
		return (result);
}

/*
 * Prompt the user for a file name and write the current log contents
 * to the chosen file. Returns an error status (noErr is ok). This function
 * returns userCanceledErr if the user cancelled the SFPutFile dialog.
 */
OSErr
SaveLogFile(
		ListHandle						logListHandle,
 		ConstStr255Param				dialogPromptString,
		ConstStr255Param				defaultFileName,
		OSType							creatorType
	)
{
		Point							where;
		SFReply							reply;
		DialogTHndl						dialog;
		Rect							box;
		OSErr							status;
		
		/*
		 * Center the dialog
		 */
		dialog = (DialogTHndl) GetResource('DLOG', putDlgID);
		if (dialog == NULL)					 		/* No such dialog!	*/
		    SetRect(&box, 0, 0, 0, 0);				/* Center on screen	*/
		else {
		    box = (*dialog)->boundsRect; 			/* Dialog shape		*/
		    ReleaseResource((Handle) dialog);		/* Done with dialog	*/
		}
		SetPt(&where,
			(width(qd.thePort->portRect) - width(box)) / 2,
			((height(qd.thePort->portRect) - GetMBarHeight()) / 3)
				+ GetMBarHeight()
		);
		SFPutFile(where, dialogPromptString, defaultFileName, NULL, &reply);
		if (reply.good == FALSE)
			status = userCanceledErr;
		else {
			SetCursor(*GetCursor(watchCursor));
			status = CreateLogFile(
						logListHandle,
						creatorType,
						reply.fName,
						reply.vRefNum
					);
			InitCursor();
			if (status != noErr)
				(void) FSDelete(reply.fName, reply.vRefNum);
		}
		return (status);
}

/*
 * Create a log on the specified file and volume. Normally, called only
 * by SaveLogFile.
 */
OSErr
CreateLogFile(
		ListHandle						logListHandle,
		OSType							creatorType,
		ConstStr255Param				fileName,
		short							vRefNum
	)
{
		OSErr							status;
		short							refNum;
		
		/*
		 * Create the file, elmininating any duplicate.
		 */
		if (creatorType == 0)
			creatorType = 'ttxt';					/* TeachText			*/
		status = Create(fileName, vRefNum, creatorType, 'TEXT');
		if (status == dupFNErr) {					/* Exists already?		*/
			status = FSDelete(fileName, vRefNum);
			if (status == noErr)
				status = Create(fileName, vRefNum, creatorType, 'TEXT');
		}
		if (status == noErr)
			status = FSOpen(fileName, vRefNum, &refNum);
		if (status == noErr) {
			LOGINFO.logFileRefNum = refNum;
			LOGINFO.logFileVRefNum = vRefNum;
			LOGINFO.logFileStatus = noErr;
			WriteCurrentLog(logListHandle);
			status = LOGINFO.logFileStatus;
		}
		if (status != noErr) {
			(void) CloseLogFile(logListHandle);
			LogStatus(logListHandle, status, "\pCreateLogFile failed");
		}
		return (status);
}

/*
 * Write the current contents of the log file. Any error will be
 * in LOGINFO.logFileStatus.
 */
void
WriteCurrentLog(
		ListHandle						logListHandle
	)
{
		short							theRow;
		long							fileLength;
		StringHandle					stringHandle;
		char							stringLockState;

		
		theRow = LIST.dataBounds.top;
		while (LOGINFO.logFileStatus == noErr && theRow < LIST.dataBounds.bottom) {
			stringHandle = GetLogStringHandle(logListHandle, theRow);
			if (stringHandle != NULL) {
				stringLockState = HGetState((Handle) stringHandle);
				HLock((Handle) stringHandle);
				fileLength = (*stringHandle)[0];
				LOGINFO.logFileStatus = FSWrite(
					LOGINFO.logFileRefNum,
					&fileLength,
					&(*stringHandle)[1]
				);
				HSetState((Handle) stringHandle, stringLockState);
				if (LOGINFO.logFileStatus == noErr) {
					fileLength = 1;
					LOGINFO.logFileStatus = FSWrite(
								LOGINFO.logFileRefNum,
								&fileLength,
								endOfLine
							);
				}
			}
			++theRow;
		};
}		

void
WriteLogLine(
		ListHandle						logListHandle,
		ConstStr255Param				theText
	)
{
		long							fileLength;
		
		if (LOGINFO.logFileRefNum != 0 && LOGINFO.logFileStatus == noErr) {
			fileLength = theText[0];
			LOGINFO.logFileStatus = FSWrite(
						LOGINFO.logFileRefNum,
						&fileLength,
						&theText[1]
					);
			if (LOGINFO.logFileStatus == noErr) {
				fileLength = 1;
				LOGINFO.logFileStatus = FSWrite(
							LOGINFO.logFileRefNum,
							&fileLength,
							endOfLine
						);
			}
			if (LOGINFO.logFileStatus != noErr) {
				CloseLogFile(logListHandle);
				LogStatus(
					logListHandle,
					LOGINFO.logFileStatus,
					"\pCan't write to log file"
				);
			}
		}
}

OSErr
CloseLogFile(
		ListHandle						logListHandle
	)
{
		OSErr							status;
		
		status = noErr;
		if (LOGINFO.logFileRefNum != 0) {
			status = FSClose(LOGINFO.logFileRefNum);
			if (status == noErr)
				status = FlushVol(NULL, LOGINFO.logFileVRefNum);
		}
		LOGINFO.logFileRefNum = 0;
		LOGINFO.logFileVRefNum = 0;
		if (status != noErr)
			LogStatus(logListHandle, status, "\pCan't close log file");
		if (LOGINFO.logFileStatus == noErr)
			LOGINFO.logFileStatus = status;
		status = LOGINFO.logFileStatus;
		LOGINFO.logFileStatus = noErr;
		return (status);
}

OSErr
GetLogFileError(
		ListHandle						logListHandle
	)
{
		return (LOGINFO.logFileStatus);
}

Boolean
HasLogFile(
		ListHandle						logListHandle
	)
{
		return (LOGINFO.logFileRefNum != 0);
}

/*
 * Copyright � 1993, Apple Computer Inc. All Rights reserved.
 *
 * This started life as a generic print driver written by Rich Siegel and posted
 * to Usenet in around 1988 or so. It has been extensively rewritten, but not
 * necessarily improved.
 */

/*
 *		OSErr
 *		PrintDriver(
 *				THPrint			hPrint,
 *				void			*clientData,
 *				Boolean			doStyle,
 *				OSErr			(*userPrepProc)(
 *										THPrnt		hPrint,
 *										void		*clientData
 *									),
 *				OSErr			(*userPageProc)(
 *										THPrnt		hPrint,
 *										void		*clientData,
 *										const Rect	*pageRect,
 *										short		pageNum
 *									)
 *			);
 *		hPrint		A Print Manager handle.  If NULL, ReportPrintDriver
 *					will allocate a handle, call the Page Setup dialog,
 *					and dispose of the handle on exit.
 *		clientData	A longword parameter that becomes a parameter to the
 *					prepProc and pageProc functions. For the LogManager,
 *					it is the ListHandle.
 *		doStyle		If TRUE, the page setup modal dialog is always called.
 *					Otherwise, it's called only if the print record is invalid.
 *
 *		userPrepProc A user-provided function that will perform print setup
 *					initializations.  It must be defined
 *						OSErr			UserPrepProc(
 *								THPrint		hPrint,
 *								void		*clientData
 *							);
 *					UserPrepProc returns noErr on success or an error status that
 *					will  be returned to the PrintDriver caller on failure.
 *					UserPrepProc must set (**hPrint).prJob.iLstPage  to the correct
 *					number of pages in the document. If all pages are specified
 *					by the user dialog, this field will be 999 when UserPrepProc
 *					is called; it must then be re-set to the correct value.
 *		userPageProc A user-provided function that draws the page.  It is defined:
 *						OSErr			UserPageProc(
 *								THPrint		hPrint,
 *								void		*clientData,
 *								Rect		*pageRect,
 *								short		pageNum
 *						);
 *					UserPageProc returns noErr if it completed correctly, or an
 *					error code that will be returned to the user.  It should
 *					return iPrAbort on user-specified aborts.
 */

/*
 * Note: the serial stuff has not been tested in several years.
 */
#ifndef bDevCItoh
#define bDevCItoh		1				/* ImageWriter				*/
#endif
#ifndef bDevLaser
#define bDevLaser		3				/* LaserWriter				*/
#endif
#define ImageWriter		(bDevCItoh)
#define	LaserWriter		(bDevLaser)

OSErr								PrintLog(
		ListHandle						logListHandle,
		THPrint							hPrint
	);
OSErr								LogPrepProc(
		THPrint							hPrint,
		void							*clientData
	);
OSErr								LogPageProc(
		THPrint							hPrint,
		void							*clientData,
		const Rect						*pageRect,
		short							pageNumber
	);
OSErr								PrintDriver(
		THPrint							hPrint,
		Boolean							doStyleDialog,
		void							*clientData,
		OSErr							(*userPrepProc)(
						THPrint		hPrint,
						void		*clientData
					),
		OSErr							(*userPageProc)(
						THPrint		hPrint,
						void		*clientData,
						const Rect	*pageRect,
						short		pageNumber
					)
	);

/*
 * PrintLog, LogPrepProc, and LogPageProc are specific to the LogManager.
 */

OSErr
PrintLog(
		ListHandle						logListHandle,
		THPrint							hPrint
	)
{
		OSErr			status;
		
		status = PrintDriver(
					hPrint,
					FALSE,
					logListHandle,
					LogPrepProc,
					LogPageProc
				);
		return (status);
}

OSErr
LogPrepProc(
		THPrint						hPrint,
		void						*clientData
	)
{
		unsigned short				linesInLog;
		unsigned short				linesPerPage;
		unsigned short				pagesInLog;
		Rect						printRect;
#define logListHandle	((ListHandle) clientData)

		/*
		 * We could add a page header here, of course.
		 */
		printRect = (**hPrint).prInfo.rPage;
		linesInLog = height(LIST.dataBounds);
		linesPerPage = height(printRect) / LIST.cellSize.v;
		pagesInLog = (linesInLog + linesPerPage - 1) / linesPerPage;
		(**hPrint).prJob.iLstPage = pagesInLog;
		return (noErr);
#undef logListHandle
}

OSErr
LogPageProc(
		THPrint							hPrint,
		void							*clientData,
		const Rect						*pageRect,
		short							pageNumber
	)
{
		unsigned short					linesPerPage;
		short							lastCell;
		Point							drawLoc;
		FontInfo						info;
		short							theRow;
		StringHandle					stringHandle;
		char							stringLockState;
#define logListHandle	((ListHandle) clientData)

		if (0) {				/* Touch unused variables	*/
			hPrint;
		}
		linesPerPage = height(*pageRect) / LIST.cellSize.v;
		TextFont(LOGINFO.fontNumber);
		TextSize(LOGINFO.fontSize);
		GetFontInfo(&info);
		theRow = (pageNumber - 1) * linesPerPage + LIST.dataBounds.top;
		lastCell = theRow + linesPerPage - 1;
		if (lastCell > LIST.dataBounds.bottom)
			lastCell = LIST.dataBounds.bottom;
		SetPt(
			&drawLoc,
			LIST.indent.h,
			pageRect->top + info.ascent
		);
		/*
		 * We could add a page header here, of course.
		 */
		for (; theRow < lastCell; ++theRow) {
			stringHandle = GetLogStringHandle(logListHandle, theRow);
			if (stringHandle != NULL) {
				stringLockState = HGetState((Handle) stringHandle);
				HLock((Handle) stringHandle);
				MoveTo(drawLoc.h, drawLoc.v);
				DrawString(*stringHandle);
				HSetState((Handle) stringHandle, stringLockState);
			}
			drawLoc.v += LIST.cellSize.v;
		};
		return (PrError());
}

/*
 * Rich Siegel's generic printer driver, somewhat modified.
 */
OSErr
PrintDriver(
		THPrint							hPrint,
		Boolean							doStyleDialog,
		void							*clientData,
		OSErr							(*userPrepProc)(
						THPrint		hPrint,
						void		*clientData
					),
		OSErr							(*userPageProc)(
						THPrint		hPrint,
						void		*clientData,
						const Rect	*pageRect,
						short		pageNumber
					)
	)
{
		OSErr				status;				/* Random status				*/
		Boolean				ourPrintHandle;		/* Did we allocate hPrint		*/
		Boolean				printIsOpen;		/* PROpen-PRClose	 			*/		
		Boolean				docIsOpen;			/* PROpenDoc-PRCloseDoc			*/
		Boolean				pageIsOpen;			/* PROpenPage-PRClosePage		*/
		short				nCopies;			/* Number of copies 			*/
		short				printDevice;		/* What kind of printer			*/
		Boolean				draftMode;			/* Draft or Spool?				*/
		TPPrPort			printPort;			/* The print port				*/
		TPrStatus			printStatus;		/* PrPicFile status info		*/
		GrafPtr				savePort;			/* Old GrafPort					*/
		short				iCopy;				/* Which copy					*/
		short				page;				/* Which page 					*/
		Rect				pageRect;			/* Current page image rect		*/
/*
 * This macro exits the print handler on any error.
 */
#define	CheckError(s)	do {				\
		if ((status = (s)) != noErr)		\
			goto exit;						\
	} while (0);

		GetPort(&savePort);
		status = noErr;
		printIsOpen = FALSE;
		docIsOpen = FALSE;
		pageIsOpen = FALSE;
		ourPrintHandle = (hPrint == NULL);
		PrOpen();
		CheckError(PrError());
		printIsOpen = TRUE;
		/*
		 * Allocate the print handle if necessary.  memFullErr on failure
		 * it failed.
		 */
		if (ourPrintHandle) {
			hPrint = (THPrint) NewHandle(sizeof (TPrint));
			if (hPrint == NULL) {
				status = MemError();
				goto exit;
			}
			PrintDefault(hPrint);
		}
		/*
		 * Validate the print handle and call the Style Dialog if necessary.  If
		 * the user cancels, just exit (noErr). Then call the job dialog to get
		 * the number of copies. Exit (noErr) if the user Cancels. (Strictly
		 * speaking, we could exit with userCanceledErr, but that just forces
		 * the caller to ignore this informational status.
		 */
		if (PrValidate(hPrint) || doStyleDialog) {
			if (PrStlDialog(hPrint) == FALSE)
				goto exit;
		}
		if (PrJobDialog(hPrint) == FALSE)
			goto exit;
		/*
		 * Our setup is done. call the user's prep procedure and exit on errors.
		 */
		SetCursor(*GetCursor(watchCursor));
		CheckError((*userPrepProc)(hPrint, clientData));
		/*
		 * Grab a few interesting parameters:
		 *	printDevice	Which printer we're using
		 *	is_draftMode	TRUE if we're in draft-mode.
		 *	nCopies			We do copies if we're in draft-mode on an Imagewriter.
		 *					Otherwise, the spooler does it for us. This hasn't
		 *					been tested in years.
		 */
		printDevice = ((**hPrint).prStl.wDev >> 8) & 0xFF;
		draftMode = (**hPrint).prJob.bJDocLoop == bDraftLoop;
		if (draftMode && printDevice == ImageWriter)
			nCopies = (**hPrint).prJob.iCopies;
		else {
			nCopies = 1;
		}
		/*
		 * Printing begins here.  PrOpenDoc sets the port.
		 */
		printPort = PrOpenDoc(hPrint, NULL, NULL);
		docIsOpen = TRUE;
		CheckError(PrError());
		for (iCopy = 1; iCopy <= nCopies; iCopy++) {
			for (page = (**hPrint).prJob.iFstPage;
						page <= (**hPrint).prJob.iLstPage;
						page++) {
				SetCursor(*GetCursor(watchCursor));
				/*
				 * Print the current page.
				 */
				PrOpenPage(printPort, NULL);
				pageIsOpen = TRUE;
				pageRect = (**hPrint).prInfo.rPage;
				status = (*userPageProc)(hPrint, clientData, &pageRect, page);
				PrClosePage(printPort);
				pageIsOpen = FALSE;
				CheckError(status);
			}
		}
		SetPort(savePort);
		PrCloseDoc(printPort);
		docIsOpen = FALSE;
		status = PrError();
		if (draftMode == FALSE && status == noErr) {
			PrPicFile(hPrint, NULL, NULL, NULL, &printStatus);
			status = PrError();
		}
		/*
		 * Everyone exits here
		 */
exit:	SetPort(savePort);
		InitCursor();
		if (pageIsOpen)
			PrClosePage(printPort);
		if (docIsOpen)
			PrCloseDoc(printPort);
		if (printIsOpen)
			PrClose();
		if (ourPrintHandle && hPrint != NULL)
			DisposHandle((Handle) hPrint);
		return (status);
}

