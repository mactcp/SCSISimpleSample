/*								DoGetDriveInfo.c								*/
/*
 * DoGetDriveInfo.c
 * Copyright � 1992-94 Apple Computer Inc. All Rights Reserved.
 */
#include "SCSISimpleSample.h"

/*
 * Execute a Bus Inquiry SCSI request on the selected device. If it succeeds,
 * display the results.
 */
void
DoGetDriveInfo(
		DeviceIdent				scsiDevice,				/* -> Bus/target/LUN	*/
		Boolean					noIntroMsg,
		Boolean					useAsynchManager
	)
{

		ScsiCmdBlock				scsiCmdBlock;
		SCSI_Inquiry_Data			inquiry;
#define SCB	(scsiCmdBlock)
		
		if (noIntroMsg == FALSE)
			ShowSCSIBusID(scsiDevice, "\pGet Drive Info");
		CLEAR(SCB);
		SCB.scsiDevice = scsiDevice;
		SCB.command.scsi6.opcode = kScsiCmdInquiry;
		SCB.command.scsi6.len = sizeof inquiry;
		SCB.bufferPtr = (Ptr) &inquiry;
		SCB.transferSize = sizeof inquiry;
		SCB.transferQuantum = 1;						/* Force handshake		*/
		/* All other command bytes are zero */
		DoSCSICommandWithSense(&scsiCmdBlock, TRUE, useAsynchManager);
		if (SCB.status == noErr)
			DoShowInquiry(scsiDevice, &inquiry);
#undef SCB
}

		
