/*							SCSIGetHighHostBusAdaptor.c							*/
/*
 * GetHighHostBusAdaptor.c
 * Copyright � 1992-94 Apple Computer Inc. All Rights Reserved.
 */
#include "SCSISimpleSample.h"
/*
 * Get the last host bus adaptor. Returns zero (and noErr) for Old SCSI.
 */
OSErr
SCSIGetHighHostBusAdaptor(
		unsigned short					*lastHostBus
	)
{
		OSErr							status;
		SCSIBusInquiryPB				busInquiryPB;
#define PB								(busInquiryPB)

		if (AsyncSCSIPresent() == FALSE) {
			*lastHostBus = 0;
			status = noErr;
		}
		else {
			CLEAR(PB);
			PB.scsiPBLength = sizeof PB;
			PB.scsiFunctionCode = SCSIBusInquiry;
			PB.scsiDevice.bus = 0xFF;
			status = SCSIAction((SCSI_PB *) &PB);
			DisplaySCSIErrorMessage(status, "\pSCSIBusInquiry failed");
			*lastHostBus = PB.scsiHiBusID;
		}
		return (status);
#undef PB
}

