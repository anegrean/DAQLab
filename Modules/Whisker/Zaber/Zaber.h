//==============================================================================
//
// Title:		Zaber.h
// Purpose:		Zaber device specific functionality.
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
#define	ZABER_DEV			0
#define ZABER_X_DEV			1
#define ZABER_Y_DEV			2
#define DEVICE_NAME_LEN		6					/* Device name length including '\0' character */
#define MIN					0					
#define MAX					1
#define CMD_LEN				32					/* Command length */
#define RESPONSE_LEN		256					/* Device Response length */
#define RESPONSE_DATA_LEN	128					/* Response data length */
#define	ASYNC				1					/* Perform movement Asynchronously */
#define SYNC				0					/* Perform movement Synchronously */
#define ZABER_DELAY(x)		Sleep(x)			/* Delay time */
//==============================================================================
// Types

/* Position */
typedef struct {
	size_t	X;			/* X Axis position */
	size_t	Y;			/* Y Axis position */
	size_t	percent;	/* Percent associated with this position */
	int		go_state;	/* Whether this position is GO or NO-GO */
} Position_t;

/**
 * Zaber device XY stage.
 */
typedef struct {
	char		device_name[DEVICE_NAME_LEN];	/* Stores device port name e.g. COM1 */
	z_port		port;							/* Port to connect to zaber device */
	char		*comport_list[20];				/* Stores list of available comport */
	int			VALID_DEVICE;					/* Flag to indicate device is opened and initialized */
	ListType	saved_positions;				/* Stores saved positions */
} zaber_device_t;

//==============================================================================
// External variables

//==============================================================================
// Global functions

int  init_zaber_device(zaber_device_t *z_dev);
int  close_zaber_device(zaber_device_t *z_dev);

void        send_cmd(z_port port, char *cmd, char *data_buf, int mode);
int			send_MoveABS_cmd(z_port port, int device, uint32_t abs_position, int mode);
int			send_MoveRel_cmd(z_port port, int device, int position);
int			send_stop_cmd(z_port port, int device);
int			send_MoveMaxMin_cmd(z_port port, int device, char *sub_cmd);
void 		get_available_COMport(char *comport[]);
int			get_device_data(z_port port, int device, char	*sub_cmd);
void		set_device_data(z_port port, int device, char *sub_cmd, uint32_t value);
		
#ifdef __cplusplus
    }
#endif

#endif  /* ndef __Zaber_H__ */
