/*								OriginalSCSI.c									*/
/*
 * OriginalSCSI.c
 * Copyright � 1992-93 Apple Computer Inc. All Rights Reserved.
 *
 * Talk to the Macintosh SCSI Handler using the "old" interface as defined in
 * Inside Mac IV. This is a synchronous call that executes a single SCSI Command
 * on a device. It does not support multiple busses or logical units.
 *
 * Calling Sequence:
 *		OSErr				OriginalSCSI(
 *				short					targetID,
 *				const SCSI_CommandPtr	scsiCommand,
 *				unsigned short			cmdBlockLength,
 *				Boolean					writeToDevice,
 *				Ptr						bufferPtr,
 *				unsigned long			transferSize,
 *				unsigned long			transferQuantum,
 *				unsigned long			completionTimeout,
 *				unsigned short			*stsBytePtr,
 *				unsigned long			*actualTransferCount
 *			);
 * The parameters have the following meaning:
 *
 *	targetID			The SCSI Bus ID of the target (0 .. 6). Note that this
 *						function can only access LUN zero.
 *	scsiCommand			The SCSI Command Block (6, 10, or 12 bytes). The command
 *						block length will be determined by examining the command
 *						parameter. This is standardized for all but
 *						"vendor-specific" commands.
 *	writeToDevice		TRUE if this command writes to the device. FALSE if this
 *						command reads from the device or does not require a data
 *						phase.
 *	bufferPtr			The user data buffer for Read/Write commands. It should be
 *						NULL if a data transfer phase is not used for this command.
 *						(e.g. for Test Unit Ready).
 *	transferSize		The total number of bytes to transfer to or from the device.
 *	transferQuantum		This is needed to configure the transfer information block
 *						(TIB). The following values are appropriate:
 *						-- Set to zero for a one-shot blind transfer.
 *							ActualTransferCount is not correctly returned.
 *						-- Set to one if a polled transfer is needed. This is
 *							useful for Request Sense or other management requests,
 *							especially requests with a variable-length record. On
 *							return, the actual number of bytes that were transferred
 *							will be in the actualTransferCount variable.
 *						-- Set to the transferSize value for a normal, "Blind"
 *							transfer. This is the normal case for data requests.
 *							ActualTransferCount will equal transferSize on success,
 *							but does not correctly return the actual count on phase
 *							errors. This, however, can be recovered from the Request
 *							Sense record.
 *						-- Set to a sub-multiple (such as a block length) for
 *							requests that need re-synchronization between sectors.
 *						-- Other values will likely result in errors.
 *	completionTimeout	The timeout (in Ticks) for the command. This should be short
 *						for disks, but must be long for tape devices and some setup
 *						requests, such as Mode Select.
 *	stsBytePtr			This short is set to the byte returned in the device's
 *						Status Phase.
 *	actualTransferCount	This will be set to the number of "cycles" through the TIB
 *						loop times the transferCount. This should equal the number
 *						of bytes transferred if transferCount is set to one.
 *						(Ignored if NULL.)
 * Return codes:
 *	noErr			normal
 *	scCommErr		no such device (selection error)
 *	scPhaseErr		user data buffer was the wrong size for the transfer.
 *					Look at the status byte to see if this is a problem: 
 *					you may merely have given a large buffer size to
 *					a variable-length request, such as Device Inquiry.
 *	sc...			other SCSI error.
 *	statusErr		Device returned "Check condition." The caller should
 *					issue a Request Sense SCSI Command to this device.
 *	controlErr		Device returned "Busy"
 *	ioErr			Other (serious) device status. The caller should
 *					examine the Status and Message bytes to determine
 *					the problem.
 *	paramErr		Could not determine the command length.
 */
#include <scsi.h>
#include <Errors.h>
#include <Gestalt.h>
#include <Memory.h>
#include <Events.h>
#include "MacSCSICommand.h"
#ifndef TRUE
#define FALSE		0
#define TRUE		1
#endif

/*
 * Execute a SCSI command using the original (Inside Mac IV) SCSI Manager.
 * Return codes:
 *
 *	noErr			normal
 *	paramErr		could not determine command length from command
 *	scCommErr		could not select this device or bus busy
 *	sc...			other scsi error
 *	statusErr		Device returned "Check condition"
 *	controlErr		Device returned "Busy" (Note: device error)
 *	ioErr			Other (serious) device status -- bug.
 */
OSErr						OriginalSCSI(
		short					targetID,			/* Device ID on this bus	*/
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
 * SCSI command status (from status phase)
 */
#define	 kScsiStatusGood			0x00			/* Normal completion		*/
#define	 kScsiStatusCheckCondition	0x02			/* Need GetExtendedStatus	*/
#define	 kScsiStatusConditionMet	0x04			/* For Compare Command?		*/
#define	 kScsiStatusBusy			0x08			/* Device busy (self-test?)	*/
#define	 kScsiStatusIntermediate	0x10			/* Intermediate status		*/
#define	 kScsiStatusResConflict		0x18			/* Reservation conflict		*/
#define	 kScsiStatusQueueFull		0x28			/* Target can't do command	*/
#define	 kScsiStatusReservedMask	0x3e			/* Vendor specific?			*/

/*
 * This is the maximum number of times we try to grab the SCSI Bus
 */
#define kMaxSCSIRetries				40				/* 10 seconds, 4 times/sec	*/
/*
 * This test is TRUE if the SCSI bus status indicates "busy" (which is the case
 * if either the BSY or SEL bit is set).
 */
#ifndef kScsiStatBSY
#define kScsiStatBSY				(1 << 6)
#endif
#ifndef kScsiStatSEL
#define kScsiStatSEL				(1 << 1)
#endif
#define ScsiBusBusy()		((SCSIStat() & (kScsiStatBSY | kScsiStatSEL)) != 0)

static void		NextFunction(void);		/* Dummy function for OriginalSCSI size	*/
static Boolean	IsVirtualMemoryRunning(void);
/*
 * These are bitmasks for the vmHoldMask variable. A bit is set if its associated
 * memory element has been held in protected (non-paged) memory.
 */
#define kHoldFunction			0x0001				/* AsyncSCSI function code	*/
#define kHoldStack				0x0002				/* Local variables			*/
#define kHoldUserBuffer			0x0004				/* User data buffer, if any	*/
#define kHoldCommandBlock		0x0008				/* SCSI Command Data Block	*/

/*
 * Execute a SCSI command.
 * Returns the final status as noted above.
 */
OSErr
OriginalSCSI(
		short					targetID,			/* SCSI device on this bus	*/
		const SCSI_CommandPtr	scsiCommand,		/* The actual scsi command	*/
		unsigned short			cmdBlockLength,		/* -> Length of CDB			*/
		Boolean					writeToDevice,		/* TRUE to write			*/
		Ptr						bufferPtr,			/* -> user data buffer		*/
		unsigned long			transferSize,		/* How much to transfer		*/
		unsigned long			transferQuantum,	/* TIB setup parameter		*/
		unsigned long			completionTimeout,	/* Ticks to wait			*/
		unsigned short			*stsBytePtr,		/* <- status phase byte		*/
		unsigned long			*actualTransferCount
	)
{
		OSErr					status;				/* Final status				*/
		OSErr					completionStatus;	/* Status from ScsiComplete	*/
		short					totalTries;			/* Get/Select retries		*/
		short					getTries;			/* Get retries				*/
		short					iCount;				/* Bus free counter			*/
		unsigned long			watchdog;			/* Timeout after this		*/
		unsigned long			myTransferCount;	/* Gets TIB loop counter	*/
		/*
		 * The TIB has the following format:
		 *	[0]	scInc	user buffer			transferQuantum or transferSize
		 *	[1] scAdd	&theTransferCount	1
		 *	[2] scLoop	-> tib[0]			transferSize / transferQuantum
		 *	[3] scStop
		 * The intent of this is to return, in actualTransferCount, the number
		 * of times we cycled through the tib[] loop. This will be the actual
		 * transfer count if transferQuantum equals one, or the number of
		 * "blocks" if transferQuantum is the length of one sector.
		 */
		SCSIInstr				tib[4];				/* Current TIB				*/
		short					messageByte;		/* For Command Complete 
		/*
		 * The following parameters are used to manage virtual memory. The code
		 * is taken from the DTS SCSI Sample Driver.
		 */
		unsigned short			vmHoldMask;
		unsigned long			vmFunctionSize;
		char					*vmProtectedStackBase;	/* Last local variable	*/
/*
 * These values are used to compute the size of the stack that we must hold in
 * protected (non-virtual) memory. kSCSIManagerStackEstimate is an estimate.
 */
#define kSCSILocalVariableSize	( \
		(unsigned long) (((Ptr) &status) - ((Ptr) &vmProtectedStackBase))	\
	)
#define kSCSIManagerStackEstimate 512
#define kSCSIProtectedStackSize (kSCSIManagerStackEstimate + kSCSILocalVariableSize)

		status = noErr;
		vmHoldMask = 0;
		/*
		 * If there is a data transfer, setup the tib.
		 */
		myTransferCount = 0;
		if (transferQuantum == 0)
			transferQuantum = transferSize;
		if (bufferPtr != NULL) {
			tib[0].scOpcode = scInc;
			tib[0].scParam1 = (unsigned long) bufferPtr;
			tib[0].scParam2 = transferQuantum;
			tib[1].scOpcode = scAdd;
			tib[1].scParam1 = (unsigned long) &myTransferCount;
			tib[1].scParam2 = transferQuantum;
			tib[2].scOpcode = scLoop;
			tib[2].scParam1 = (-2 * sizeof (SCSIInstr));
			tib[2].scParam2 = transferSize / tib[0].scParam2;
			tib[3].scOpcode = scStop;
			tib[3].scParam1 = 0;
			tib[3].scParam2 = 0;
		}
		if (IsVirtualMemoryRunning()) {

			/*
			 * Virtual memory is active. Lock all of the memory segments that we
			 * need in "real" memory (i.e. non-paged pool) for the duration of the
			 * call. Since  we need to back out of VM holds if there are errors,
			 * we'll use bits in vmHoldMask to record the status of our attempts.
			 *
			 * Note: in a real application or driver, the user buffers should be
			 * held outside of the SCSI Manager code:
			 *		HoldMemory(data buffer);
			 *		HoldMemory(autosense buffer);
			 *		status = CallSCSIManager(...);
			 *		UnholdMemory(...);
			 *
			 * First, hold the MacSCSI function. It starts at AsyncSCSI and
			 * extends to the start of the next function. This is marked by a
			 * dummy function. The left-margin comments indicate the value
			 * of vmHoldCount if the indicated HoldMemory succeeded. This is not
			 * needed for drivers.
			 */
			vmFunctionSize =
				(unsigned long) NextFunction - (unsigned long) OriginalSCSI;
			status = HoldMemory(OriginalSCSI, vmFunctionSize);
			if (status == noErr)
				vmHoldMask |= kHoldFunction;
			if (status == noErr) {
				/*
				 * Hold a chunk of stack space to call the SCSI Manager and to
				 * protect our local variables. This is always needed, as drivers
				 * can be called from application contexts.
				 */
				vmProtectedStackBase =
					(char *) &vmProtectedStackBase - kSCSIManagerStackEstimate;
				status = HoldMemory(vmProtectedStackBase, kSCSIProtectedStackSize);
				if (status == noErr)
					vmHoldMask |= kHoldStack;
			}
			if (status == noErr) {
				/*
				 * Lock down the command block. In this sample, we allocated
				 * the command block in the application heap. A driver would
				 * typically allocate it in the System Heap and, hence, would
				 * not require this call.
				 */
				status = HoldMemory((Ptr) scsiCommand, cmdBlockLength);
				if (status == noErr)
					vmHoldMask |= kHoldCommandBlock;
			}		
			if (status == noErr && bufferPtr != NULL) {
				/*
				 * Lock down the user buffer, if any. In a real-world application
				 * or driver, this would be done before calling the SCSI interface.
				 */
				status = HoldMemory(bufferPtr, transferSize);
				if (status != noErr)
					vmHoldMask |= kHoldStack;
			}
			if (status != noErr)
				goto exit;
		}
		/*
		 * Arbitrate for the scsi bus.  This will fail if some other device is
		 * accessing the bus at this time (which is unlikely).
		 *
		 *** Do not set breakpoints or call any functions that may require device
		 *** I/O (such as display code that accesses font resources between
		 *** SCSIGet and SCSIComplete,
		 *
		 */
		for (totalTries = 0; totalTries < kMaxSCSIRetries; totalTries++) {
			for (getTries = 0; getTries < 4; getTries++) {
				/*
				 * Wait for the bus to go free.
				 */
				watchdog = TickCount() + 300;		/* 5 second timeout			*/
				while (ScsiBusBusy()) {
					if (TickCount() > watchdog) {
						status = scArbNBErr;
						goto exit;
					}
				}
				/*
				 * The bus is free, try to grab it
				 */
				for (iCount = 0; iCount < 4; iCount++) {
					if ((status = SCSIGet()) == noErr)
						break;
				}
				if (status == noErr)
					break;							/* Success: we have the bus */
				/*
				 * The bus became busy again. Try to wait for it to go free.
				 */
				for (iCount = 0; iCount < 100 && ScsiBusBusy(); iCount++)
					;
			} /* The getTries loop */
			if (status != noErr) {
				/*
				 * The SCSI Manager thinks the bus is not busy and not selected,
				 * but "someone" has set its internal semaphore that signals
				 * that the SCSI Manager itself is busy. The application will have
				 * to handle this problem. (We tried getTries * 4 times).
				 */
				goto exit;
			}
			/*
			 * We now own the SCSI bus. Try to select the device.
			 */
			if ((status = SCSISelect(targetID)) != noErr)
				goto exit;
			/*
			 * From this point on, we must exit through SCSIComplete() even if an
			 * error is detected. Send a command to the selected device. There are
			 * several failure modes, including an illegal command (such as a
			 * write to a read-only device). If the command failed because of
			 * "device busy", we will try it again.
			 */
			status = SCSICmd((Ptr) scsiCommand, cmdBlockLength);
			if (status == noErr && bufferPtr != NULL) {
				/*
				 * This command requires a data transfer.
				 */
				if (writeToDevice) {
					if (transferQuantum == 1)
						status = SCSIWrite((Ptr) tib);
					else {
						status = SCSIWBlind((Ptr) tib);
					}
				}
				else {
					if (transferQuantum == 1)
						status = SCSIRead((Ptr) tib);
					else {
						status = SCSIRBlind((Ptr) tib);
					}
				}
			}
finish:
			/*
			 * SCSIComplete "runs" the bus-phase algorithm until the bitter end,
			 * returning the status and command-completion message bytes..
			 */
			completionStatus = SCSIComplete(
						(short *) stsBytePtr,
						&messageByte,
						completionTimeout
					);
			/*
			 * If we have an error here, return as the "final" status.
			 * 
			 */
			if (completionStatus != noErr)
				status = completionStatus;
			else {
				/*
				 * ScsiComplete is happy. If the device is busy, Pause for 1/4
				 * second and try again.
				 */
				if (*stsBytePtr == kScsiStatusBusy) {
					watchdog = TickCount() + 15;
					while (TickCount() < watchdog)
						;
					continue;				/* Do next totalTries attempt		*/
				}
			}
			/*
			 * This is the normal exit (success) or final failure exit.
			 */
			break;
		} /* totalTries loop */
exit:
		/*
		 * If we held memory, unhold it now.  We ignore UnholdMemory errors:
		 * there isn't much we can do about them. Note that this must be
		 * done by driver or asynchronous completion routines.
		 */
		if ((vmHoldMask & kHoldUserBuffer) != 0)
			(void) UnholdMemory(bufferPtr, transferSize);
		if ((vmHoldMask & kHoldCommandBlock) != 0)
			(void) UnholdMemory((Ptr) scsiCommand, cmdBlockLength);
		if ((vmHoldMask & kHoldStack) != 0)
			(void) UnholdMemory(vmProtectedStackBase, kSCSIProtectedStackSize);
		if ((vmHoldMask & kHoldFunction) != 0)
			(void) UnholdMemory(OriginalSCSI, vmFunctionSize);
		/*
		 * Return the number of bytes transferred to the caller. If the caller
		 * supplied an actual count and the count is no greater than the maximum,
		 * ignore any phase errors.
		 */
		if (actualTransferCount != NULL) {
			*actualTransferCount = myTransferCount;
			if (status == scPhaseErr
			 && writeToDevice == FALSE
			 && myTransferCount <= transferSize
			 && myTransferCount > 0)
				status = noErr;
		}
		/*
		 * Return an artificial error if the device returns a non-zero status:
		 *	statusErr		Caller should issue RequestSense.
		 *	controlErr		Device is busy (self-test?) try again later.
		 *	ioErr			Something is dreadfully wrong.
		 *	scPhaseErr		This may not be a "real" error -- it may mean that the
		 *					user data buffer was too large for the transfer. (See
		 *					the check above.)
		 * Also, there is a bug in the combination of System 7.0.1 and the 53C96
		 * that may cause the real SCSI Status Byte to be in the Message byte.
		 */
		if (*stsBytePtr == kScsiStatusGood
		 && messageByte == kScsiStatusCheckCondition)
			*stsBytePtr = kScsiStatusCheckCondition;
		if (status == noErr) {
			switch (*stsBytePtr) {
			case kScsiStatusGood:									break;
			case kScsiStatusCheckCondition:	status = statusErr;		break;
			case kScsiStatusBusy:			status = controlErr;	break;
			default:						status = ioErr;			break;
			}
		}
		return (status);
}

static void NextFunction(void) { }	/* Dummy function for OriginalSCSI size	*/

static Boolean
IsVirtualMemoryRunning(void)
{
		OSErr						status;
		long						response;
		
		status = Gestalt(gestaltVMAttr, &response);
		/*
		 * VM is active iff Gestalt succeeded and the response is appropriate.
		 */
		return (status == noErr && ((response & (1 << gestaltVMPresent)) != 0));
}
