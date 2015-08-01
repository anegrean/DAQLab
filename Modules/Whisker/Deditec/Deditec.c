//==============================================================================
//
// Title:		Deditec.c
// Purpose:		A short description of the implementation.
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
	
	input = DapiDIGet1(de_dev->handle, ch);
	
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
	
	DapiDOSet1(de_dev->handle, ch, flag);
	if (is_deditec_error()) {
		return -1;
	}
	
	/* TODO : Remove this. Check output of this channel 
	ULONG	input;
	input = DapiDIGet1(de_dev->handle, ch);
	LOG_MSG1(9, "DEditec input value check %ul ", input);
	*/
	return 0;
}

int
set_with_timer(delib_device_t *de_dev, ULONG ch, unsigned int flag, long time)
{
	if (de_dev->handle == 0) {
		return 0;	
	}
	
	DapiDOSet1(de_dev->handle, ch, flag);
	if (is_deditec_error()) {
		return -1;
	}

	Sleep(time);				/* TODO: It should be nearly accurate time. */
	
	flag ^= 1 << 0;
	DapiDOSet1(de_dev->handle, ch, flag);
	if (is_deditec_error()) {
		return -1;
	}
	
	return 0;
}

int 
is_deditec_error()
{
	unsigned char msg[500];

	if (DapiGetLastError() != DAPI_ERR_NONE)
	{

		DapiGetLastErrorText((unsigned char*) msg, sizeof(msg));
		printf("Error Code = %x * Message = %s\n", 0, msg);

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
	
	/* Set I/O to output */
	DapiSpecialCommand(handle, DAPI_SPECIAL_CMD_SET_DIR_DX_8, 0, 0xf, 0);
	if (is_deditec_error()) {
		close_deditec_device(de_dev);
		return -1;
	}
	
	/* Set channel 6 to 1 */
	//DapiDOSet1(handle, 4, 1);
	//is_deditec_error();
	
	//DapiSpecialCommand(handle, DAPI_SPECIAL_CMD_SET_DIR_DX_8, 0, 0, 0);
	//is_deditec_error();
	
	//DapiDOSet1(handle, 4, 0);
	//is_deditec_error();
	
	//set_ch_OnOff(de_dev, AIR_PUFF_CH);
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
	return;
}
