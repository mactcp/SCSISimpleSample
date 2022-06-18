/*							MacScsiCommand.h							*/
/*
 * Scsi-specific definitions.
 */
#ifndef __MacSCSICommand__
#define __MacSCSICommand__

//#include <stddef.h>
/*
 * Include the O.S. files in a specific order to make sure that we have
 * a definition for the _SCSIAtomic trap.
 */
#include <Traps.h>
#ifndef _SCSIAtomic
#define _SCSIAtomic	0xA089
#endif
/*
 * This uses the new "common" SCSI.h which is not yet in the public
 * header folders.
 */
#include "SCSI.h"

#ifndef NULL
#define NULL		0
#endif

/*
 * The 6-byte commands are used for most simple
 * I/O requests.
 */
struct SCSI_6_Byte_Command {				/* Six-byte command			*/
	unsigned char		opcode;				/*  0						*/
	unsigned char		lbn3;				/*  1 lbn in low 5			*/
	unsigned char		lbn2;				/*  2						*/
	unsigned char		lbn1;				/*  3						*/
	unsigned char		len;				/*  4						*/
	unsigned char		ctrl;				/*  5						*/
};
typedef struct SCSI_6_Byte_Command SCSI_6_Byte_Command;

struct SCSI_10_Byte_Command {				/* Ten-byte command			*/
	unsigned char		opcode;				/*  0						*/
	unsigned char		lun;				/*  1						*/
	unsigned char		lbn4;				/*  2						*/
	unsigned char		lbn3;				/*  3						*/
	unsigned char		lbn2;				/*  4						*/
	unsigned char		lbn1;				/*  5						*/
	unsigned char		pad;				/*  6						*/
	unsigned char		len2;				/*  7						*/
	unsigned char		len1;				/*  8						*/
	unsigned char		ctrl;				/*  9						*/
};
typedef struct SCSI_10_Byte_Command SCSI_10_Byte_Command;

struct SCSI_12_Byte_Command {				/* Twelve-byte command		*/
	unsigned char		opcode;				/*  0						*/
	unsigned char		lun;				/*  1						*/
	unsigned char		lbn4;				/*  2						*/
	unsigned char		lbn3;				/*  3						*/
	unsigned char		lbn2;				/*  4						*/
	unsigned char		lbn1;				/*  5						*/
	unsigned char		len4;				/*  6						*/
	unsigned char		len3;				/*  7						*/
	unsigned char		len2;				/*  8						*/
	unsigned char		len1;				/*  9						*/
	unsigned char		pad;				/* 10						*/
	unsigned char		ctrl;				/* 11						*/
};
typedef struct SCSI_12_Byte_Command SCSI_12_Byte_Command;

/*
 * This union defines all scsi commands.
 */
union SCSI_Command {
	SCSI_6_Byte_Command		scsi6;
	SCSI_10_Byte_Command	scsi10;
	SCSI_12_Byte_Command	scsi12;
	unsigned char			scsi[12];
};
typedef union SCSI_Command SCSI_Command, *SCSI_CommandPtr;

/*
 * Returned by a read-capacity command.
 */
struct SCSI_Capacity_Data {
	unsigned char		lbn4;				/* Number					*/
	unsigned char		lbn3;				/*  of						*/
	unsigned char		lbn2;				/*   logical				*/
	unsigned char		lbn1;				/*    blocks				*/
	unsigned char		len4;				/* Length					*/
	unsigned char		len3;				/*  of each					*/
	unsigned char		len2;				/*   logical block			*/
	unsigned char		len1;				/*    in bytes				*/
};
typedef struct SCSI_Capacity_Data SCSI_Capacity_Data;

struct SCSI_Inquiry_Data {					/* Inquiry returns this		*/
	unsigned char		devType;			/*  0 Device type,			*/
	unsigned char		devTypeMod;			/*  1 Device type modifier	*/
	unsigned char		version;			/*  2 ISO/ECMA/ANSI version	*/
	unsigned char		format;				/*  3 Response data format	*/
	unsigned char		length;				/*  4 Additional Length		*/
	unsigned char		reserved5;			/*  5 Reserved				*/
	unsigned char		reserved6;			/*  6 Reserved				*/
	unsigned char		flags;				/*  7 Capability flags		*/
	unsigned char		vendor[8];			/*  8-15 Vendor-specific	*/
	unsigned char		product[16];		/* 16-31 Product id			*/
	unsigned char		revision[4];		/* 32-35 Product revision	*/
	unsigned char		vendorSpecific[20]; /* 36-55 Vendor stuff		*/
	unsigned char		moreReserved[40];	/* 56-95 Reserved			*/
};
typedef struct SCSI_Inquiry_Data SCSI_Inquiry_Data;

/*
 * This bit may be set in devTypeMod
 */
enum {
	kScsiInquiryRMB = 0x80					/* Removable medium	if set	*/
};
/*
 * These bits may be set in flags
 */
enum {
	kScsiInquiryRelAdr	= 0x80,				/* Has relative addressing	*/
	kScsiInquiryWBus32	= 0x40,				/* Wide (32-bit) transfers	*/
	kScsiInquiryWBus16	= 0x20,				/* Wide (16-bit) transfers	*/
	kScsiInquirySync	= 0x10,				/* Synchronous transfers	*/
	kScsiInquiryLinked	= 0x08,				/* Linked commands ok		*/
	kScsiInquiryReserved = 0x04,
	kScsiInquiryCmdQue	= 0x02,				/* Tagged cmd queuing ok	*/
	kScsiInquirySftRe	= 0x01				/* Soft reset alternative	*/
};

enum {
	kScsiDevTypeDirect					= 0,
	kScsiDevTypeSequential,
	kScsiDevTypePrinter,
	kScsiDevTypeProcessor,
	kScsiDevTypeWorm,						/* Write-once, read multiple		*/
	kScsiDevTypeCDROM,
	kScsiDevTypeScanner,
	kScsiDevTypeOptical,
	kScsiDevTypeChanger,
	kScsiDevTypeComm,
	kScsiDevTypeGraphicArts0A,
	kScsiDevTypeGraphicArts0B,
	kScsiDevTypeFirstReserved,				/* Start of reserved sequence		*/
	kScsiDevTypeUnknownOrMissing		= 0x1F,
	kScsiDevTypeMask					= 0x1F
};
/*
 * These are device type modifiers. We need them to distinguish between "unknown"
 * and "missing" devices.
 */
enum {
	kScsiDevTypeQualifierConnected		= 0x00,	/* Exists and is connected		*/
	kScsiDevTypeQualifierNotConnected	= 0x20,	/* Logical unit exists			*/
	kScsiDevTypeQualifierReserved		= 0x40,
	kScsiDevTypeQualifierMissing		= 0x60,	/* No such logical unit			*/
	kScsiDevTypeQualifierVendorSpecific	= 0x80,	/* Other bits are unspecified	*/
	kScsiDevTypeQualifierMask			= 0xE0
};
#define kScsiDevTypeMissing	\
	(kScsiDevTypeUnknownOrMissing | kScsiDevTypeQualifierMissing)

/*
 * This is the data that is returned after a GetExtendedStatus
 * request. The errorCode gives a general indication of the error,
 * which may be qualified by the additionalSenseCode and
 * additionalSenseQualifier fields. These may be device (vendor)
 * specific values, however. The info[] field contains additional
 * information. For a media error, it contains the failing
 * logical block number (most-significant byte first).
 */
struct SCSI_Sense_Data {				/* Request Sense result			*/
	unsigned char		errorCode;		/*  0	Class code, valid lbn	*/
	unsigned char		segmentNumber;	/*  1	Segment number			*/
	unsigned char		senseKey;		/*  2	Sense key and flags		*/
	unsigned char		info[4];
	unsigned char		additionalSenseLength;
	unsigned char		reservedForCopy[4];
	unsigned char		additionalSenseCode;
	unsigned char		additionalSenseQualifier;	
	unsigned char		fruCode;		/* Field replacable unit code	*/
	unsigned char		senseKeySpecific[2];
	unsigned char		additional[101];
};
typedef struct SCSI_Sense_Data SCSI_Sense_Data;
/*
 * The high-bit of errorCode signals whether there is a logical
 * block. The low value signals whether there is a valid sense
 */
#define kScsiSenseHasLBN			0x80	/* Logical block number set	*/
#define kScsiSenseInfoValid			0x70	/* Is sense key valid?		*/
#define kScsiSenseInfoMask			0x70	/* Mask for sense info		*/
/*
 * These bits may be set in the sense key
 */
#define kScsiSenseKeyMask			0x0F
#define kScsiSenseILI				0x20	/* Illegal logical Length	*/
#define kScsiSenseEOM				0x40	/* End of media				*/
#define kScsiSenseFileMark			0x80	/* End of file mark			*/

/*
 * SCSI sense codes. (Returned after request sense).
 */
#define	 kScsiSenseNone				0x00	/* No error					*/
#define	 kScsiSenseRecoveredErr		0x01	/* Warning					*/
#define	 kScsiSenseNotReady			0x02	/* Device not ready			*/
#define	 kScsiSenseMediumErr		0x03	/* Device medium error		*/
#define	 kScsiSenseHardwareErr		0x04	/* Device hardware error	*/
#define	 kScsiSenseIllegalReq		0x05	/* Illegal request for dev.	*/
#define	 kScsiSenseUnitAtn			0x06	/* Unit attention (not err)	*/
#define	 kScsiSenseDataProtect		0x07	/* Data protection			*/
#define	 kScsiSenseBlankCheck		0x08	/* Tape-specific error		*/
#define	 kScsiSenseVendorSpecific	0x09	/* Vendor-specific error	*/
#define	 kScsiSenseCopyAborted		0x0a	/* Copy request cancelled	*/
#define	 kScsiSenseAbortedCmd		0x0b	/* Initiator aborted cmd.	*/
#define	 kScsiSenseEqual			0x0c	/* Comparison equal			*/
#define	 kScsiSenseVolumeOverflow	0x0d	/* Write past end mark		*/
#define	 kScsiSenseMiscompare		0x0e	/* Comparison failed		*/
#define	 kScsiSenseCurrentErr		0x70
#define	 kScsiSenseDeferredErr		0x71

/*
 * Mode sense parameter header
 */
struct SCSI_ModeParamHeader {
	unsigned char		modeDataLength;
	unsigned char		mediumType;
	unsigned char		deviceSpecific;
	unsigned char		blockDescriptorLength;
};
typedef struct SCSI_ModeParamHeader SCSI_ModeParamHeader;

struct SCSI_ModeParamBlockDescriptor {
	unsigned char		densityCode;
	unsigned char		numberOfBlocks[3];
	unsigned char		reserved;
	unsigned char		blockLength[3];
};
typedef struct SCSI_ModeParamBlockDescriptor SCSI_ModeParamBlockDescriptor;

union SCSI_ModeParamPage {
	unsigned char		data[1];
	struct {
		unsigned char	code;
		unsigned char	length;
	} page;
};
typedef union SCSI_ModeParamPage SCSI_ModeParamPage;

/*
 * LogSense parameter header
 */
struct SCSI_LogSenseParamHeader {
	unsigned char		pageCode;
	unsigned char		reserved;
	unsigned char		pageLength[2];
};
typedef struct SCSI_LogSenseParamHeader SCSI_LogSenseParamHeader;

/*
 * Log parameter pages are variable-length with a fixed length header.
 */
union SCSI_LogSenseParamPage {
	unsigned char		data[1];
	struct {
		unsigned char	parameterCode[2];
		unsigned char	flags;
		unsigned char	parameterLength;
	} page;
};
typedef union SCSI_LogSenseParamPage SCSI_LogSenseParamPage;

/*
 * SCSI command status (from status phase)
 */
#define	 kScsiStatusGood			0x00	/* Normal completion		*/
#define	 kScsiStatusCheckCondition	0x02	/* Need GetExtendedStatus	*/
#define	 kScsiStatusConditionMet	0x04
#define	 kScsiStatusBusy			0x08	/* Device busy (self-test?)	*/
#define	 kScsiStatusIntermediate	0x10	/* Intermediate status		*/
#define	 kScsiStatusResConflict		0x18	/* Reservation conflict		*/
#define	 kScsiStatusQueueFull		0x28	/* Target can't do command	*/
#define	 kScsiStatusReservedMask	0x3e	/* Vendor specific?			*/

/*
 * SCSI command codes. Commands defined as ...6, ...10, ...12, are
 * six-byte, ten-byte, and twelve-byte variants of the indicated command.
 */
/*
 * These commands are supported for all devices.
 */
#define kScsiCmdChangeDefinition	0x40
#define kScsiCmdCompare				0x39
#define kScsiCmdCopy				0x18
#define kScsiCmdCopyAndVerify		0x3a
#define kScsiCmdInquiry				0x12
#define kScsiCmdLogSelect			0x4c
#define kScsiCmdLogSense			0x4d
#define kScsiCmdModeSelect12		0x55
#define kScsiCmdModeSelect6			0x15
#define kScsiCmdModeSense12			0x5a
#define kScsiCmdModeSense6			0x1a
#define kScsiCmdReadBuffer			0x3c
#define kScsiCmdRecvDiagResult		0x1c
#define kScsiCmdRequestSense		0x03
#define kScsiCmdSendDiagnostic		0x1d
#define kScsiCmdTestUnitReady		0x00
#define kScsiCmdWriteBuffer			0x3b

/*
 * These commands are supported by direct-access devices only.
 */
#define kScsiCmdFormatUnit			0x04
#define kSCSICmdCopy				0x18
#define kSCSICmdCopyAndVerify		0x3a
#define kScsiCmdLockUnlockCache		0x36
#define kScsiCmdPrefetch			0x34
#define kScsiCmdPreventAllowRemoval	0x1e
#define kScsiCmdRead6				0x08
#define kScsiCmdRead10				0x28
#define kScsiCmdReadCapacity		0x25
#define kScsiCmdReadDefectData		0x37
#define kScsiCmdReadLong			0x3e
#define kScsiCmdReassignBlocks		0x07
#define kScsiCmdRelease				0x17
#define kScsiCmdReserve				0x16
#define kScsiCmdRezeroUnit			0x01
#define kScsiCmdSearchDataEql		0x31
#define kScsiCmdSearchDataHigh		0x30
#define kScsiCmdSearchDataLow		0x32
#define kScsiCmdSeek6				0x0b
#define kScsiCmdSeek10				0x2b
#define kScsiCmdSetLimits			0x33
#define kScsiCmdStartStopUnit		0x1b
#define kScsiCmdSynchronizeCache	0x35
#define kScsiCmdVerify				0x2f
#define kScsiCmdWrite6				0x0a
#define kScsiCmdWrite10				0x2a
#define kScsiCmdWriteAndVerify		0x2e
#define kScsiCmdWriteLong			0x3f
#define kScsiCmdWriteSame			0x41

/*
 * These commands are supported by sequential devices.
 */
#define kScsiCmdRewind				0x01
#define kScsiCmdWriteFilemarks		0x10
#define kScsiCmdSpace				0x11
#define kScsiCmdLoadUnload			0x1B
/*
 * ANSI SCSI-II for CD-ROM devices.
 */
#define kScsiCmdReadCDTableOfContents	0x43

/*
 * Message codes (for Msg In and Msg Out phases). The Macintosh
 * SCSI Manager can't really deal with these.
 */
#define kScsiMsgAbort				0x06
#define kScsiMsgAbortTag			0x0d
#define kScsiMsgBusDeviceReset		0x0c
#define kScsiMsgClearQueue			0x0e
#define kScsiMsgCmdComplete			0x00
#define kScsiMsgDisconnect			0x04
#define kScsiMsgIdentify			0x80
#define kScsiMsgIgnoreWideResdue	0x23
#define kScsiMsgInitiateRecovery	0x0f
#define kScsiMsgInitiatorDetectedErr 0x05
#define kScsiMsgLinkedCmdComplete	0x0a
#define kScsiMsgLinkedCmdCompleteFlag 0x0b
#define kScsiMsgParityErr			0x09
#define kScsiMsgRejectMsg			0x07
#define kScsiMsgModifyDataPtr		0x00 /* Extended msg		*/
#define kScsiMsgNop					0x08
#define kScsiMsgHeadOfQueueTag		0x21 /* Two byte msg		*/
#define kScsiMsgOrderedQueueTag		0x22 /* Two byte msg		*/
#define kScsiMsgSimpleQueueTag		0x20 /* Two byte msg		*/
#define kScsiMsgReleaseRecovery		0x10
#define kScsiMsgRestorePointers		0x03
#define kScsiMsgSaveDataPointers	0x02
#define kScsiMsgSyncXferReq			0x01 /* Extended msg		*/
#define kScsiMsgWideDataXferReq		0x03 /* Extended msg		*/
#define kScsiMsgTerminateIOP		0x11
#define kScsiMsgExtended			0x01

#define kScsiMsgTwoByte				0x20
#define kScsiMsgTwoByteMin			0x20
#define kScsiMsgTwoByteMax			0x2f

/*
 * Default timeout times for SCSI commands.
 */
#define kScsiNormalCompletionTime	(30L)			/* 1/2 second				*/
/*
 * Dratted DAT tape.
 */
#define kScsiDATCompletionTime		(60L * 60L);	/* One minute				*/
/*
 * Yes, we do allow 90 seconds for spin-up of those dratted tape drives.
 */
#define kScsiSpinUpCompletionTime	(60L * 90L)

/*
 * The NCR Bits, as returned by ScsiStat are only useful for maintenence
 * and testing. Only the following bits are valid in the current
 * implementation of the SCSI Manager. There is no guarantee that the
 * bits are accessable, or useful, in future SCSI implementations.
 * Note, however, that the Asynchronous SCSI Manager sets these bits to
 * correspond to its current internal state.
 *
 * Using these bits, the following implications can be drawn:
 *	kScsiStatBSY		Bus is busy. (On systems with multiple busses,
 *						there is no indication which bus is busy.)
 *	kScsiStatREQ		Bus is busy. There is no way to determine whether
 *						the target has changed phase or has set REQ.
 *	Bus Phase			If kScsiStatREQ and kSCSIStatBSY are set, the
 *						phase bits will indicate the current bus phase
 *						from the point of view of the initiator. It may
 *						not necessarily correspond exactly to the hardware
 *						bus phase.
 */
#define	kScsiStatBSY			(1 << 6)	/* Bus Busy					*/
#define	kScsiStatREQ			(1 << 5)	/* Set if Bus Busy			*/
#define	kScsiStatMSG			(1 << 4)	/* MSG phase bit			*/
#define	kScsiStatCD				(1 << 3)	/* C/D phase bit			*/
#define	kScsiStatIO				(1 << 2)	/* I/O phase bit			*/
#define kScsiStatSEL			(1 << 1)	/* Select phase bit			*/
#define	kScsiPhaseMask	(kScsiStatMSG | kScsiStatCD | kScsiStatIO)
#define kScsiPhaseShift	(2)
#define	ScsiBusPhase(x) (((x) & kScsiPhaseMask) >> kScsiPhaseShift)

/*
 * The phases are defined by a combination of bus lines. Note: these values
 * have already been shifted. Other values are undefined. This is really
 * only useful for the original SCSI manager.
 */
#define kScsiPhaseDATO		0	/* Data output (host -> device)			*/
#define kScsiPhaseDATI		1	/* Data input  (device -> host)			*/
#define kScsiPhaseCMD		2	/* Command								*/
#define kScsiPhaseSTS		3	/* Status								*/
#define kScsiPhaseMSGO		6	/* Message output						*/
#define kScsiPhaseMSGI		7	/* Message input						*/

#endif /* __MacSCSICommand__ */
