/*								SCSIGetCommandLength.c							*/
/*
 * SCSIGetCommandLength.c
 * Copyright � 1992-94 Apple Computer Inc. All Rights Reserved.
 *
 * Use the command byte to determine the length of the SCSI Command. This will
 * work for all registered SCSI-II commands, but not for "vendor-specific"
 * commands, or commands outside of the registered range.  Returns zero
 * for "unknown" or one of the defined command lengths, 6, 10, or 12..
 *
 * Calling Sequence:
 *		unsigned short		SCSIGetCommandLength(
 *				const Ptr		cmdBlock
 *			);
 */
#include "MacSCSICommand.h"
unsigned short				SCSIGetCommandLength(
		const SCSI_CommandPtr	cmdBlock
	);

unsigned short
SCSIGetCommandLength(
		const SCSI_CommandPtr	cmdBlock
	)
{
		unsigned short			result;
		/*
		 * Look at the "group code" in the command operation. Return a parameter
		 * error for the reserved (3, 4) and vendor-specific command (6, 7)
		 * command groups. Otherwise, set the command length from the group code
		 * value as specified in the SCSI-II spec. Then, copy the command block
		 * into the parameter block (this centralizes everything for debugging
		 * convenience).
		 */
		switch (cmdBlock->scsi[0] & 0xE0) {
		case (0 << 5):	result = 6;		break;
		case (1 << 5):
		case (2 << 5):	result = 10;	break;
		case (5 << 5):	result = 12;	break;
		default:		result = 0;		break;
		}
		return (result);
}
