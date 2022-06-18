/*								SCSISimpleSample.h								*/
/*
 * SCSISimpleSample.h
 * Copyright � 1992-94 Apple Computer Inc. All Rights Reserved.
 */
#ifdef REZ
#define kApplicationCreator	'????'			/* Rez can't deal with long hex		*/
#else
#define kApplicationCreator	0x3F3F3F3FL		/* '????' without trigraphs			*/
#endif

#define MBAR_MenuBar		1000
#define MENU_Apple			1
#define MENU_File			128
#define MENU_Edit			129
#define MENU_Test			130
#define MENU_CurrentBus		131
#define MENU_CurrentTarget	132
#define MENU_CurrentLUN		133
#define STRS_SenseBase		1000
#define DLOG_About			128

#define kMinWindowWidth		200
#define kMinWindowHeight	300
#define kLogLines			512

#ifndef REZ
/*
 * These definitions are only for the code files.
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
#include <Packages.h>
#endif

#ifndef TRUE
#define TRUE	1
#define FALSE	0
#endif
#ifndef EXTERN
#define EXTERN	extern
#endif

#include "MacSCSICommand.h"
#include "LogManager.h"

#define kScrollBarWidth		16
#define kScrollBarOffset	(kScrollBarWidth - 1)

/*
 * Items in the Apple Menu
 */
enum {
	kAppleAbout = 1
};

/*
 * Items in the File Menu
 */
enum {
	kFileCreateLogFile = 1,
	kFileCloseLogFile,
	kFileUnused1,
	kFilePageSetup,
	kFilePrint,
	kFileUnused2,
	kFileDebug,
	kFileUnused3,
	kFileQuit
};

enum EditMenu {
	kEditUndo				= 1,
	kEditUnused,
	kEditCut,
	kEditCopy,
	kEditPaste,
	kEditClear
};

/*
 * Items in the test Menu
 */
enum {
	kTestEnableNewManager = 1,
	kTestEnableAllLogicalUnits,
	kTestEnableSelectWithATN,
	kTestUnused1,
	kTestDoDisconnect,
	kTestDontDisconnect,
	kTestUnused2,
	kTestListSCSIDevices,
	kTestGetDriveInfo,
	kTestUnitReady,
	kTestReadBlockZero,
	kTestUnused3,
	kTestVerboseDisplay,
	kTestDummyLastEntryThankYouANSICCommittee
};

/*
 * This is a parameter block for DoSCSICommandWithSense that contains all of the
 * data needed to perform a SCSI command on either the original or asynchronous
 * SCSI Managers. Note: some of this is used only by one or the other manager.
 */
struct ScsiCmdBlock {
	DeviceIdent			scsiDevice;				/* -> Bus/target/LUN			*/
	Ptr					bufferPtr;				/* -> data buffer				*/
	unsigned long		transferSize;			/* -> bytes to transfer			*/
	unsigned long		transferQuantum;		/* -> polled or blocksize		*/
	unsigned short		statusByte;				/* <- From Status Phase			*/
	unsigned long		actualTransferCount;	/* <- Transfer count			*/
	Boolean				writeToDevice;			/* -> Need data out?			*/
	unsigned long		scsiFlags;				/* -> asynch flags				*/
	OSErr				status;					/* <- Current OSErr				*/
	OSErr				requestSenseStatus;		/* <- From RequestSense			*/
	SCSI_Command		command;				/* -> Current command			*/
	SCSI_Sense_Data		sense;					/* <- Gets Sense data			*/
};
typedef struct ScsiCmdBlock ScsiCmdBlock, *ScsiCmdBlockPtr;
/*
 * Notes on the above:
 *	scsiDevice			Used for both managers, but the original manager
 *						ignores the bus and LUN designations. The caller
 *						must stuff the LUN into the command data block.
 *	bufferPtr			If NULL, no read/write is performed.
 *	transferSize		If zero, no read/write is performed.
 *	transferQuantum		This is used by the handshake and TIB setup. Note:
 *						we support only the simplest "512, 0" handshake
 *						and TIB. The following values are useful:
 *							0		transferQuantum := transferSize
 *							1		force polled read/write
 *							> 1		blocked read (for example, 512)
 *	statusByte			Returned from the device
 *	actualTransfercount Returned in bytes (transferQuantum sets granularity)
 *	scsiFlags			Set special flags (scsiDontDisconnect) here.
 *	status				Overall operation status (note special status values)
 *	requestSenseStatus	Has status of Request Sense (only if status was
 *						statusErr, indicating that "Check condition" status.
 */
	
/*
 * These are the things the user can choose from the menu:
 *	ListSCSIDevices				Scan all busses and display info for all
 *								devices (all targets, all LUNs)
 *	DeviceInquiry				Execute an Inquiry command for this device
 *								and display the results.
 *	TestUnitReady				Execute a Test Unit Ready command for this
 *								device and display the results.
 */
void						DoListSCSIDevices(void);
void						DoGetDriveInfo(
		DeviceIdent				scsiDevice,				/* -> Bus/target/LUN	*/
		Boolean					noIntroMsg,
		Boolean					useAsynchManager
	);
void						DoTestUnitReady(
		DeviceIdent				scsiDevice				/* -> Bus/target/LUN	*/
	);
void						DoReadBlockZero(
		DeviceIdent				scsiDevice				/* -> Bus/target/LUN	*/
	);
/*
 * These are low-level commands that are needed to scan the bus.
 */
Boolean						SCSICheckForDevicePresent(
		DeviceIdent				scsiDevice,
		Boolean					enableAsynchSCSI
	);
/*
 * Check whether the asynchronous SCSI Manager may be called for this bus.
 * This will return a status error if the bus is inaccessable. If successful,
 * it will set useAsynchManager FALSE only if this bus is managed by a
 * third-party SCSI hardware interface that operates by patching the
 * original SCSI Manager traps, or uses some other private interface.
 */
OSErr
SCSIBusAPI(
		DeviceIdent				scsiDevice,
		Boolean					*useAsynchManager
	);
/*
 * Get the maximum target for a specified bus.
 */
OSErr
SCSIGetMaxTargetID(
		DeviceIdent						scsiDevice,
		unsigned short					*maxTarget
	);
/*
 * Return the number of SCSI busses on this system.
 */
OSErr						SCSIGetHighHostBusAdaptor(
		unsigned short			*lastHostBus
	);
/*
 * Return the SCSI bus ID of the initiator (Macintosh) on this bus. This will
 * normally return 7. It always returns 7 if the asynchronous SCSI Manager is
 * not present on this machine. Errors are serious.
 */ 
OSErr						SCSIGetInitiatorID(
		DeviceIdent				scsiDevice,
		unsigned short			*initiatorID
	);
/*
 * TRUE if the asynchronous SCSI Manager is running on this machine.
 */
Boolean						AsyncSCSIPresent(void);

/*
 * All commands are performed by this function. If the asynchronous SCSI Manager
 * is present, it is called directly. If it is not present, the original SCSI
 * Manager is called and, if the device returns Check Condition, a Request
 * Sense command is issued.
 */
void						DoSCSICommandWithSense(
		register ScsiCmdBlockPtr	scsiCmdBlockPtr,
		Boolean					displayError,
		Boolean					enableAsynchSCSI
	);

/*
 * The following functions display operation results.
 */
void						ShowSCSIBusID(
		DeviceIdent				scsiDevice,				/* -> Bus/target/LUN	*/
		ConstStr255Param		commandText
	);
void						ShowRequestSense(
		register ScsiCmdBlockPtr	scsiCmdBlockPtr
	);
/*
 * This does the actual formatting and display of the Request Sense results
 */
void						DoShowRequestSense(
		DeviceIdent				scsiDevice,
		OSErr					operationStatus,
		OSErr					requestSenseStatus,
		const SCSI_CommandPtr	scsiCommand,
		const SCSI_Sense_Data	*sensePtr
	);
void						ShowStatusError(
		DeviceIdent				scsiDevice,				/* -> Bus/target/LUN	*/
		OSErr					errorStatus,
		const SCSI_CommandPtr	scsiCommand
	);
void						DoShowSCSICommand(
		const SCSI_CommandPtr	cmdBlock,			/* -> SCSI command			*/
		ConstStr255Param		message
	);
/*
 * The inquiry data is stored in SCB.bufferPtr
 */
void						ShowInquiry(
		register ScsiCmdBlockPtr	scsiCmdBlockPtr
	);
void						DoShowInquiry(
		DeviceIdent				scsiDevice,				/* -> Bus/target/LUN	*/
		const SCSI_Inquiry_Data	*inquiry
	);
void						ShowDeviceState(
		register ScsiCmdBlockPtr	scsiCmdBlockPtr
	);
void						AppendDeviceID(
		StringPtr				result,
		DeviceIdent				scsiDevice				/* -> Bus/target/LUN	*/
	);
void						DisplaySCSIErrorMessage(
		OSErr					errorStatus,
		ConstStr255Param		errorText
	);
/*
 * Use the SCSI-II command class to determine the command length. Returns
 * zero if the length could not be determined.
 */
unsigned short				SCSIGetCommandLength(
		const Ptr				cmdBlock
	);
/*
 * These two functions are called by DoSCSICommandWithSense to perform the
 * actual operation. OriginalSCSI uses the Inside Mac IV SCSI Manager, while
 * AsyncSCSI uses SCSI Manager 4.3.
 * Return codes:
 *
 *	noErr			normal
 *	unimpErr		AsyncSCSI called, but SCSI Manager 4.3 not installed.
 *	scCommErr		Could not select this device or bus busy (Original only)
 *	statusErr		Device returned "Check condition"
 *	controlErr		Device returned "Busy" (Note: device error)
 *	ioErr			Other (serious) device status -- bug.
 *	sc...			Other (Inside Mac IV) SCSI Manager error
 *	scsi...			Other (SCSI Manager 4.3) error.
 */
OSErr						OriginalSCSI(
		unsigned short			targetID,			/* Device ID on this bus	*/
		const SCSI_CommandPtr	scsiCommand,		/* The actual scsi command	*/
		unsigned short			cmdBlockLength,		/* -> Length of CDB			*/
		Boolean					writeToDevice,		/* TRUE to write			*/
		Ptr						bufferPtr,			/* -> user data buffer		*/
		unsigned long			transferSize,		/* How much to transfer		*/
		unsigned long			transferQuantum,	/* TIB setup parameter		*/
		unsigned long			completionTimeout,	/* Ticks to wait			*/
		unsigned short			*stsBytePtr,		/* <- status phase byte		*/
		unsigned long			*actualTransferCount
	);
/*
 * AsyncSCSI returns unimpErr if the _SCSIAtomic (SCSI Manager 4.3)
 * trap is not present. If so, just call the "old" OriginalSCSI.
 */
OSErr						AsyncSCSI(
		DeviceIdent				scsiDevice,			/* -> Bus/target/LUN		*/
		const SCSI_CommandPtr	scsiCommand,		/* The actual scsi command	*/
		unsigned short			cmdBlockLength,		/* -> Length of CDB			*/
		Boolean					writeToDevice,		/* TRUE to write			*/
		Ptr						bufferPtr,			/* -> user data buffer		*/
		unsigned long			transferSize,		/* How much to transfer		*/
		unsigned short			scsiHandshake[handshakeDataLength],
		SCSI_Sense_Data			*senseDataPtr,		/* Request Sense results	*/
		unsigned long			senseDataSize,		/* Request Sense data size	*/
		unsigned long			completionTimeout,	/* Ticks to wait			*/
		unsigned short			*stsBytePtr,		/* <- status phase byte		*/
		unsigned long			*actualTransferCount
	);

/*
 * String formatting utilities (for debugging/display)
 */
/*
 * AppendChar writes a character into the string. Note that
 * it wraps around if the string size exceeds 255 bytes.
 */
#define AppendChar(result, c) (result[++result[0]] = (c))
void						AppendUnsigned(
		StringPtr				result,
		unsigned long			value
	);
void						AppendSigned(
		StringPtr				result,
		signed long				value
	);
void						AppendUnsignedLeadingZeros(
		StringPtr				result,
		unsigned long			value,
		short					fieldWidth,
		short					terminatorChar
	);
void						AppendHexLeadingZeros(
		StringPtr				result,
		unsigned long			value,
		short					fieldWidth
	);
void						AppendUnsignedInField(
		StringPtr				result,
		unsigned long			value,
		short					fieldWidth
	);
void						AppendBytes(
		StringPtr				result,
		const Ptr				source,
		unsigned short			length
	);
void						AppendPascalString(
		StringPtr				result,
		const StringPtr			value
	);
void						AppendCString(
		StringPtr				result,
		const char				*source,
		unsigned short			maxLength		/* Ignored if zero	*/
	);
void						AppendOSType(
		StringPtr				result,
		OSType					value
	);
/*
 * Window Utilities
 */
void						DoZoomWindow(
		WindowPtr				theWindow,
		short					whichPart
	);
Boolean						DoGrowWindow(
		WindowPtr				theWindow,
		Point					eventWhere,
		short					minimumWidth,
		short					minimumHeight
	);
void						MyDrawGrowIcon(
		WindowPtr				theWindow
	);

/*
 * Format a block of data into the log.
 */
void						DisplayDataBlock(
		Ptr						dataPtr,
		unsigned short			dataLength
	);
/*
 * Cheap 'n dirty pascal string copy routine.
 */
#define pstrcpy(dst, src) do {							\
		StringPtr	_src = (StringPtr) (src);			\
		BlockMove(_src, dst, _src[0] + 1);				\
	} while (0)
/*
 * Cheap 'n dirty pascal string concat.
 */
#define pstrcat(dst, src) do {							\
		StringPtr		_dst = (dst);					\
		StringPtr		_src = (StringPtr) (src);		\
		short			_len;							\
		_len = 255 - _dst[0];							\
		if (_len > _src[0]) _len = _src[0];				\
		BlockMove(&_src[1], &_dst[1] + _dst[0], _len);	\
		_dst[0] += _len;								\
	} while (0)

/*
 * Cheap 'n dirty memory clear routine.
 */
#define CLEAR(record) do {								\
		register char	*ptr = (char *) &record;		\
		register long	size;							\
		for (size = sizeof record; size > 0; --size)	\
			*ptr++ = 0;									\
	} while (0)

#define width(r)	((r).right - (r).left)
#define height(r)	((r).bottom - (r).top)

#define LOG(what)	DisplayLogString(gLogListHandle, (what))
#define VERBOSE(what) do {								\
		if (gVerboseDisplay)							\
			LOG(what);									\
	} while (0)
/*
 * Global variables.
 */
EXTERN WindowPtr				gMainWindow;
EXTERN EventRecord				gCurrentEvent;
#define EVENT					(gCurrentEvent)
EXTERN ListHandle				gLogListHandle;
EXTERN THPrint					gPrintHandle;
EXTERN Boolean					gQuitNow;
EXTERN Boolean					gUpdateMenusNeeded;
EXTERN Boolean					gInForeground;
/*
 * These flags are set/cleared by menu options to control the asynchronous SCSI
 * Manager -- they are not used if the asynchronous manager is not present.
 *	gEnableNewSCSIManager		FALSE to always use the original manager,
 *								even if the new manager is present.
 *	gEnableSelectWithATN		FALSE to supress Select With ATN.
 *	gDoDisconnect				Set the scsibDoDisconnect flag
 *	gDontDisconnect				Set the scsibDontDisconnect flag.
 *								Note: both "do" and "don't" may be set.
 */
EXTERN Boolean					gEnableNewSCSIManager;
EXTERN Boolean					gEnableSelectWithATN;
EXTERN Boolean					gDoDisconnect;
EXTERN Boolean					gDontDisconnect;
EXTERN Boolean					gVerboseDisplay;
EXTERN MenuHandle				gAppleMenu;
EXTERN MenuHandle				gFileMenu;
EXTERN MenuHandle				gEditMenu;
EXTERN MenuHandle				gTestMenu;
EXTERN MenuHandle				gCurrentBusMenu;
EXTERN MenuHandle				gCurrentTargetMenu;
EXTERN MenuHandle				gCurrentLUNMenu;
EXTERN DeviceIdent				gCurrentDevice;
EXTERN unsigned short			gMaxLogicalUnit;
EXTERN DeviceIdent				*gDeviceList;
EXTERN unsigned short			gMaxDevice;		/* Number of items in gDeviceList	*/

#endif		/* REZ			*/
