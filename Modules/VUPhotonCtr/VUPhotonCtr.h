//==============================================================================
//
// Title:		VUPhotonCtr.h
// Purpose:		A short description of the interface.
//
// Created on:	17-6-2014 at 20:51:00 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __VUPhotonCtr_H__
#define __VUPhotonCtr_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "DAQLabModule.h"

//==============================================================================
// Constants

#define MOD_VUPhotonCtr_NAME 		"VU Photon Counter"
		


//==============================================================================
// Types

typedef struct VUPhotonCtr	VUPhotonCtr_type;   






//==============================================================================
// Global functions


DAQLabModule_type*	initalloc_VUPhotonCtr 	(DAQLabModule_type* mod, char className[], char instanceName[]);
void 				discard_VUPhotonCtr 	(DAQLabModule_type** mod);



#ifdef __cplusplus
    }
#endif

#endif  /* ndef __VUPhotonCtr_H__ */
