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
#ifdef DAQLabModule_PIStage
#include "PIStage.h"
#else		/* PIStage not defined */
#define	MOD_PIStage_NAME NULL
#define	initalloc_PIStage NULL
#endif		/* PIStage */

#ifdef DAQLabModule_LangLStep
#include "LangLStep.h"
#else		/* LangLStep not defined */
#define	MOD_LangLStep_NAME	NULL
#define	initalloc_LangLStep	NULL
#endif		/* LangLstep */
		
#ifdef DAQLabModule_VUPhotonCtr
#include "VUPhotonCtr.h"		  
#else		/* VUPhoton not defined */
#define	MOD_VUPhotonCtr_NAME NULL
#define	initalloc_VUPhotonCtr NULL
#endif		/* VUPhoton */
		
#ifdef DAQLabModule_Pockells
#include "Pockells.h"
#else		/* Pockells not defined */
#define MOD_Pockells_NAME NULL
#define initalloc_PockellsModule NULL
#endif		/* Pockells */
		
#ifdef DAQLabModule_NIDAQmxManager
#include "NIDAQmxManager.h"		   
#else		/* NIDAQmxManager not defined */
#define	MOD_NIDAQmxManager_NAME NULL
#define	initalloc_NIDAQmxManager NULL
#endif		/* NIDAQmxManager */
		
#ifdef DAQLabModule_LaserScanning
#include "\Modules\Laser Scanning\LaserScanning.h"
#else		/* LaserScanning not defined */ 
#define	MOD_LaserScanning_NAME NULL
#define	initalloc_LaserScanning NULL
#endif		/* LaserScanning */

#ifdef DAQLabModule_DataStorage
#include "DataStorage.h"
#else		/* DataStorage not defined */ 
#define	MOD_DataStorage_NAME NULL
#define	initalloc_DataStorage NULL
#endif		/* LaserScanning */

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __Module_Header_H__ */
