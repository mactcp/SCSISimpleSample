/*							DoScsiCommandWithSense.c							*/
/*
 * DoScsiCommandWithSense.c
 * Copyright � 1992-93 Apple Computer Inc. All Rights Reserved.
 *
 * This is the common entry to the original and asynchronous SCSI Manager calls:
 * if the asynchronous SCSI Manager is present, it calls it. If not present, it
 * calls the original SCSI Manager and executes Request Sense if necessary.
 */
#include "SCSISimpleSample.h"

void							IssueRequestSense(
		register ScsiCmdBlockPtr	scsiCmdBlockPtr
	);


/*
 * Do one SCSI Command. If the device returns Check Condition, issue Request Sense
 * (original SCSI Manager only) and interpret the sense data. The original SCSI
 * command status is in SCB.status. If it is statusErr or scsiNonZeroStatus,
 * the sense data is in SCB.sense and the Request Sense status is in
 * SCB.requestSenseStatus.
 */
void
DoSCSICommandWithSense(
		register ScsiCmdBlockPtr	scsiCmdBlockPtr,
		Boolean					displayError,
		Boolean					enableAsynchSCSI
	)
{
		unsigned short			cmdBlockLength;
		unsigned short			scsiHandshake[handshakeDataLength];
		
#define SCB	(*scsiCmdBlockPtr)
		
		/*
		 * Store the LUN information in the command block - this is needed
		 * for devices that only examine the command block for LUN values.
		 * (On SCSI-II, the asynchronous SCSI Manager also includes the
		 * LUN in the identify message).
		 */
		SCB.command.scsi[1] &= ~0xE0;
		SCB.command.scsi[1] |= (SCB.scsiDevice.LUN & 0x03) << 5;
		cmdBlockLength = SCSIGetCommandLength((Ptr) &SCB.command);
		/*
		 * Try to call SCSI Manager 4.3, if it fails with unimpErr, call
		 * the old SCSI Manager. Note: AsyncSCSI [in this instance]
		 * is synchronous. Real-world applications would use an asynchronous
		 * variant.
		 */
		if (enableAsynchSCSI == FALSE || gEnableNewSCSIManager == FALSE)
			SCB.status = unimpErr;					/* Always original SCSI	*/
		else {
			/*
			 * If SCB.transferQuantum equals one, request a polled transfer.
			 * Otherwise, setup a simple "single block" handshake field.
			 */
			if (SCB.transferQuantum != 1) {
				CLEAR(scsiHandshake);
				scsiHandshake[0] = SCB.transferQuantum;
			}
			SCB.status = AsyncSCSI(
						SCB.scsiDevice,				/* Bus/target/LUN		*/
						&SCB.command,				/* The command			*/
						cmdBlockLength,				/* Command length		*/
						SCB.writeToDevice,			/* TRUE if writing		*/
						SCB.bufferPtr,				/* Data buffer, if any	*/
						SCB.transferSize,			/* Data transfer length	*/
						(SCB.transferQuantum == 1) ? NULL : scsiHandshake,
						&SCB.sense,					/* For sense result		*/
						sizeof SCB.sense,			/* Sense buffer size	*/
						kScsiSpinUpCompletionTime,	/* Watchdog timeout		*/
						&SCB.statusByte,			/* Gets STS Phase byte	*/
						&SCB.actualTransferCount	/* Bytes actually done	*/
					);
		}
		if (SCB.status != unimpErr) {
			/*
			 * The asynchronous SCSI Manager did something interesting.
			 */
			switch (SCB.status) {
			case noErr:
				break;
			case scsiDeviceNotThere:
			case scsiSelectTimeout:
			case scsiBusInvalid:
			case scsiTIDInvalid:
				/* These all mean "no such device" */
				break;
			case statusErr:
				/*
				 * Hmm. If the error is statusErr, the device returned
				 * "Check Condition" and the SCSI Manager successfully
				 * issued a Request Sense. We'll display the result of the
				 * sense and update the spinUpState.
				 */
				SCB.requestSenseStatus = noErr;
				if (displayError)
					ShowRequestSense(scsiCmdBlockPtr);
				break;
			default:
				if (displayError)
					ShowStatusError(SCB.scsiDevice, SCB.status, &SCB.command);
				break;
			}
		}
		else {
			/*
			 * Call the original SCSI Manager.
			 */
			SCB.status = OriginalSCSI(
						SCB.scsiDevice.targetID,
						&SCB.command,
						cmdBlockLength,
						SCB.writeToDevice,
						SCB.bufferPtr,
						SCB.transferSize,
						SCB.transferQuantum,
						kScsiSpinUpCompletionTime,
						&SCB.statusByte,
						&SCB.actualTransferCount
					);
			if (SCB.status != noErr) {
				if (displayError)
					ShowStatusError(SCB.scsiDevice, SCB.status, &SCB.command);
				if (SCB.status == statusErr) {
					/*
					 * Device returned "Check Condition."
					 */
					IssueRequestSense(scsiCmdBlockPtr);
					if (displayError)
						ShowRequestSense(scsiCmdBlockPtr);
				}
			}
		}
}

/*
 * Execute "Get Extended Sense." This should not fail. This is only called by
 * "old" SCSI Manager calls: the new SCSI Manager always enables "autosense."
 */
void
IssueRequestSense(
		register ScsiCmdBlockPtr	scsiCmdBlockPtr
	)
{
		SCSI_6_Byte_Command		requestSense;
		unsigned short			statusByte;				/* <- From Status Phase	*/
		unsigned long			actualTransferCount;
		
		CLEAR(requestSense);
		requestSense.opcode = kScsiCmdRequestSense;
		requestSense.len = sizeof SCB.sense;
		/*
		 * Stuff the logical unit number into the command block.
		 */
		requestSense.lbn3 &= 0xE0;
		requestSense.lbn3 |= (SCB.scsiDevice.targetID & 0x03) << 5;
		SCB.requestSenseStatus = OriginalSCSI(
					SCB.scsiDevice.targetID,
					(SCSI_CommandPtr) &requestSense,
					sizeof requestSense,
					FALSE,								/* No write				*/
					(Ptr) &SCB.sense,
					sizeof SCB.sense,
					1,
					kScsiSpinUpCompletionTime,
					&statusByte,
					&actualTransferCount
				);
}

