/*								SCSISimpleSampleDisplay.c						*/
/*
 * This module displays the results from all tests. Note that it is
 * intentionally pretty crude so the entire display is described in this file;
 * a "real" program would move strings to resources and, perhaps, more clearly
 * explain the results.
 */
#include "SCSISimpleSample.h"

typedef struct {
	unsigned char	senseKey;
	StringPtr		senseText;
} SenseKeyText;

/*
 * The sense keys.
 */
static StringPtr		gSenseKeyText[] = {
	"\pNo error",
	"\pRecovered error",
	"\pNot ready",
	"\pMedium error",
	
	"\pHardware error",
	"\pIllegal request",
	"\pUnit attention",
	"\pData protection",

	"\pBlank check",	
	"\pVendor specific",
	"\pCopy aborted",
	"\pCommand aborted",

	"\pCompare equal",	
	"\pVolume overflow",
	"\pMiscompare",
	"\pReserved"
};

StringPtr		gScsiDeviceType[] = {
	"\pDisk",
	"\pTape",
	"\pPrinter",
	"\pProc.",
	"\pWORM",
	"\pCD-ROM",
	"\pScanner",
	"\pOptical",
	"\pChanger",
	"\pCommunications",
	"\pGraphics Arts 0A",
	"\pGraphics Arts 0B",
};

StringPtr		gScsiState[] = {
	"\pNot Ready",
	"\pBecoming Ready",
	"\pUnit Attention",
	"\pBusy",
	"\pReady",
	"\pFailure"
};

typedef struct CmdInfo {
	unsigned short				cmdByte;
	StringPtr					text;
} CmdInfo, *CmdInfoPtr;
static CmdInfo				gCmdInfo[] = {
	{ kScsiCmdInquiry,				"\pInquiry"						},
	{ kScsiCmdLogSelect,			"\pLog Select"					},
	{ kScsiCmdLogSense,				"\pLog Sense"					},
	{ kScsiCmdModeSelect12,			"\pMode Select [12]"			},
	{ kScsiCmdModeSelect6,			"\pMode Select [6]"				},
	{ kScsiCmdModeSense12,			"\pMode Sense [6]"				},
	{ kScsiCmdRequestSense,			"\pRequest Sense"				},
	{ kScsiCmdTestUnitReady,		"\pTest Unit Ready"				},
	{ kScsiCmdPreventAllowRemoval,	"\pPrevent or Allow Removal"	},
	{ kScsiCmdRead6,				"\pRead [6]"					},
	{ kScsiCmdRead10,				"\pRead [10]"					},
	{ kScsiCmdReadCapacity,			"\pRead Device Capacity"		},
	{ kScsiCmdReadDefectData,		"\pRead Defect Data"			},
	{ kScsiCmdReassignBlocks,		"\pReassign Blocks"				},
	{ kScsiCmdStartStopUnit,		"\pStart or Stop Unit"			},
	{ 0xFF,							NULL							}
};


/*
 * ShowSCSIBusID is called before sending a command to the indicated device.
 */
void
ShowSCSIBusID(
		DeviceIdent				scsiDevice,				/* -> Bus/target/LUN	*/
		ConstStr255Param		commandText
	)
{
		Str255				work;
		
		pstrcpy(work, "\pBus ID: ");
		AppendDeviceID(work, scsiDevice);
		if (commandText != NULL) {
			pstrcat(work, "\p, ");
			pstrcat(work, commandText);
		}
		LOG(work);
}

/*
 * Display the results of an extended sense request.
 * Format the fields we understand.
 */
void
ShowRequestSense(
		register ScsiCmdBlockPtr	scsiCmdBlockPtr
	)
{
#define SCB		(*scsiCmdBlockPtr)
		DoShowRequestSense(
			SCB.scsiDevice,				/* For this device							*/
			SCB.status,					/* The original error from the SCSI Manager	*/
			SCB.requestSenseStatus,		/* The error, if any, from Request Sense	*/
			&SCB.command,				/* The command that failed					*/
			&SCB.sense					/* -> the sense buffer						*/
		);
#undef SCB
}

/*
 * Actually display the results of a request sense. This is independent of the SCB
 */
void
DoShowRequestSense(
		DeviceIdent				scsiDevice,
		OSErr					operationStatus,
		OSErr					requestSenseStatus,
		const SCSI_CommandPtr	scsiCommand,
		const SCSI_Sense_Data	*sensePtr
	)
{
		register int				i;
		Str255						work;
		Str255						senseMessage;
#define SENSE		(*sensePtr)

		if (operationStatus != noErr) {
			ShowSCSIBusID(scsiDevice, "\pOperation failed");
			DoShowSCSICommand(scsiCommand, NULL);
		}
		if (operationStatus != noErr && operationStatus != statusErr) {
			/*
			 * The operation failed, and the failure was severe enough that
			 * the device didn't return Check Condition. Perhaps there is
			 * no device at this bus/target.
			 */
			ShowStatusError(scsiDevice, operationStatus, scsiCommand);
		}
		if (operationStatus == statusErr && requestSenseStatus == noErr) {
			/*
			 * The operation failed, the device returned Check Condition, and
			 * we were able to read the Request Sense information.
			 */
			pstrcpy(work, "\pSense: ");
			if ((SENSE.errorCode & kScsiSenseInfoMask) != kScsiSenseInfoValid) {
				AppendPascalString(work, "\p(invalid sense code)");
				AppendHexLeadingZeros(work, SENSE.errorCode, 2);
				LOG(work);
			}
			else {
				AppendPascalString(
					work, gSenseKeyText[SENSE.senseKey & kScsiSenseKeyMask]);
				if ((SENSE.senseKey & kScsiSenseILI) != 0)
					AppendPascalString(work, "\p, Illegal Logical Length");
				if ((SENSE.senseKey & kScsiSenseEOM) != 0)
					AppendPascalString(work, "\p, End of Media");
				if ((SENSE.senseKey & kScsiSenseFileMark) != 0)
					AppendPascalString(work, "\p, File Mark");
				LOG(work);
				work[0] = 0;
				AppendPascalString(work, "\p ASC: ");
				AppendHexLeadingZeros(work, SENSE.additionalSenseCode, 2);
				AppendPascalString(work, "\p, ASQ: ");
				AppendHexLeadingZeros(work, SENSE.additionalSenseQualifier, 2);
				for (i = 1;; ++i) {
					GetIndString(
						senseMessage, SENSE.additionalSenseCode + STRS_SenseBase, i);
					if (senseMessage[0] == 0) {
						/*
						 * We don't know the sense qualifier, use the default
						 * additional sense code message, if any.
						 */
						GetIndString(
							senseMessage,
							SENSE.additionalSenseCode + STRS_SenseBase,
							1
						);
						if (senseMessage[0] != 0)
							goto displaySense;
						break;
					}
					if (senseMessage[1] == SENSE.additionalSenseQualifier) {
displaySense:			senseMessage[1] = senseMessage[0] - 1;
						AppendPascalString(work, "\p, ");
						AppendPascalString(work, (StringPtr) &senseMessage[1]);
						break;
					}
				}
				LOG(work);
			}
		}
#undef SENSE
}

/*
 * This formats and displays the SCSI Manager OSErr code (with out extensions)
 */
void
ShowStatusError(
		DeviceIdent				scsiDevice,
		OSErr					errorStatus,
		const SCSI_CommandPtr	scsiCommand
	)
{
		Str255					work;
		StringHandle			errorHdl;
		
		if (errorStatus != noErr || gVerboseDisplay) {
			pstrcpy(work, "\pDevice: ");
			AppendDeviceID(work, scsiDevice);
			AppendPascalString(work, "\p, ");
			switch (errorStatus) {
			case noErr:				AppendPascalString(work, "\pSuccessful");		break;
			case scsiNonZeroStatus:	/* Continue */
			case statusErr:			AppendPascalString(work, "\pCheck Condition");	break;
			case controlErr:		AppendPascalString(work, "\pBusy");				break;
			case scBadParmsErr:		AppendPascalString(work, "\pParam Error");		break;
			case scCommErr:			AppendPascalString(work, "\pNo Device");		break;
			case scPhaseErr:		AppendPascalString(work, "\pPhase Error");		break;
			case scComplPhaseErr:	AppendPascalString(work, "\pBad Phase");		break;
			default:
				AppendPascalString(work, "\pSystem Error ");
				AppendSigned(work, errorStatus);
				errorHdl = (StringHandle) GetResource('Estr', errorStatus);
				if (errorHdl != NULL) {
					AppendPascalString(work, "\p, \"");
					HLock((Handle) errorHdl);
					AppendPascalString(work, *errorHdl);
					HUnlock((Handle) errorHdl);
					ReleaseResource((Handle) errorHdl);
					AppendPascalString(work, "\p\"");
				}
				break;
			}
			LOG(work);
			DoShowSCSICommand(scsiCommand, NULL);
		}
}

void
DoShowInquiry(
		DeviceIdent				scsiDevice,				/* -> Bus/target/LUN	*/
		const SCSI_Inquiry_Data	*inquiry
	)
{
		unsigned short				i;
		Str255						work;
#define INQUIRY		(*inquiry)

		work[0] = 0;
		AppendDeviceID(work, scsiDevice);
		i = INQUIRY.devType;
		if (i == kScsiDevTypeMissing)
			AppendPascalString(work, "\p: Missing device or logical unit");
		else {
			if (i >= kScsiDevTypeFirstReserved) {
				AppendPascalString(work, "\p: Reserved or unknown device type (");
				AppendHexLeadingZeros(work, i, 2);
				AppendPascalString(work, "\p)");
			}
			else {
				AppendPascalString(work, "\p: \"");
				AppendPascalString(work, gScsiDeviceType[i]);
				AppendPascalString(work, "\p\"");
			}
			AppendPascalString(work, "\p, \"");
			AppendBytes(work, (const Ptr) INQUIRY.vendor, sizeof INQUIRY.vendor);
			AppendPascalString(work, "\p\", \"");
			AppendBytes(work, (const Ptr) INQUIRY.product, sizeof INQUIRY.product);
			AppendPascalString(work, "\p\", \"");
			AppendBytes(work, (const Ptr) INQUIRY.revision, sizeof INQUIRY.revision);
			AppendPascalString(work, "\p\"");
		}
		LOG(work);
#undef INQUIRY
}

void
DisplaySCSIErrorMessage(
		OSErr					errorStatus,
		ConstStr255Param		errorText
	)
{
		Str255					work;
		Handle					errorMsgHdl;
		
		if (errorStatus != noErr) {
			pstrcpy(work, "\pSCSI Error (");
			AppendSigned(work, errorStatus);
			AppendPascalString(work, "\p) at ");
			AppendPascalString(work, (const StringPtr) errorText);
			AppendChar(work, '.');
			errorMsgHdl = GetResource('Estr', errorStatus);
			if (errorMsgHdl != NULL) {
				AppendPascalString(work, "\p \"");
				AppendPascalString(work, (StringPtr) *errorMsgHdl);
				AppendPascalString(work, "\p\"");
				ReleaseResource(errorMsgHdl);
			}
			LOG(work);
		}
}

void
DoShowSCSICommand(
		const SCSI_CommandPtr	scsiCommand,
		ConstStr255Param		message
	)
{
		register CmdInfoPtr		cmdInfoPtr;
		register unsigned short	i;
		unsigned short			cmdBlockLength;		/* -> Length of CDB			*/
		Str255					work;
		
		work[0] = 0;
		if (message != NULL && message[0] != 0) {
			AppendPascalString(work, (StringPtr) message);
			AppendPascalString(work, "\p, ");
		}
		pstrcpy(work, "\pCommand ");
		for (cmdInfoPtr = gCmdInfo; cmdInfoPtr->text != NULL; cmdInfoPtr++) {
			if (cmdInfoPtr->cmdByte == scsiCommand->scsi[0]) {
				AppendPascalString(work, "\p (");
				AppendPascalString(work, cmdInfoPtr->text);
				AppendPascalString(work, "\p)");
				break;
			}
		}
		AppendPascalString(work, "\p =");
		cmdBlockLength = SCSIGetCommandLength((Ptr) scsiCommand);
		if (cmdBlockLength == 0)
			cmdBlockLength = sizeof (SCSI_Command);
		for (i = 0; i < cmdBlockLength; i++) {
			AppendChar(work, ' ');
			AppendHexLeadingZeros(work, (unsigned long) scsiCommand->scsi[i], 2);
		}
		LOG(work);
}


void
AppendDeviceID(
		StringPtr				result,
		DeviceIdent				scsiDevice
	)
{
		AppendUnsigned(result, scsiDevice.bus);
		AppendChar(result, '.');
		AppendUnsigned(result, scsiDevice.targetID);
		AppendChar(result, '.');
		AppendUnsigned(result, scsiDevice.LUN);
}

