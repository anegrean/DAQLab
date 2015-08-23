//==============================================================================
//
// Title:		Module_Header.h
// Purpose:		It includes header files for required modules..
//
// Created on:	16-7-2015 at 22:23:04 by Vinod Nigade.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __Module_Header_H__
#define __Module_Header_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files
		
/* 
 * Include only required module. Define compiler build option in the format
 * DAQLabModule_<module_name>
 */

//------------------------------------------------------
// PIStage Z stage
//------------------------------------------------------
#ifdef DAQLabModule_PIStage
	#include "PIStage.h"
#else		// PIStage not defined
	#define	MOD_PIStage_NAME NULL
	#define	initalloc_PIStage NULL
#endif		

//------------------------------------------------------
// LangLStep XY stage
//------------------------------------------------------
#ifdef DAQLabModule_LangLStep
	#include "LangLStep.h"
#else		// not defined
	#define	MOD_LangLStep_NAME	NULL
	#define	initalloc_LangLStep	NULL
#endif		

//------------------------------------------------------
// VU Photon Counter
//------------------------------------------------------
#ifdef DAQLabModule_VUPhotonCtr
	#include "VUPhotonCtr.h"		  
#else		// not defined
	#define	MOD_VUPhotonCtr_NAME NULL
	#define	initalloc_VUPhotonCtr NULL
#endif		

//------------------------------------------------------
// Pockells cell modulator
//------------------------------------------------------
#ifdef DAQLabModule_Pockells
	#include "Pockells.h"
#else		// not defined
	#define MOD_Pockells_NAME NULL
	#define initalloc_PockellsModule NULL
#endif

//------------------------------------------------------
// NI DAQmx support
//------------------------------------------------------
#ifdef DAQLabModule_NIDAQmxManager
	#include "NIDAQmxManager.h"		   
#else		// not defined
	#define	MOD_NIDAQmxManager_NAME NULL
	#define	initalloc_NIDAQmxManager NULL
#endif

//------------------------------------------------------
// Laser scanning
//------------------------------------------------------
#ifdef DAQLabModule_LaserScanning
	#include "\Modules\Laser Scanning\LaserScanning.h"
#else		// not defined
	#define	MOD_LaserScanning_NAME NULL
	#define	initalloc_LaserScanning NULL
#endif

//------------------------------------------------------
// Whisker Module
//------------------------------------------------------
#ifdef DAQLabModule_Whisker
#include "Whisker.h"
#else		// Whisker not defined
#define	MOD_Whisker_Name NULL
#define initalloc_WhiskerModule NULL
#endif		

//------------------------------------------------------
// Data storage (will be part of framework)
//------------------------------------------------------
#ifdef DAQLabModule_DataStorage
#include "DataStorage.h"
#else		// DataStorage not defined 
#define	MOD_DataStorage_NAME NULL
#define	initalloc_DataStorage NULL
#endif		

//------------------------------------------------------
// Coherent Chameleon Laser
//------------------------------------------------------
#ifdef DAQLabModule_CoherentCham
	#include "CoherentCham.h"
#else		// not defined
	#define	MOD_CoherentCham_NAME NULL
	#define	initalloc_CoherentCham NULL
#endif	


#ifdef __cplusplus
    }
#endif

#endif  /* ndef __Module_Header_H__ */
