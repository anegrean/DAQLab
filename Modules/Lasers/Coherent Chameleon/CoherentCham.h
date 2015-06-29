//==============================================================================
//
// Title:		CoherentCham.h
// Purpose:		A short description of the interface.
//
// Created on:	5/17/2015 at 12:15:00 AM by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __ChameleonTiSa_H__
#define __ChameleonTiSa_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "cvidef.h"

//==============================================================================
// Constants
		
#define MOD_CoherentCham_NAME 	"Coherent Chameleon TiSa laser" 

//==============================================================================
// Types

//==============================================================================
// External variables

//==============================================================================
// Global functions
		
DAQLabModule_type*		initalloc_CoherentCham 			(DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl);
void 					discard_CoherentCham	 		(DAQLabModule_type** mod);


#endif  /* ndef __ChameleonTiSa_H__ */  
