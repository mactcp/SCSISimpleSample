/*								SCSIGetInitiatorID.c							*/
/*
 * SCSIGetInitiatorID.c
 * Copyright � 1992-94 Apple Computer Inc. All Rights Reserved.
 */
#include "SCSISimpleSample.h"

/*
 * Get the SCSI bus ID of the Macintosh (initiator) on this bus (only
 * scsiDevice.bus is referenced).
 */
OSErr
SCSIGetInitiatorID(
		DeviceIdent				scsiDevice,
		unsigned short			*initiatorID
	)
{
		OSErr						status;
		SCSIBusInquiryPB			busInquiryPB;
#define PB							(busInquiryPB)

		if (AsyncSCSIPresent() == FALSE) {
			*initiatorID = 7;
			status = noErr;
		}
		else {
			CLEAR(PB);
			PB.scsiPBLength = sizeof PB;
			PB.scsiFunctionCode = SCSIBusInquiry;
			PB.scsiDevice = scsiDevice;
			status = SCSIAction((SCSI_PB *) &PB);
			DisplaySCSIErrorMessage(status, "\pSCSIBusInquiry failed");
			*initiatorID = PB.scsiInitiatorID;
		}
		return (status);
}

