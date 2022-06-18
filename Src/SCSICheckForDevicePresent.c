/*							SCSICheckForDevicePreseent.c						*/
/*
 * SCSICheckForDevicePreseent.c
 * Copyright � 1992-94 Apple Computer Inc. All Rights Reserved.
 *
 * Return TRUE if the indicated device is present on this system. This function
 * only logs unexpected errors..
 */
#include "SCSISimpleSample.h"

Boolean
SCSICheckForDevicePresent(
		DeviceIdent				scsiDevice,
		Boolean					enableAsynchSCSI
	)
{
		Boolean						result;
		ScsiCmdBlock				scsiCmdBlock;
		SCSI_Inquiry_Data			inquiry;
#define SCB		(scsiCmdBlock)
#define SENSE	(SCB.sense)
		
		if (gVerboseDisplay)
			ShowSCSIBusID(scsiDevice, "\pChecking for device presence");
		CLEAR(SCB);
		SCB.scsiDevice = scsiDevice;
		SCB.command.scsi6.opcode = kScsiCmdInquiry;
		SCB.command.scsi6.len = sizeof inquiry;
		SCB.bufferPtr = (Ptr) &inquiry;
		SCB.transferSize = sizeof inquiry;
		SCB.transferQuantum = 1;						/* Force handshake		*/
		/* All other command bytes are zero */
		DoSCSICommandWithSense(&scsiCmdBlock, FALSE, enableAsynchSCSI);
		switch (SCB.status) {
		case noErr:
			if (inquiry.devType == kScsiDevTypeMissing) {
				VERBOSE("\pNo such device");
				result = FALSE;
			}
			else {
				VERBOSE("\pDevice is present");
				result = TRUE;
			}
			break;
		case statusErr:
			/*
			 * The target returned Check Condition. We need to look at the sense
			 * data, if any, to distinguish between an "offline" but present device,
			 * and a non-existant logical unit. Note: some drives return Check
			 * Condition, and "no sense error" if we try to access an incorrect
			 * logical unit. This might reasonably be remapped as "illegal request.
			 */
			if (SCB.requestSenseStatus == noErr
			 && (SENSE.errorCode & kScsiSenseInfoMask) != kScsiSenseInfoValid)
			 	result = 0;
			 else {
				switch (SENSE.senseKey) {
				case kScsiSenseIllegalReq:
					result = FALSE;
					break;
				default:
					ShowRequestSense(&scsiCmdBlock);
					result = TRUE;
					break;
				}
			}
			break;	
		default:								/* Strange error		*/
			ShowRequestSense(&scsiCmdBlock);
			/* Fall through */
		case scsiDeviceNotThere:
		case scsiSelectTimeout:
		case scsiBusInvalid:
		case scsiTIDInvalid:
		case scCommErr:							/* No such device		*/
		case scsiIdentifyMessageRejected:
			result = FALSE;
			break;
		}
		return (result);
#undef SCB
}

