/*									LogManager.h								*/
/*
 * LogManager.h
 * Copyright � 1993 Apple Computer Inc. All rights reserved.
 *
 * These functions manage a logging display for error messages and other text.
 * The log is implemented as a ListManager list that can hold nLogItems. This
 * module is intentionally more-or-less self-contained so it can easily be
 * exported to other applications.
 */
#ifndef __LogManager__
#define __LogManager__
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
#include <Lists.h>
#endif
#include <Printing.h>

/*
 * Usage:
 *		ListHandle					CreateLog(
 *				const Rect				*viewRect,
 *				short					listFontNumber,
 *				short					listFontSize,
 *				short					nLogLines
 *			);
 *	Create a new log display in the current window. To define a log, first
 *	SetPort to the window, and pass viewRect in window coordinates. Returns
 *	NULL on errors. if nLogLines is zero, a default value (128) will be used.
 *	Note: the log will be displayed with a horizontal and vertical scrollbar.
 *	Be sure to dimension the viewRect to leave room for both: they are not
 *	included in the viewRect (for compatibility with other ListManager calls).
 *
 *		void						DisposeLog(
 *				ListHandle				logListHandle
 *			);
 *	Dispose of the log. logListHandle may be NULL without disasterous results.
 *
 *		Boolean						DoClickInLog(
 *				ListHandle				logListHandle,
 *				const EventRecord		*eventRecord
 *			);
 *	Called on a mouse-down in the log viewRect. Returns FALSE if the mouse-down
 *	wasn't in the viewRect (this isn't really an error, but lets you call
 *	DoClickInLog on all mouseDown's). Note: the port should be set to the log
 *	window. You cannot select text in the log. This is a limitation:
 *	Copy to the Clipboard would be useful.
 *
 *		void						UpdateLog(
 *				ListHandle				logListHandle
 *			);
 *	Called on update events in the log window. UpdateLog passes the window
 *	visRgn to the List Manager.
 *
 *		void						UpdateLogWindow(
 *				ListHandle				logListHandle
 *			);
 *	Called to update the log area without an update event. This may be used
 *	after displaying a dialog or alert that might have obscured the window.
 *
 *		void						ActivateLog(
 *				ListHandle				logListHandle,
 *				Boolean					activating
 *			);
 *	Called on activate, deactivate, suspend and resume events.
 *
 *		void						MoveLog(
 *				ListHandle				logListHandle,
 *				short					leftEdge,
 *				short					topEdge
 *			);
 *	Called to move the log within the window (after resizing the window)
 *	LeftEdge and topEdge are in window coordinates.
 *
 *		void						SizeLog(
 *				ListHandle				logListHandle,
 *				short					newWidth,
 *				short					newHeight
 *			);
 *	Resize the log area (after resizing the window). NewWidth and NewHeight
 *	are in window coordinates.
 *
 * The following calls display data in the log.
 *
 *		void						LogStatus(
 *				ListHandle				logListHandle,
 *				OSErr					theError,
 *				ConstStr255Param		infoText
 *			);
 *	If theError is not noErr, display the error value and infoText in the log.
 *
 *		void						DisplayLogString(
 *				ListHandle				logListHandle,
 *				ConstStr255Param		theString
 *			);
 *	Display the argument string in the log.
 *
 *
 * File I/O
 *
 *		OSErr						SaveLogFile(
 *				ListHandle				logListHandle,
 *				ConstStr255Param		dialogPromptString,
 *				ConstStr255Param		defaultFileName,
 *				OSType					creatorType
 *			);
 *	Prompt the user for a file name and write the current log contents
 *	to the chosen file. Returns an error status (noErr is ok). This function
 *	returns userCanceledErr if the user cancelled the SFPutFile dialog.
 *
 *		OSErr						CreateLogFile(
 *				ListHandle				logListHandle,
 *				OSType					creatorType,
 *				ConstStr255Param		fileName,
 *				short					vRefNum
 *			);
 *	Create a log on the specified file and volume. Normally, called only
 *	by SaveLogFile.
 *
 *		void						WriteCurrentLog(
 *				ListHandle				logListHandle
 *			);
 *	Write the current log to the file. This is called when the log file
 *	is first created.
 *
 *		void						WriteLogLine(
 *				ListHandle				logListHandle,
 *				ConstStr255Param		theText
 *			);
 *	Write a single line of text to the currently-open log, if any.
 *	Any errors are stored in the INFO structure. WriteLogLine is
 *	called to "dribble" new text into the log file.
 *
 *		OSErr						CloseLogFile(
 *				ListHandle				logListHandle
 *			);
 *	Close the currently active log file (if any). Returns the earliest
 *	status (i.e., if the disk filled during a "dribble" operation, it
 *	will return a device full error).
 *
 *		OSErr						GetLogFileError(
 *				ListHandle				logListHandle
 *			);
 *	Return the last file error stored in the log record.
 *
 *		Boolean						HasLogFile(
 *				ListHandle				logListHandle
 *			);
 *	TRUE if a log file is currently open.
 *
 * This function prints the log
 *
 *		OSErr						PrintLog(
 *				ListHandle				logListHandle,
 *				THPrint					hPrint
 *			);
 *	Print the current contents of the log. if hPrint is non-null, it will
 *	be used to configure the printer. Otherwise, a Print Handle will be
 *	allocated (and disposed) internally. No page numbers or headers or
 *	other useful stuff. Sorry.
 */

/*
 * This record is stored in the userHandle variable in the list. The values are
 * needed to specify the font, limit the number of error log lines that are stored,
 * and preserve the log area color. We also hide the horizontal scrollbar here
 * because, if we leave it in the ListRecord, the ListManager will decide to
 * permanently un-hilite it.
 *
 * Note: the LogInfoRecord is private to the LogManager routines.
 */
typedef struct LogInfoRecord {
		ControlHandle		hScroll;
		short				logLines;
		short				fontNumber;
		short				fontSize;
		RGBColor			foreColor;
		RGBColor			backColor;
		short				logFileRefNum;		/* Non-zero if log file open	*/
		short				logFileVRefNum;		/* Log file volume refNum		*/
		OSErr				logFileStatus;		/* noError or last log file err	*/
} LogInfoRecord, *LogInfoPtr, **LogInfoHdl;

/*
 * CreateLog
 *		Create a new log display in the current window. To define a log, first
 *		SetPort to the window, and pass viewRect in window coordinates. Returns
 *		NULL on errors. if nLogLines is zero, a default value (128) will be used.
 *		Note: the log will be displayed with a horizontal and vertical scrollbar.
 *		Be sure to dimension the viewRect to leave room for both.
 */
ListHandle							CreateLog(
		const Rect						*viewRect,
		short							listFontNumber,
		short							listFontSize,
		short							nLogLines
	);
/*
 * DisposeLog
 *		Dispose of the log. logListHandle may be NULL.
 */
void								DisposeLog(
		ListHandle						logListHandle
	);
/*
 * DoClickInLog
 *		Call on a mouse-down in the log viewRect. Returns FALSE if the mouse-down
 *		wasn't in the viewRect (this isn't really an error, but lets you call
 *		DoClickInLog on all mouseDown's). Note: the port should be set to the log
 *		window. You cannot select text in the log.
 */
Boolean								DoClickInLog(
		ListHandle						logListHandle,
		const EventRecord				*eventRecord
	);
/*
 * UpdateLog
 *		Call on update events in the log window.
 */
void								UpdateLog(
		ListHandle						logListHandle
	);
/*
 * UpdateLogWindow
 *		Call to force an update on the log's window. This may be used after
 *		displaying a dialog or alert that might have obscured the window.
 */
void								UpdateLogWindow(
		ListHandle						logListHandle
	);
/*
 * ActivateLog
 *		Call on activate, suspend, and resume events.
 */
void								ActivateLog(
		ListHandle						logListHandle,
		Boolean							activating
	);
/*
 * MoveLog
 *		Move the log area within the window.
 */
void								MoveLog(
		ListHandle						logListHandle,
		short							leftEdge,
		short							topEdge
	);
/*
 * SizeLog
 *		Change the size of the log area.
 */
void								SizeLog(
		ListHandle						logListHandle,
		short							newWidth,
		short							newHeight
	);
/*
 * LogStatus
 *		Call with an error status code and some text to display.
 */
void								LogStatus(
		ListHandle						logListHandle,
		OSErr							theError,
		const StringPtr					infoText
	);
/*
 * DisplayLogString
 *		Call with some text to display.
 */
void								DisplayLogString(
		ListHandle						logListHandle,
		const StringPtr					theString
	);
StringHandle						GetLogStringHandle(
		ListHandle						logListHandle,
		short							theRow
	);
/*
 * Log file functions.
 */
OSErr								SaveLogFile(
		ListHandle						logListHandle,
 		ConstStr255Param				dialogPromptString,
		ConstStr255Param				defaultFileName,
		OSType							creatorType
	);

OSErr								CreateLogFile(
		ListHandle						logListHandle,
		OSType							creatorType,
		ConstStr255Param				fileName,
		short							vRefNum
	);
void								WriteCurrentLog(
		ListHandle						logListHandle
	);
void								WriteLogLine(
		ListHandle						logListHandle,
		ConstStr255Param				theText
	);
OSErr								CloseLogFile(
		ListHandle						logListHandle
	);
OSErr								GetLogFileError(
		ListHandle						logListHandle
	);
Boolean								HasLogFile(
		ListHandle						logListHandle
	);
/*
 * Print the log.
 */
OSErr								PrintLog(
		ListHandle						logListHandle,
		THPrint							hPrint
	);
#endif /* __LogManager__ */



