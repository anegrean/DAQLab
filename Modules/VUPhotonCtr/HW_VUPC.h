//==============================================================================
//
// Title:		HW_VUPC.h
// Purpose:		A short description of the interface.
//
// Created on:	2-7-2014 at 14:31:24 by Lex.
// Copyright:	Vrije Universiteit. All Rights Reserved.
//
//==============================================================================

#ifndef __HW_VUPC_H__
#define __HW_VUPC_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "cvidef.h"

//==============================================================================
// Constants
		
			 //instrument registers
#define	 CTRL_REG		0x0000
#define	 STAT_REG		0x0004
#define	 VERS_REG		0x0008
#define	 ERROR_REG		0x000C
#define	 PMT1_CTRL_REG	0x0010
#define	 PMT2_CTRL_REG	0x0014
#define	 PMT3_CTRL_REG	0x0018
#define	 PMT4_CTRL_REG	0x001C
		
		//control register bits
#define	APPSTART_BIT	0x00000001
#define	DMASTART_BIT	0x00000002
#define	UPDPMT12_BIT	0x00000020
#define	UPDPMT34_BIT	0x00000040
#define	RESET_BIT		0x00000080 
#define	FFRESET_BIT		0x00000100
#define	TESTMODE0_BIT	0x00004000
#define	TESTMODE1_BIT	0x00008000  		
#define	PMT1HV_BIT		0x00010000		
#define	PMT1PELT_BIT	0x00020000		
#define	PMT1FAN_BIT		0x00040000
#define	PMT2HV_BIT		0x00100000		
#define	PMT2PELT_BIT	0x00200000		
#define	PMT2FAN_BIT		0x00400000
#define	PMT3HV_BIT		0x01000000		
#define	PMT3PELT_BIT	0x02000000		
#define	PMT3FAN_BIT		0x04000000
#define	PMT4HV_BIT		0x10000000		
#define	PMT4PELT_BIT	0x20000000		
#define	PMT4FAN_BIT		0x40000000
		
		//status register bits
#define RUNNING_BIT		0x00000001
#define PMT1TEMP_BIT	0x00000002		
#define PMT1CURR_BIT	0x00000004
#define PMT2TEMP_BIT	0x00000008		
#define PMT2CURR_BIT	0x00000010
#define PMT3TEMP_BIT	0x00000020		
#define PMT3CURR_BIT	0x00000040
#define PMT4TEMP_BIT	0x00000080		
#define PMT4CURR_BIT	0x00000100
#define TRIGFAIL_BIT	0x00000200
#define FFUNDRFL_BIT	0x00000400		
#define FFEMPTY_BIT		0x00000800
#define FFQFULL_BIT		0x00001000
#define FFALMFULL_BIT	0x00002000
#define FFOVERFLOW_BIT	0x00004000
		
#define WDTE_ERR_BIT	0x01000000
#define RDTE_ERR_BIT	0x02000000
#define DTE_DONE_BIT	0x80000000
		
#define MAJOR_VERSION	1
#define MINOR_VERSION	5 

		//driver status bits
#define READYFORDATA	0x00000001
#define WORKERACTIVE	0x00000002
		
#define BYTESPERPIXEL	8  
		
// Constants
#define MAXBUFSIZE	1000000  //1 MB ?
		
#define READ	0
#define WRITE	1	


//==============================================================================
// Types
		
		


//==============================================================================
// External variables

//==============================================================================
// Global functions

int ReadPMTReg(unsigned long regaddress,unsigned long *regval);
int WritePMTReg(unsigned long regaddress,unsigned long regvalue);
int PMTController_Init(void);
int PMTController_Finalize(void);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __HW_VUPC_H__ */
