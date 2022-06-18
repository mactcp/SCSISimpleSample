/*								SCSIBusAPI.c							*/
/*
 * SCSIBusAPI.c
 * Copyright � 1992-94 Apple Computer Inc. All Rights Reserved.
 */
#include "SCSISimpleSample.h"

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
	)
{
		OSErr						status;
		SCSIBusInquiryPB			busInquiryPB;
#define PB							(busInquiryPB)

		if (AsyncSCSIPresent() == FALSE) {
			*useAsynchManager = FALSE;
			status = noErr;
		}
		else {
			CLEAR(PB);
			PB.scsiPBLength = sizeof PB;
			PB.scsiFunctionCode = SCSIBusInquiry;
			PB.scsiDevice = scsiDevice;
			status = SCSIAction((SCSI_PB *) &PB);
			if (status == noErr)
				*useAsynchManager = TRUE;
			else if (scsiDevice.bus == 0) {
				/*
				 * If bus zero is not registered, it must be accessed
				 * via the original API.
				 */
				status = noErr;
				*useAsynchManager = FALSE;
			}
			else {
				/*
				 * This is a problem: this bus cannot be accessed unless it has
				 * been registered. For example, the second bus of a Quadra
				 * 950 cannot be accessed if the SCSI Manager extension is not
				 * isntalled and a third-party asynchronous SCSI Manager card
				 * is installed as bus 2. Return the error to skip this bus.
				 */
			}
		}
		return (status);
#undef PB
}

