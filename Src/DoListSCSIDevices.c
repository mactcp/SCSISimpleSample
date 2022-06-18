/*								DoListSCSIDevices.c								*/
/*
 * DoListSCSIDevices.c
 * Copyright � 1992-94 Apple Computer Inc. All Rights Reserved.
 *
 * Find all SCSI devices. The alogrithm first asks the SCSI Manager for the
 * number of busses, then loops through each bus for each device and LUN.
 * old SCSI Manager. This is made complex by the flexible SCSI Manager 4.3
 * architecture: it is possible for the asynchronous SCSI Manager to only
 * be available on a third-party bus interface, for example. Because of this,
 * we must always scan the bus using the original SCSI Manager even if the
 * asynchronous manager is present.
 */
#include "SCSISimpleSample.h"

void
DoListSCSIDevices(void)
{
		OSErr							status;
		unsigned short					lastHostBus;
		unsigned short					initiatorID;
		unsigned short					bus;
		unsigned short					targetID;
		unsigned short					LUN;
		DeviceIdent						scsiDevice;
		SCSIGetVirtualIDInfoPB			scsiGetVirtualIDInfo;
		short							deviceCount;
		unsigned short					maxTarget;
		Boolean							useAsynchManager;
		Str255							work;
		
		LOG("\pList all SCSI Devices");
		deviceCount = 0;
		/*
		 * If we have the asynchronous SCSI Manager, find out how many busses
		 * are present on this system. If not, force a "single bus" scan, since 
		 * DoSCSICommandWithSense ignores the hostBus information if it is
		 * forced into "old-style" calls.
		 */
		if (gEnableNewSCSIManager)
			status = SCSIGetHighHostBusAdaptor(&lastHostBus);
		else {
			status = noErr;
			lastHostBus = 0;					/* Force one bus only			*/
		}
		if (status == noErr) {
			for (bus = 0; bus <= lastHostBus; bus++) {
				/*
				 * Look at this SCSI bus. This would be a good place to allocate
				 * the SCSIExecIO command block. In this sample, however, it's
				 * allocated on each call to AsyncSCSI, though this is inefficient.
				 * Note that it is possible to have busses with no devices. This
				 * is true for Apple Macintosh models with two busses (such as
				 * the Quadra 950 and PowerMac 8100). Also, if you install a
				 * third-party bus adaptor that supports the asynchronous SCSI
				 * Manager on a machine with two busses, it would be assigned
				 * bus 2 (with busses 0 and 1 referencing the internal system
				 * busses). In this case, a system could have no devices on bus
				 * 0 or 1.
				 */
				*((long *) &scsiDevice) = 0;
				scsiDevice.bus = bus;
				/*
				 * Check whether we can access this scsi device. SCSIBusAPI will
				 * return an error status if this bus is inaccessable (i.e. no bus
				 * or other trouble). If it returns noErr, useAsyncManager will
				 * be TRUE if the asynchronous SCSI Manager is supported for this
				 * bus, and FALSE if it can only be accessed through the original
				 * SCSI Manager. This would indicate that a third-party bus
				 * interface patched the original SCSI Manager traps (i.e.,
				 * patched SCSIGet, SCSISelect, etc).
				 */
				status = SCSIBusAPI(scsiDevice, &useAsynchManager);
				if (status == noErr) {
					if (useAsynchManager)
						status = SCSIGetInitiatorID(scsiDevice, &initiatorID);
					else {
						initiatorID = 7;	/* Asynch manager is disabled		*/
					}
				}
				if (status != noErr)
					continue;
				/*
				 * SCSIGetInitiatorID returned the bus ID of the Macintosh. This
				 * is almost always seven, but only the SCSI Manager knows for
				 * sure. Note that, by getting the Macintosh bus ID dynamically,
				 * we prepare the code for a future system that permitted more
				 * than one Macintosh on the same SCSI bus.
				 */
				status = SCSIGetMaxTargetID(scsiDevice, &maxTarget);
				for (targetID = 0; targetID <= maxTarget; targetID++) {
					if (targetID != initiatorID) {
						scsiDevice.targetID = targetID;
						for (LUN = 0; LUN <= gMaxLogicalUnit; LUN++) {
							/*
							 * Try to send a command to this LUN. If it fails,
							 * don't try for higher-valued LUNs.
							 * SCSICheckForDevicePresent looks, carefully, at the
							 * returned error to distinguish between missing
							 * devices and devices that are present, but unable to
							 * respond, such as CD-ROM players with no disk
							 * inserted. This call to SCSICheckForDevicePresent
							 * will use the asynchronous SCSI Manager if it can.
							 *
							 * Note that, if the asynchronous manager is not
							 * available, we can still check for non-zero LUNs by
							 * using the old method of stuffing the LUN into the
							 * command block, however this is not supported in
							 * this example.
							 */
							scsiDevice.LUN = LUN;
							if (SCSICheckForDevicePresent(
										scsiDevice, useAsynchManager) == FALSE)
								break;					/* Don't look for LUNs	*/
							else {
								++deviceCount;			/* Found a device		*/
								DoGetDriveInfo(scsiDevice, TRUE, useAsynchManager);
							}							/* Check status			*/
						}								/* LUN loop				*/
					}									/* Not the initiator id	*/
				}										/* Target loop			*/
			}											/* Bus loop				*/
			/*
			 * Now, we need to look at the hard-wired SCSI drive addresses and
			 * check whether a third-party hardware interface that does not use
			 * the asynchronous SCSI Manager recognizes this address. If
			 * gEnableNewSCSIManager is FALSE, the above loop called the original
			 * SCSI Manager, so we don't have to try it again. In this sequence,
			 * we hard-wire the initiator ID to seven, as there is no supported
			 * way to determine it from the SCSI Manager or operating system.
			 */
			if (gEnableNewSCSIManager) {
				scsiDevice.bus = 0;
				for (targetID = 0; targetID <= 6; targetID++) {
					 CLEAR(scsiGetVirtualIDInfo);
					 scsiGetVirtualIDInfo.scsiPBLength = sizeof scsiGetVirtualIDInfo;
					 scsiGetVirtualIDInfo.scsiOldCallID = targetID;
					 status = SCSIAction((SCSI_PB *) &scsiGetVirtualIDInfo);
					 if (status != noErr) {
					 	/*
					 	 * The asynchronous SCSI Manager does not know about this
					 	 * target ID. Check whether it exists (forcing the request
					 	 * to use the original SCSI Manager).
					 	 */
					 	scsiDevice.targetID = targetID;
					 	for (LUN = 0; LUN <= gMaxLogicalUnit; LUN++) {
					 		scsiDevice.LUN = LUN;
							if (SCSICheckForDevicePresent(scsiDevice, FALSE) == FALSE)
								break;					/* Don't look for LUNs	*/
							else {
								++deviceCount;			/* Found a device		*/
								DoGetDriveInfo(scsiDevice, TRUE, FALSE);
							}							/* Check status			*/
						}
					}
				}
			}
		}												/* Found a host adaptor	*/
		NumToString(deviceCount, work);
		AppendPascalString(work, "\p SCSI Devices");
		LOG(work);
}		
