//==============================================================================
//
// Title:		Zaber.h
// Purpose:		A short description of the interface.
//
// Created on:	22-7-2015 at 16:01:44 by Vinod Nigade.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __Zaber_H__
#define __Zaber_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files
#include "DAQLab.h"
#include "stdint.h"
#include "za_serial.h"

//==============================================================================
// Constants
#define DEVICE_NAME_LEN		6					/* Device name length including '\0' character */
#define MIN					0					
#define MAX					1
#define CMD_LEN				32					/* Command length */
//==============================================================================
// Types
		
/**
 * Zaber device XY stage.
 */
typedef struct {
	char		device_name[DEVICE_NAME_LEN];	/* Stores device port name e.g. COM1 */
	z_port		port;							/* Port to connect to zaber device */
	char		*comport_list[20];				/* Stores list of available comport */
	int			VALID_DEVICE;					/* Flag to indicate device is opened and initialized */
	uint32_t 	absolute_position;				/* Absolute position */
} zaber_device_t;

//==============================================================================
// External variables

//==============================================================================
// Global functions

int  init_zaber_device(zaber_device_t *z_dev);
int  close_zaber_device(zaber_device_t *z_dev);

int	 send_MoveABS_cmd(z_port port, uint32_t abs_position);
int  send_MoveRel_cmd(z_port port, int device, int position);
void get_available_COMport(char *comport[]);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __Zaber_H__ */
