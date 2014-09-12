//==============================================================================
//
// Title:		DataStorage.h
// Purpose:		A short description of the interface.
//
// Created on:	12-9-2014 at 12:08:22 by Systeembeheer.
// Copyright:	IT-Groep FEW, Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __DataStorage_H__
#define __DataStorage_H__


#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files
#include "DAQLabModule.h"

//==============================================================================
// Constants

#define MOD_DataStore_NAME 		"Data Storage"



//==============================================================================
// Types

typedef struct DatStore	DataStorage_type;


//==============================================================================
// Global functions


DAQLabModule_type*	initalloc_DataStorage 	(DAQLabModule_type* mod, char className[], char instanceName[]);
void 				discard_DataStorage    	(DAQLabModule_type** mod);



#ifdef __cplusplus
    }
#endif


#endif  /* ndef __DataStorage_H__ */
