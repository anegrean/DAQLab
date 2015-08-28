//==============================================================================
//
// Title:		Deditec.c
// Purpose:		Deditec hardware interfaces are defined.
//
// Created on:	22-7-2015 at 16:49:54 by Vinod Nigade.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "Deditec.h"

//==============================================================================
// Constants


//==============================================================================
// Types

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables

//==============================================================================
// Global functions

/************************************************************
 * Deditec Device functions will follow after this section  *
 ************************************************************/

/* Return status of input channel.
 * -1 : Error
 * Else 0 or 1.
 */
int
get_input_value(delib_device_t *de_dev, ULONG ch)
{
	ULONG	input;
	
	if (de_dev->handle == 0) {
		return -1;
	}
	
	CmtGetLock(de_dev->lock);
	input = DapiDIGet1(de_dev->handle, ch);
	CmtReleaseLock(de_dev->lock);
	
	if (is_deditec_error()) {
		return -1;
	}
	
	return (int)input;
}

int
set_without_timer(delib_device_t *de_dev, ULONG ch, unsigned int flag)
{
	if (de_dev->handle == 0) {
		return 0;	
	}
	
	CmtGetLock(de_dev->lock);
	DapiDOSet1(de_dev->handle, ch, flag);
	CmtReleaseLock(de_dev->lock);
	if (is_deditec_error()) {
		return -1;
	}
	
	return 0;
}

int
set_with_timer(delib_device_t *de_dev, ULONG ch, unsigned int flag, long time)
{
	if (de_dev->handle == 0) {
		return 0;	
	}
	
	CmtGetLock(de_dev->lock);
	DapiDOSet1(de_dev->handle, ch, flag);
	CmtReleaseLock(de_dev->lock);
	
	if (is_deditec_error()) {
		return -1;
	}

	DEDITEC_DELAY(time);		/* TODO: It should be nearly accurate time. */
	
	flag ^= 1 << 0;
	CmtGetLock(de_dev->lock);
	DapiDOSet1(de_dev->handle, ch, flag);
	CmtReleaseLock(de_dev->lock);
	
	if (is_deditec_error()) {
		return -1;
	}
	
	return 0;
}

static int 
is_deditec_error()
{
	unsigned char msg[500];

	if (DapiGetLastError() != DAPI_ERR_NONE) {

		DapiGetLastErrorText((unsigned char*) msg, sizeof(msg));
		LOG_MSG2(5, "Error Code = %x * Message = %s\n", 0, msg);

		DapiClearLastError();

		return TRUE;
	}

	return FALSE;
}

int
init_deditec_device(delib_device_t *de_dev)
{
	ULONG	handle;
	
	handle = DapiOpenModule(de_dev->module_number, 0);
	if (handle == 0) {
		LOG_MSG(0, "Cannot open deditec device \n");
		return -1;
	}
	
	LOG_MSG1(9, "Deditec module handle %ul", handle);
	de_dev->handle = handle;
	
	/* Set Direction.
	 * CH 1-8 to output : i.e. first bit 1 (lowermost)
	 * CH 9-16 to input : i.e. second bit 0
	 * CH 17-24 to output : i.e. third bit 1
	 * Therefore, Direction = 101
	 */
	DapiSpecialCommand(handle, DAPI_SPECIAL_CMD_SET_DIR_DX_8, 0 /* Start channel */, 
					   					0x5 /* Direction */, 0);
	if (is_deditec_error()) {
		close_deditec_device(de_dev);
		return -1;
	}
	
	/* Get a lock for multi-thread protection 
	 * TODO: It has to be a spin lock not the sleep lock
	 */ 
	CmtNewLock (NULL, 0, &(de_dev->lock));
	return 0;
}

void
close_deditec_device(delib_device_t *de_dev)
{
	if (de_dev->handle == 0) {					/* Not initialized */
		return;
	}
	
	DapiCloseModule(de_dev->handle);
	de_dev->handle = 0;
	
	/* Discard lock */
	CmtDiscardLock(de_dev->lock);
	return;
}
