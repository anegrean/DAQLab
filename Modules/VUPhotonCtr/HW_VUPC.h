//==============================================================================
//
// Title:		HW_VUPC.h
// Purpose:		A short description of the interface.
//
// Created on:	2-7-2014 at 14:31:24 by Lex van der Gracht.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __HW_VUPC_H__
#define __HW_VUPC_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "VUPhotonCtr.h"
#include "NIDAQmxManager.h" 
		

//==============================================================================
// Constants
		
		//instrument registers
#define	 CTRL_REG		0x00B0		//0x0000
#define	 STAT_REG		0x00B4		//0x0004
#define	 VERS_REG		0x00B8		//0x0008
#define	 ERROR_REG		0x00BC		//0x000C
#define	 PMT1_CTRL_REG	0x00C0		//0x0010
#define	 PMT2_CTRL_REG	0x00C4		//0x0014
#define	 PMT3_CTRL_REG	0x00C8		//0x0018
#define	 PMT4_CTRL_REG	0x00CC		//0x001C
			 
		
		//control register bits
#define	APPSTART_BIT	0x00000001
//#define	DMASTART_BIT	0x00000002
#define	UPDPMT12_BIT	0x00000020
#define	UPDPMT34_BIT	0x00000040
#define	RESET_BIT		0x00000080 
//#define	FFRESET_BIT		0x00000100
#define	TESTMODE0_BIT	0x00004000
#define	TESTMODE1_BIT	0x00008000  		
#define	PMT1HV_BIT		0x00010000		
#define	PMT1PELT_BIT	0x00020000		
#define	PMT1FAN_BIT		0x00080000
#define	PMT1EN_BIT		0x80000000
#define	PMT2HV_BIT		0x00100000		
#define	PMT2PELT_BIT	0x00200000		
#define	PMT2FAN_BIT		0x00400000
#define	PMT2EN_BIT		0x00800000
#define	PMT3HV_BIT		0x01000000		
#define	PMT3PELT_BIT	0x02000000		
#define	PMT3FAN_BIT		0x04000000
#define	PMT3EN_BIT		0x08000000
#define	PMT4HV_BIT		0x10000000		
#define	PMT4PELT_BIT	0x20000000		
#define	PMT4FAN_BIT		0x40000000
#define	PMT4EN_BIT		0x80000000
		
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
#define DOOROPEN_BIT	0x00000200
//#define FFUNDRFL_BIT	0x00000400		
//#define FFEMPTY_BIT		0x00001000
//#define FFQFULL_BIT		0x00001000
#define FFALMFULL_BIT	0x00002000
#define FFOVERFLOW_BIT	0x00004000
#define COVERFLOW_BIT	0x00008000
		
#define PMT1	1
#define PMT2	2 
#define PMT3	3 
#define PMT4	4 

		
#define READ	0
#define WRITE	1	
		
// Maximum number of measurement channels
#define MAX_CHANNELS				4

	// Maximum gain applied to a PMT in [V]
#define MAX_GAIN_VOLTAGE		0.9
	
// definitions for ERC-PhotonCounter
#define STATREG		"STAT"
#define CTRLREG 	"CTRL"
#define PMT1THRES 	"PM1T"
#define PMT1OFFSET 	"PM1O"  
#define PMT1GAIN 	"PM1G"
#define PMT2THRES 	"PM2T"
#define PMT2OFFSET 	"PM2O"  
#define PMT2GAIN 	"PM2G"
#define PMT3THRES 	"PM3T"
#define PMT3OFFSET 	"PM3O"  
#define PMT3GAIN 	"PM3G"
#define PMT4THRES 	"PM4T"
#define PMT4OFFSET 	"PM4O"  
#define PMT4GAIN 	"PM4G"


//==============================================================================
// Types
typedef enum {
	PMT_LED_OFF,
	PMT_LED_ON,
	PMT_LED_ERROR
} PMT_LED_type;
	
typedef enum {
	PMT_MODE_OFF,
	PMT_MODE_ON,
	PMT_MODE_ON_ACQ
} PMT_Mode_type;



//==============================================================================
// External variables

//==============================================================================
// Global functions

int 		ReadPMTReg					(unsigned long regaddress, unsigned long *regval);
int 		WritePMTReg					(unsigned long regaddress, unsigned long regvalue);
int 		PMTController_Init			(void);
int 		PMTController_Finalize		(void);
int 		PMTReset					(void);
int 		PMTClearFifo				(void);
int 		PMT_SetMode 				(int PMTnr, PMT_Mode_type mode);
int 		PMT_SetFan 					(int PMTnr, BOOL value);
int 		PMT_SetCooling 				(int PMTnr, BOOL value);
int 		SetPMTTresh				    (int PMTnr, double threshold);
int 		SetPMTGain				    (int PMTnr, double gain);  
int 		PMT_SetTestMode				(BOOL testmode);
int 		PMT_ClearControl			(int PMTnr);
void 		ResetDataCounter			(void);
int 		GetDataCounter				(void);
int 		PMTStartAcq					(TaskMode_type mode, TaskControl_type* taskControl, double samplingRate, Channel_type** channels);
int 		PMTStopAcq					(void);
void 		SetNrSamples				(int numsamples);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __HW_VUPC_H__ */
