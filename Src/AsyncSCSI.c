/*									AsyncSCSI.c									*/
/*
 * AsyncSCSI.c
 * Copyright � 1992-93 Apple Computer Inc. All Rights Reserved.
 *
 * Talk to the Macintosh SCSI Manager 4.3 using the "new" interface. This is a
 * synchronous call that executes a single SCSI Command on a device. It will
 * automatically calls Request Sense on errors. Note: this is intended as a self-
 * contained illustration of the Asynchronous SCSI Manager and is (intentionally)
 * inefficient in that it does many "bureaucratic" things that would normally
 * be done once when an application or device driver is initialized.
 *
 * Calling Sequence:
 *		OSErr				AsyncSCSI(
 *				DeviceIdent				scsiDevice,
 *				const SCSI_CommandPtr	scsiCommand,
 *				unsigned short			cmdBlockLength,
 *				Boolean					writeToDevice,
 *				Ptr						bufferPtr,
 *				unsigned long			transferSize,
 *				unsigned short			scsiHandshake[handshakeDataLength],
 *				SCSI_Sense_Data			*senseDataPtr,
 *				unsigned long			senseDataSize,
 *				unsigned long			completionTimeout,
 *				unsigned short			*stsBytePtr,
 *				unsigned long			*actualTransferCount
 *			);
 * The parameters have the following meaning:
 *
 *	scsiDevice			The SCSI host bus, target, lun that we are talking to.
 *	scsiCommand			The SCSI Command Block (6, 10, or 12 bytes).
 *	cmdBlockLength		The length in bytes of the command block.
 *	writeToDevice		TRUE if this command writes to the device. FALSE if this
 *						command reads from the device or does not require a data
 *						phase.
 *	bufferPtr			The user data buffer for Read/Write commands. It should be
 *						NULL if a data transfer phase is not used for this command.
 *						(e.g. for Test Unit Ready).
 *	transferSize		The total number of bytes to transfer.
 *	scsiHandshake		This will be copied to the scsiHandshake field in the
 *						SCSIAction parameter block. Useful handshake fields
 *						include the following:
 *							0				No handshake: do one blind transfer,
 *							512,0			Normal disk blind transfer,
 *							1,511,0			Disk blind transfer if the device
 *											can stall before *and* after the
 *											first byte in a sector.
 *						If scsiHandshake is NULL, a polled transfer will be done..
 *	senseDataPtr		If not NULL and the original request failed, this will
 *						be filled with the result from a Request Sense operation.
 *	senseDataSize		This is the size of the Request Sense data buffer.
 *	completionTimeout	The timeout (in Ticks) for the command. This should be
 *						short for disks, but must be long for tape devices and
 *						some setup requests, such as Mode Select.
 *	stsBytePtr			This short is set to the byte returned in the device's
 *						Status Phase.
 *	actualTransferCount	This will be set to the number "cycles" through the TIB
 *						loop. This should equal the number of bytes transferred if
 *						transferCount is set to one. (Ignored if NULL.)
 * Return codes:
 *	noErr			normal
 *	unimpErr		SCSI Manager 4.3 is not available: the calling function should
 *					call the "old" (Inside Mac IV) SCSI Manager.
 *	statusErr		Device returned "Check condition" and SCSI Manager was able
 *					to successfully issue Request Sense. There is data in the
 *					Sense Record, but you cannot assume that the original request
 *					succeeded.
 *	paramErr		Could not determine the command length.
 *	scsi...			Other error
 */
#include <GestaltEqu.h>
#include <Memory.h>
#include <Events.h>
#include <Errors.h>
#include "MacSCSICommand.h"
#ifndef TRUE
#define TRUE		1
#define FALSE		0
#endif

/*
 * These globals are defined by SCSISimpleSample.h, and are needed only for
 * testing and debugging.
 */
extern Boolean					gEnableSelectWithATN;
extern Boolean					gDoDisconnect;
extern Boolean					gDontDisconnect;

static void 					NextFunction(void);		/* For HoldMemory size	*/
/*
 * This should be specified at application/driver startup, and not the
 * inefficient "call Gestalt each time" shown here.
 */
static Boolean					IsVirtualMemoryRunning(void);
#ifndef CLEAR
/*
 * Cheap 'n dirty memory clear routine.
 */
#define CLEAR(record) do {								\
		register char	*ptr = (char *) &record;		\
		register long	size;							\
		for (size = sizeof record; size > 0; --size)	\
			*ptr++ = 0;									\
	} while (0)

#endif

/*
 * These are bitmasks for the vmHoldMask variable. A bit is set if its associated
 * memory element has been held in protected (non-paged) memory.
 */
#define kHoldFunction			0x0001				/* AsyncSCSI function code	*/
#define kHoldStack				0x0002				/* Local variables			*/
#define kHoldUserBuffer			0x0004				/* User data buffer, if any	*/
#define kHoldSenseBuffer		0x0008				/* Sense buffer, if any		*/
#define kHoldParamBlock			0x0010				/* SCSIExecIOPB				*/

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
 * Execute a SCSI command.
 * Returns the final status as noted above.
 */
OSErr
AsyncSCSI(
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
	)
{
		OSErr					status;				/* Result code				*/
		SCSIBusInquiryPB		busInquiryPB;		/* Used for SCSIBusInquiry	*/
		register SCSIExecIOPB	*execIOPBPtr;		/* Used for SCSIAction		*/
#define PB						(*execIOPBPtr)		/* PB references paramBlock	*/
		unsigned long			execIOPBSize;		/* SCSIAction pb size		*/
		Boolean					enableSelectWithATN; /* Ok to select with ATN?	*/
		register short			i;					/* Move command block index	*/
		
		/*
		 * These two flags are used to record whether the asynchronous SCSI
		 * Manager is present on this machine (one flag records whether it is
		 * present, the other whether we've tested for presence). This is
		 * reasonable for applications. However, this is not sufficient for
		 * drivers or other code that may be called before the system has been
		 * completely initialized, as the asynchronous SCSI Manager may be
		 * installed by a System Extension.
		 */
		static Boolean			gHasAsyncSCSIManager;
		static Boolean			gTestedForAsyncSCSIManager;
		/*
		 * The following parameters are used to manage virtual memory.
		 */
		unsigned short			vmHoldMask;
		unsigned long			vmFunctionSize;
		void					*vmProtectedStackBase;	/* Last local var	*/
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
		 * If the asynchronous SCSI Manager exists, we will allocate a parameter
		 * block that will be freed when the function exits (if allocation
		 * succeeded). In a real application or driver, this would be allocated
		 * once, as part of the per-device initialization.
		 */
		execIOPBPtr = NULL;
		/*
		 * First, make sure that the asynchronous SCSI Manager has been installed.
		 * In an application, this may be done once when the application starts.
		 * In a driver, this test must be deferred until the Process Manager
		 * is running.
		 */
		if (gTestedForAsyncSCSIManager == FALSE) {
			gTestedForAsyncSCSIManager = TRUE;
			gHasAsyncSCSIManager = (
					NGetTrapAddress(_SCSIAtomic, OSTrap)
					!= NGetTrapAddress(_Unimplemented, OSTrap)
				);
		}
		if (gHasAsyncSCSIManager == FALSE) {
			status = unimpErr;
			goto exit;
		}
		/*
		 * Allocate a parameter block for this bus. In a production application
		 * or driver, this would be done, once, along with other initialization.
		 * Note that we always clear the parameter block: the SCSI Manager will
		 * fail with an error if some fields it expects to be NULL (such as
		 * the queue link) are non-NULL.
		 */
		CLEAR(busInquiryPB);
		busInquiryPB.scsiPBLength = sizeof busInquiryPB;
		busInquiryPB.scsiFunctionCode = SCSIBusInquiry;
		busInquiryPB.scsiDevice = scsiDevice;
		SCSIAction((SCSI_PB *) &busInquiryPB);
		status = busInquiryPB.scsiResult;
		if (status != noErr)
			goto exit;
		/*
		 * If we are running on a Quadra 840-AV with a CD300, the Macintosh will
		 * hang if it tries to access LUN 1. We check for this problem in two
		 * ways: by examining a global "LUN 1 test ok" flag, and by checking
		 * whether the bug was fixed, either by running on later hardware or by
		 * installing a System Update.
		 */
		enableSelectWithATN =
				gEnableSelectWithATN
				&& (busInquiryPB.scsiWeirdStuff & scsiTargetDrivenSDTRSafe) != 0; 
		/*
		 * Allocate a parameter block for this request using the size that
		 * was returned in the busInquiry parameter block.
		 */
		execIOPBSize = busInquiryPB.scsiIOpbSize;
		execIOPBPtr = (SCSIExecIOPB *) NewPtrClear(execIOPBSize);
		if (execIOPBPtr == NULL) {
			status = MemError();
			goto exit;
		}
		/*
		 * Setup the parameter block for the user's request.
		 */
		PB.scsiPBLength = execIOPBSize;
		PB.scsiFunctionCode = SCSIExecIO;
		PB.scsiTimeout = completionTimeout;
		PB.scsiDevice = scsiDevice;
		PB.scsiCDBLength = cmdBlockLength;
		/*
		 * Copy the command block into the SCSI ExecIO Parameter block to
		 * centralize everything for debugging. Also, this is one less thing to
		 * have to lock into physical memory. Note that BlockMove of six, ten,
		 * or twelve bytes is very inefficient. Utility application software
		 * should move the bytes using an inline operation, and drivers will
		 * either store a pointer to a per-request command data block or
		 * explicitly construct the command in the paramater block.
		 */
		for (i = 0; i < cmdBlockLength; i++)
			PB.scsiCDB.cdbBytes[i] = scsiCommand->scsi[i];
		/*
		 * Specify the transfer direction, if any, and setup the other SCSI
		 * operation flags. scsiSIMQNoFreeze prevents the SCSI Manager from
		 * blocking further operation if an error is detected.
		 */
		PB.scsiFlags = scsiSIMQNoFreeze;
		if (bufferPtr == NULL || transferSize == 0)
			PB.scsiFlags |= scsiDirectionNone;
		else {
			/*
			 * If the user did not specify a scsi handshake field, select "polled"
			 * transfers, otherwise, select "blind."
			 */
			PB.scsiTransferType = (scsiHandshake == NULL)
						? scsiTransferPolled
						: scsiTransferBlind;
			PB.scsiDataPtr = (unsigned char *) bufferPtr;
			PB.scsiDataLength = transferSize;
			PB.scsiDataType = scsiDataBuffer;
			PB.scsiFlags |= (writeToDevice) ? scsiDirectionOut : scsiDirectionIn;
			if (scsiHandshake != NULL) {
				for (i = 0; i < handshakeDataLength; i++)
					PB.scsiHandshake[i] = scsiHandshake[i];
			}
		}
		/*
		 * Do we support autosense?
		 */
		if (senseDataPtr != NULL && senseDataSize >= 5) {
			senseDataPtr->errorCode = 0;
			PB.scsiSensePtr = (unsigned char *) senseDataPtr;
			PB.scsiSenseLength = senseDataSize;
		}
		else {
			PB.scsiFlags |= scsiDisableAutosense;
		}
		/*
		 * Look at the global flags that can be set to configure the asynchronous
		 * SCSI Manager - these are for testing, and would typically be set
		 * permanently to a particular (device-specific) state by a real application.
		 */
		if (enableSelectWithATN == FALSE)	/* Enabled by user and SCSI manager? */
			PB.scsiIOFlags |= scsiDisableSelectWAtn;
		if (gDoDisconnect)
			PB.scsiFlags |= scsiDoDisconnect;
		if (gDontDisconnect)
			PB.scsiFlags |= scsiDontDisconnect;
		/*
		 * We are now ready to perform the operation. If virtual memory is active
		 * however, we must lock down all memory segments that can be potentially
		 * "touched" while the SCSI request is being executed. All of this is
		 * needed for applications. For drivers, this can generally be ignored as
		 * the driver code and driver-specific resources are stored in the System
		 * Heap, which is always "held" in physical memory and the Device Manager
		 * locks down data buffers when PBRead/PBWrite are called. However, note
		 * that if Control or Status calls require data transfers, the driver
		 * must explicitly lock the buffer.
		 */
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
				(unsigned long) NextFunction - (unsigned long) AsyncSCSI;
			status = HoldMemory(AsyncSCSI, vmFunctionSize);
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
				 * Lock down the parameter block. In this sample, we allocated
				 * the parameter block in the application heap. A driver would
				 * typically allocate it in the System Heap and, hence, would
				 * not require this call.
				 */
				status = HoldMemory((Ptr) execIOPBPtr, execIOPBSize);
				if (status == noErr)
					vmHoldMask |= kHoldParamBlock;
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
			if (status == noErr && PB.scsiSensePtr != NULL) {
				/*
				 * Lock down the sense data. A driver would probably allocate one
				 * of these (in a per-transaction buffer) in the System Heap. An
				 * application should lock this before calling the SCSI Manager.
				 */
				status = HoldMemory(PB.scsiSensePtr, PB.scsiSenseLength);
				if (status == noErr)
					vmHoldMask |= kHoldSenseBuffer;
			}
		}
		/*
		 * Finally, call the asynchronous SCSI Manager. SCSIAction is synchronous
		 * because we did not specify a completion routine.
		 */
		if (status == noErr) {
			status = SCSIAction((SCSI_PB *) &PB);
			if (status == noErr)
				status = PB.scsiResult;
		}
		/*
		 * If we held memory, unhold it now.  We ignore UnholdMemory errors:
		 * there isn't much we can do about them. Note that this must be
		 * done by driver or asynchronous completion routines.
		 */
exit:	if ((vmHoldMask & kHoldSenseBuffer) != 0)
			(void) UnholdMemory(PB.scsiSensePtr, PB.scsiSenseLength);
		if ((vmHoldMask & kHoldUserBuffer) != 0)
			(void) UnholdMemory(bufferPtr, transferSize);
		if ((vmHoldMask & kHoldParamBlock) != 0)
			(void) UnholdMemory((Ptr) execIOPBPtr, execIOPBSize);
		if ((vmHoldMask & kHoldStack) != 0)
			(void) UnholdMemory(vmProtectedStackBase, kSCSIProtectedStackSize);
		if ((vmHoldMask & kHoldFunction) != 0)
			(void) UnholdMemory(AsyncSCSI, vmFunctionSize);
		/*
		 * Now, look at the result of the operation.
		 */
		if (execIOPBPtr != NULL) {
			/*
			 * Recover some data to return to the user. 
			 */
			if (stsBytePtr != NULL)
				*stsBytePtr = PB.scsiSCSIstatus;
			if (actualTransferCount != NULL)
				*actualTransferCount = transferSize - PB.scsiDataResidual;
			/*
			 * Note: scsiDataRunError is issued if our transfer request was larger
			 * or smaller than the actual transfer length. We need to examine the
			 * actual transfer sizes to see how to handle this error. This is
			 * not necessarily complete or correct. The intent here is to supress
			 * the transfer length error when executing Request Sense or other
			 * administrative commands with variable-length result blocks.
			 * Note that the user can then recover the actual block length by
			 * examining the actualTransferCount parameter.
			 */
			if (status == scsiDataRunError				/* Over/underrun error	*/
			 && writeToDevice == FALSE					/* But we're reading	*/
			 && actualTransferCount != NULL				/* And user wants count	*/
			 && (*actualTransferCount) <= transferSize	/* And its a short read	*/
			 && (*actualTransferCount) > 0)				/* And some data read?	*/
				status = noErr;							/* If so, ignore error	*/
			/*
			 * If the device issued Check Condition and the SCSI Manager was able
			 * to retrieve a Request Sense datum, change the error to our private
			 * "Check Condition" status.
			 */
			if (status == scsiNonZeroStatus
			 && (PB.scsiResultFlags & scsiAutosenseValid) != 0)
			 	status = statusErr;
		 	DisposePtr((Ptr) execIOPBPtr);
		}
		return (status);
#undef PB
}

static void NextFunction(void) { }	/* Dummy function for AsyncSCSI size	*/

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

