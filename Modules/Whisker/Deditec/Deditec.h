//==============================================================================
//
// Title:		Deditec.h
// Purpose:		A short description of the interface.
//
// Created on:	22-7-2015 at 16:49:01 by Vinod Nigade.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __Deditec_H__
#define __Deditec_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files
#include "DAQLab.h"
#include "delib.h"

//==============================================================================
// Constants
#define TOT_IO_CH			32		/* Total number of I/O channels */
#define TOT_VALID_IO_CH		4		/* Total number of Useful channel for our module */
		
#define AIR_PUFF_CH			0
#define DROP_IN_CH			1
#define DROP_OUT_CH			2
#define ZAXIS_MOVE_CH		3
		
#define ON					1
#define OFF					0
//==============================================================================
// Types

/**
 * Deditec USB-TTL device. 
 */
typedef struct {
	int			module_number;				/* Specifies which module to open e.g. USB_TTL_32 */
	ULONG		handle;						/* Handle to the corresponding module */
	int			IO_Channel[TOT_VALID_IO_CH];/* Stores I/O channel number required */ 		
} delib_device_t;

//==============================================================================
// External variables

//==============================================================================
// Global functions

/* Init Device functions */
int init_deditec_device(delib_device_t *de_dev);

/* CLose device functions */
void close_deditec_device(delib_device_t *de_dev);

int set_with_timer(delib_device_t *de_dev, ULONG ch, unsigned int flag, long time);
int set_without_timer(delib_device_t *de_dev, ULONG ch, unsigned int flag);
int get_input_value(delib_device_t *de_dev, ULONG ch);
int is_deditec_error(void);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __Deditec_H__ */