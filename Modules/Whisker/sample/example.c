//==============================================================================
//
// Title:		example.c
// Purpose:		Example file that explains how to use zaber and deditec device
//				functions defined in Zaber.h and Deditec.h
//
// Created on:	04-08-2015 at 01:25:57 by Vinod Nigade.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

#include "Zaber.h"
#include "Deditec.h"

#define ZABER_ABS_POS	120000

zaber_device_t	z_dev;					/* Zaber Device */
delib_device_t	de_dev;					/* Deditec Device */

int
main(int argc, char *argv[])
{
	char	port_name[DEVICE_NAME_LEN] = "COM3";	/* Check your serial port setting */
	int		ret = 0;
	
	strncpy(z_dev.device_name, port_name, DEVICE_NAME_LEN - 1);
	
	/* Connect to Zaber device */
    ret = init_zaber_device(&z_dev);
	if (ret < 0) {
		MessagePopup ("COM Port Open Error", "Fail to init zaber device!");
		return -1;
	}
	
	/* Send absolute Move command */
	send_MoveABS_cmd(z_dev.port, ZABER_ABS_POS);
	
	/* Close zaber device */
	close_zaber_device(&z_dev);
	
	/* Connect to Deditec device */
	ret = init_deditec_device(&de_dev);
	if (ret < 0) {
		MessagePopup ("Deditec module Open Error", "Fail to init deditec module!");
		return -1;	 
	}
	
	/* Send Output ON/OFF with timer */
	set_with_timer(&de_dev, 4 /* IO Channel 4 */, ON, 1000 /* 1 sec interval */);
	
	/* Check input signal */
	ret = get_input_value(&de_dev, 5 /* IO Channel 5 */);
	if (ret == 0) {
		MessagePopup ("Deditec Input", "I/O channel 5 has input OFF");
	} else {
		MessagePopup ("Deditec Input", "I/O channel 5 has input ON");
	}
	
	/* Close deditec device */
	close_deditec_device(&de_dev);
	return 0;
}
