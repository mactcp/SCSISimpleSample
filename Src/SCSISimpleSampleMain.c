/*								SCSISimpleSampleMain.c							*/
/*
 * SCSISimpleSampleMain.c
 * Copyright � 1992-94 Apple Computer Inc. All Rights Reserved.
 *
 * This is a "simple sample" that illustrates calls to the original and the
 * asynchronous SCSI Managers. In particular, it contains "definitive"
 * algorithms for determining the number of SCSI devices attached to a
 * Macintosh system.
 */
#define EXTERN
#include	"SCSISimpleSample.h"
#include	<Packages.h>
#include	<Desk.h>
#include	<OSEvents.h>
#pragma segment MainCode

#ifdef __powerc
/*
 * Power PC does not automatically define the QuickDraw globals. This is because
 * only "application" code-fragments need these globals, and this cannot be
 * determined before the code fragment is constructed.
 */
QDGlobals			qd;		/* This is not automatically defined on PowerPC		*/
#endif

void								main(void);
void								EventLoop(void);
void								DoMouseEvent(void);
void								DoContentClick(
		WindowPtr						theWindow
	);
void								DoCommand(
		WindowPtr						theWindow,
		long							menuChoice
	);
void								DoWindowKeyDown(
		WindowPtr						theWindow
	);
void								SetupEverything(void);
void								AdjustMenus(void);
void								AdjustEditMenu(
		Boolean							isDeskAccessory
	);
void								SetupMenus(void);
void								BuildWindow(void);
void								DecorateDisplay(
		WindowPtr						theWindow,
		Boolean							redraw
	);
void								DrawWindow(
		WindowPtr						theWindow
	);
void								DoAbout(void);
void								DoPageSetup(void);
void								HexToString(
		unsigned long					value,
		short							hexDigits,
		Str255							result
	);
Boolean								HexToNum(
		ConstStr255Param				hexString,
		unsigned long					*result
	);
		
#define IsOurWindow(theWindow)	((theWindow) == gMainWindow)
/*
 * This preserves the currently-chosen host bus ID in case the user switches
 * between "old" and "new" SCSI Managers.
 */
static unsigned short				gOldHostBusID;

void
main(void)
{
		SetupEverything();
		BuildWindow();
		gUpdateMenusNeeded = TRUE;
		gInForeground = TRUE;
		gEnableNewSCSIManager = AsyncSCSIPresent();
		if (gEnableNewSCSIManager)
			LOG("\pHas asynchronous SCSI Manager");
		else {
			LOG("\pAsynchronous SCSI Manager not present");
		}
		gEnableSelectWithATN = gEnableNewSCSIManager;
		InitCursor();
		while (gQuitNow == FALSE) {
			EventLoop();
		}
		ExitToShell();
}

/*
 * BuildWindow creates a document window and stores a text log
 * as its content.
 */
void
BuildWindow()
{
		short					fontSize;
		short					fontNumber;
		Rect					viewRect;
		
		fontSize = 10;
		GetFNum("\pCourier", &fontNumber);
		if (RealFont(fontNumber, fontSize) == FALSE)
			fontNumber = applFont;
		viewRect = qd.screenBits.bounds;
		viewRect.top += (GetMBarHeight() * 2);
		viewRect.bottom -= 4;
		viewRect.left += 4;
		viewRect.right = (width(viewRect) / 2);
		gMainWindow = NewWindow(
						NULL,
						&viewRect,
						"\pSCSI Scan Bus Sample",
						TRUE,
						zoomDocProc,
						(WindowPtr) -1L,
						TRUE,				/* Has GoAway box		*/
						0					/* No refCon			*/
					);
		if (gMainWindow == NULL) {
			SysBeep(10);
			ExitToShell();
		}
		SetPort(gMainWindow);
		viewRect = gMainWindow->portRect;
		viewRect.right -= kScrollBarOffset;
		viewRect.bottom -= kScrollBarOffset;
		gLogListHandle = CreateLog(
					&viewRect,
					fontNumber,
					fontSize,
					kLogLines
				);
		if (gLogListHandle == NULL) {
			SysBeep(10);
			ExitToShell();
		}
}

void
EventLoop(void)
{
		long							menuChoice;
		register WindowPtr				theWindow;
		GrafPtr							savePort;
		Boolean							isActivating;
		
		if (gUpdateMenusNeeded) {
			gUpdateMenusNeeded = FALSE;
			AdjustMenus();
		}
		WaitNextEvent(
			everyEvent,
			&EVENT,
			(gInForeground) ? 10L : 60L,
			NULL
		);
		theWindow = FrontWindow();
		switch (EVENT.what) {
		case nullEvent:
			break;
		case keyDown:
		case autoKey:
			if ((EVENT.message & charCodeMask) == '.'
			 && (EVENT.modifiers & cmdKey) != 0) {
				FlushEvents(keyDown | autoKey, 0);
				gQuitNow = TRUE;
			}
			else if ((EVENT.modifiers & cmdKey) != 0) {
				if (EVENT.what == keyDown) {
					menuChoice = MenuKey(EVENT.message & charCodeMask);
					if (HiWord(menuChoice) != 0)
						DoCommand(theWindow, menuChoice);
					else if (IsOurWindow(theWindow)) {
						DoWindowKeyDown(theWindow);
					}
				}
			}
			else if (IsOurWindow(theWindow)) {
				DoWindowKeyDown(theWindow);
			}
			break;
		case mouseDown:
			DoMouseEvent();
			break;
		case updateEvt:
			theWindow = (WindowPtr) EVENT.message;
			GetPort(&savePort);
			SetPort(theWindow);
			BeginUpdate(theWindow);
			EraseRect(&theWindow->portRect);
			DrawControls(theWindow);
			DrawGrowIcon(theWindow);
			if (IsOurWindow(theWindow))
				DrawWindow(theWindow);
			EndUpdate(theWindow);
			SetPort(savePort);
			break;
		case activateEvt:
			theWindow = (WindowPtr) EVENT.message;
			isActivating = ((EVENT.modifiers & activeFlag) != 0);
			goto activateEvent;
			break;
		case osEvt:
			switch (((unsigned long) EVENT.message) >> 24) {
			case mouseMovedMessage:
				break;
			case suspendResumeMessage:
				isActivating = ((EVENT.message & 0x01) != 0);
activateEvent:		if (isActivating) {
					/*
					 * Activate this window. Activate events define theWindow
					 * from the event record, while suspend/resume uses the
					 * pre-set FrontWindow value.
					 */
					SelectWindow(theWindow);
					(void) TEFromScrap();
				}
				if (IsOurWindow(theWindow) && gLogListHandle != NULL)
					LActivate(isActivating, gLogListHandle);
				else {
					/* Desk accessory or what? */
				}
				gInForeground = isActivating;
				gUpdateMenusNeeded = TRUE;
				break;
			}
			break;
		}
}

/*
 * DoMouseEvent
 * The user clicked on something. Handle application-wide processing here, or call
 * a Catalog Browser function for specific action.
 */
void
DoMouseEvent(void)
{
		WindowPtr		theWindow;
		short			whichPart;
		
		whichPart = FindWindow(EVENT.where, &theWindow);
		if (theWindow == NULL)
			theWindow = FrontWindow();
		if (whichPart == inMenuBar && IsOurWindow(theWindow) == FALSE)
			theWindow = FrontWindow();
		switch (whichPart) {
		case inDesk:
			break;
		case inMenuBar:
			InitCursor();
			DoCommand(theWindow, MenuSelect(EVENT.where));
			break;
		case inDrag:
			DragWindow(theWindow, EVENT.where, &qd.screenBits.bounds);
			break;
		case inGoAway:
			if (TrackGoAway(theWindow, EVENT.where)) {
				if (IsOurWindow(theWindow)) {
					/*
					 * Not quite so simple: we need to handle open files, too.
					 */
					gQuitNow = TRUE;
				}
			}
			break;
		case inZoomIn:
		case inZoomOut:
			if (IsOurWindow(theWindow)
			 && TrackBox(theWindow, EVENT.where, whichPart)) {
				DoZoomWindow(theWindow, whichPart);
				goto resizeWindow;
			}
			break;
		case inGrow:
			if (IsOurWindow(theWindow)) {
				if (DoGrowWindow(
							theWindow,
							EVENT.where,
							kMinWindowWidth,
							kMinWindowHeight
						)) {
resizeWindow:		DecorateDisplay(theWindow, TRUE);
				}
			}
			break;
		case inContent:
			if (theWindow != FrontWindow())
				SelectWindow(theWindow);
			else if (IsOurWindow(theWindow)) {
				DoContentClick(theWindow);
			}
			else {
				/* Nothing happens here		*/
			}
			break;
		default:
			break;
		}
		/*
		 * Do not touch theWindow here.
		 */
}

void
DoContentClick(
		WindowPtr				theWindow
	)
{
		if (0) {				/* Touch unused variables	*/
			theWindow;
		}
		DoClickInLog(gLogListHandle, &EVENT);
}

void
DoWindowKeyDown(
		WindowPtr						theWindow
	)
{
		if (0) {				/* Touch unused variables	*/
			theWindow;
		}
		/* Nothing happens here */
}

void
DrawWindow(
		WindowPtr						theWindow
	)
{
		if (0) {				/* Touch unused variables	*/
			theWindow;
		}
		UpdateLog(gLogListHandle);
}

void
DecorateDisplay(
		WindowPtr						theWindow,
		Boolean							redraw
	)
{
		Rect							viewRect;
		
		if (0) {				/* Touch unused variables	*/
			redraw;
		}
		viewRect = theWindow->portRect;
		viewRect.right -= kScrollBarOffset;
		viewRect.bottom -= kScrollBarOffset;
		if (gLogListHandle != NULL) {
			MoveLog(gLogListHandle, viewRect.left, viewRect.top);
			SizeLog(gLogListHandle, width(viewRect), height(viewRect));
		}
}

void
DoCommand(
		WindowPtr						theWindow,
		long							menuChoice
	)
{
		short							menuItem;
		Str255							menuText;
		GrafPtr							savePort;
		OSErr							status;
		Boolean							useAsynchManager;

		menuItem = LoWord(menuChoice);
		switch (HiWord(menuChoice)) {
		case MENU_Apple:
			if (menuItem == kAppleAbout)
				DoAbout();
			else {
				GetItem(gAppleMenu, menuItem, menuText);
				AdjustEditMenu(TRUE);
				GetPort(&savePort);
				OpenDeskAcc(menuText);
				SetPort(savePort);
				AdjustEditMenu(IsOurWindow(theWindow) == FALSE);
			}
			break;
		case MENU_File:
			switch (menuItem) {
			case kFileCreateLogFile:
				status = SaveLogFile(
							gLogListHandle,
							"\pSave Log",
							"\pSCSI Test Log",
							'ttxt'
						);
				break;
			case kFileCloseLogFile:
				status = CloseLogFile(gLogListHandle);
				break;
			case kFilePageSetup:
				DoPageSetup();
				break;
			case kFilePrint:
				PrintLog(gLogListHandle, gPrintHandle);
				break;
			case kFileDebug:
				Debugger();
				break;
			case kFileQuit:
				gQuitNow = TRUE;
				break;
			}
			break;
		case MENU_Test:
			switch (menuItem) {
			case kTestEnableNewManager:
				/*
				 * Note that this menu option is disabled if the asynchronous
				 * SCSI Manager is not present. This means that, if it is
				 * not present at application start, it can't be enabled later.
				 */
				gEnableNewSCSIManager = (!gEnableNewSCSIManager);
				if (gEnableNewSCSIManager)
					gCurrentDevice.bus = gOldHostBusID;
				else {
					gOldHostBusID = gCurrentDevice.bus;
					gCurrentDevice.bus = 0;
				}
				gUpdateMenusNeeded = TRUE;
				break;
			case kTestEnableAllLogicalUnits:
				/*
				 * The ROM SCSI Manager on the Quadra 660-AV and 840-AV has
				 * a bug that will hang the machine if it tries to access
				 * a non-zero logical unit. This menu option makes that error
				 * harder to elicit.
				 */
				gMaxLogicalUnit = (gMaxLogicalUnit == 0) ? 7 : 0;
				if (gCurrentDevice.LUN > gMaxLogicalUnit)
					gCurrentDevice.LUN = gMaxLogicalUnit;
				gUpdateMenusNeeded = TRUE;
				break;
			case kTestEnableSelectWithATN:
				gEnableSelectWithATN = !gEnableSelectWithATN;
				gUpdateMenusNeeded = TRUE;
				break;
			case kTestDoDisconnect:
				gDoDisconnect = !gDoDisconnect;
				gUpdateMenusNeeded = TRUE;
				break;
			case kTestDontDisconnect:
				gDontDisconnect = !gDontDisconnect;
				gUpdateMenusNeeded = TRUE;
				break;
			case kTestVerboseDisplay:
				gVerboseDisplay = (!gVerboseDisplay);
				gUpdateMenusNeeded = TRUE;
				break;
			case kTestListSCSIDevices:
				DoListSCSIDevices();
				break;
			case kTestGetDriveInfo:
				status = SCSIBusAPI(gCurrentDevice, &useAsynchManager);
				if (status != noErr)
					useAsynchManager = FALSE;
				DoGetDriveInfo(gCurrentDevice, FALSE, useAsynchManager);
				break;
			case kTestUnitReady:
				DoTestUnitReady(gCurrentDevice);
				break;
			case kTestReadBlockZero:
				DoReadBlockZero(gCurrentDevice);
				break;
			default:
				break;
			}
			break;
		case MENU_CurrentBus:
			gCurrentDevice.bus = menuItem - 1;
			gUpdateMenusNeeded = TRUE;
			break;
		case MENU_CurrentTarget:
			gCurrentDevice.targetID = menuItem - 1;
			gUpdateMenusNeeded = TRUE;
			break;
		case MENU_CurrentLUN:
			gCurrentDevice.LUN = menuItem - 1;
			gUpdateMenusNeeded = TRUE;
			break;
		}
		HiliteMenu(0);
}

void
SetupEverything()
{
		int					i;
		
		MaxApplZone();
		InitGraf(&qd.thePort);
		InitFonts();
		InitWindows();
		InitMenus();
		TEInit();
		InitDialogs(0);
		for (i = 0; i < 8; i++)
			MoreMasters();
		HNoPurge((Handle) GetCursor(watchCursor));
		SetCursor(*GetCursor(watchCursor));
		SetupMenus();
}

void
SetupMenus()
{
		register Handle		menuBarHdl;

		/*
		 * We ought to do some error checking here.
		 */
		menuBarHdl = GetNewMBar(MBAR_MenuBar);
		SetMenuBar(menuBarHdl);
		gAppleMenu = GetMHandle(MENU_Apple);
		AddResMenu(gAppleMenu, 'DRVR');
		gFileMenu = GetMHandle(MENU_File);
		gEditMenu = GetMHandle(MENU_Edit);
		gTestMenu = GetMHandle(MENU_Test);
		gCurrentBusMenu = GetMHandle(MENU_CurrentBus);
		gCurrentTargetMenu = GetMHandle(MENU_CurrentTarget);
		gCurrentLUNMenu = GetMHandle(MENU_CurrentLUN);
		DrawMenuBar();
		gUpdateMenusNeeded = TRUE;
}

void
AdjustMenus(void)
{
		short					i;
		short					nItems;
		unsigned short			maxTarget;
		unsigned short			lastHostBus;
		OSErr					status;
		
		EnableItem(gFileMenu, kFileQuit);
		EnableItem(gFileMenu, kFileDebug);
		if (IsOurWindow(FrontWindow())) {
			EnableItem(gFileMenu, kFilePageSetup);
			EnableItem(gFileMenu, kFilePrint);
			EnableItem(gTestMenu, kTestListSCSIDevices);
			EnableItem(gTestMenu, kTestGetDriveInfo);
			EnableItem(gTestMenu, kTestUnitReady);
			EnableItem(gTestMenu, kTestVerboseDisplay);
			CheckItem(gTestMenu, kTestVerboseDisplay, gVerboseDisplay);	
			EnableItem(gTestMenu, kTestEnableAllLogicalUnits);
			CheckItem(gTestMenu, kTestEnableAllLogicalUnits, (gMaxLogicalUnit == 7));
			if (AsyncSCSIPresent()) {
				EnableItem(gTestMenu, kTestEnableNewManager);
			}
			else {
				DisableItem(gTestMenu, kTestEnableNewManager);
			}
			if (gEnableNewSCSIManager) {
				EnableItem(gCurrentBusMenu, 0);
				EnableItem(gTestMenu, kTestEnableSelectWithATN);
				EnableItem(gTestMenu, kTestDoDisconnect);
				EnableItem(gTestMenu, kTestDontDisconnect);
			}
			else {
				DisableItem(gCurrentBusMenu, 0);
				DisableItem(gTestMenu, kTestEnableSelectWithATN);
				DisableItem(gTestMenu, kTestDoDisconnect);
				DisableItem(gTestMenu, kTestDontDisconnect);
			}
			CheckItem(gTestMenu, kTestEnableNewManager, gEnableNewSCSIManager);
			CheckItem(gTestMenu, kTestEnableSelectWithATN, gEnableSelectWithATN);
			CheckItem(gTestMenu, kTestDoDisconnect, gDoDisconnect);
			CheckItem(gTestMenu, kTestDontDisconnect, gDontDisconnect);
			EnableItem(gTestMenu, kTestEnableAllLogicalUnits);
			CheckItem(gTestMenu, kTestEnableAllLogicalUnits, (gMaxLogicalUnit == 7));
			/* */
			status = SCSIGetMaxTargetID(gCurrentDevice, &maxTarget);
			if (status != noErr)
				maxTarget = 7;
			nItems = CountMItems(gCurrentTargetMenu);
			for (i = 1; i <= nItems; i++) {
				if ((i - 1) <= maxTarget)
					EnableItem(gCurrentTargetMenu, i);
				else {
					DisableItem(gCurrentTargetMenu, i);
				}
			}
			for (i = 1; i <= nItems; i++)
				CheckItem(gCurrentTargetMenu, i, (gCurrentDevice.targetID == (i - 1)) ? TRUE : FALSE);
			status = SCSIGetHighHostBusAdaptor(&lastHostBus);
			nItems = CountMItems(gCurrentBusMenu);
			for (i = 1; i <= nItems; i++) {
				if ((i - 1) <= lastHostBus)
					EnableItem(gCurrentBusMenu, i);
				else {
					DisableItem(gCurrentBusMenu, i);
				}
			}
			for (i = 1; i <= nItems; i++)
				CheckItem(gCurrentBusMenu, i, (gCurrentDevice.bus == (i - 1)) ? TRUE : FALSE);
			nItems = CountMItems(gCurrentLUNMenu);
			for (i = 1; i <= nItems; i++) {
				if ((i - 1) <= gMaxLogicalUnit)
					EnableItem(gCurrentLUNMenu, i);
				else {
					DisableItem(gCurrentLUNMenu, i);
				}
				CheckItem(gCurrentLUNMenu, i, (gCurrentDevice.LUN == (i - 1)) ? TRUE : FALSE);
			}
			/* */
			if (HasLogFile(gLogListHandle)) {
				EnableItem(gFileMenu, kFileCloseLogFile);
				DisableItem(gFileMenu, kFileCreateLogFile);
			}
			else {
				EnableItem(gFileMenu, kFileCreateLogFile);
				DisableItem(gFileMenu, kFileCloseLogFile);
			}
		}
		AdjustEditMenu(IsOurWindow(FrontWindow()) == FALSE);
}

/*
 * AdjustEditMenu
 * Enable/disable Edit Menu options.
 */
void
AdjustEditMenu(
		Boolean				isDeskAcc
	)
{
		if (isDeskAcc) {
			EnableItem(gEditMenu, kEditUndo);
			EnableItem(gEditMenu, kEditCut);
			EnableItem(gEditMenu, kEditCopy);
			EnableItem(gEditMenu, kEditPaste);
			EnableItem(gEditMenu, kEditClear);
		}
		else {
			DisableItem(gEditMenu, kEditUndo);
			DisableItem(gEditMenu, kEditCut);
			DisableItem(gEditMenu, kEditCopy);
			DisableItem(gEditMenu, kEditPaste);
			DisableItem(gEditMenu, kEditClear);
		}
}

void
DoAbout()
{
		GrafPtr							savePort;
		DialogPtr						dialog;
		short							item;
		
		GetPort(&savePort);
		dialog = GetNewDialog(DLOG_About, NULL, (WindowPtr) -1L);
		ShowWindow(dialog);
		SetPort(dialog);
		ModalDialog(NULL, &item);
		DisposDialog(dialog);
		SetPort(savePort);
}

void
HexToString(
		unsigned long					value,
		short							hexDigits,
		Str255							result
	)
{
		register unsigned short			digit;
		register char					*text;
		
		text = (char *) result;
		text[0] = hexDigits;
		for (; hexDigits > 0; --hexDigits) {
			digit = value & 0xF;
			if (digit >= 10)
				text[hexDigits] = digit - 10 + 'a';
			else {
				text[hexDigits] = digit + '0';
			}
			value >>= 4;
		}
}

Boolean
HexToNum(
		ConstStr255Param				hexString,
		unsigned long					*result
	)
{
		register short					i;
		register unsigned short			digit;
		
		*result = 0;
		for (i = 1; i <= hexString[0]; i++) {
			digit = hexString[i];
			if (digit >= '0' && digit <= '9')
				digit -= '0';
			else if (digit >= 'a' && digit <= 'f')
				digit = digit - 'a' + 10;
			else if (digit >= 'A' && digit <= 'F')
				digit = digit - 'A' + 10;
			else {
				return (FALSE);
			}
			*result <<= 4;
			*result |= digit;
		}
		return (TRUE);
}

void
DoPageSetup(void)
{
		PrOpen();
		if (PrError() == noErr) {
			if (gPrintHandle == NULL) {
				gPrintHandle = (THPrint) NewHandle(sizeof (TPrint));
				if (gPrintHandle != NULL)
					PrintDefault(gPrintHandle);
			}
			if (gPrintHandle != NULL)
				(void) PrStlDialog(gPrintHandle);
			PrClose();
		}
}

