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
		
#define MAX_DS_CHANNELS			4  //test value



//==============================================================================
// Types

typedef struct DatStore	DataStorage_type;

typedef struct {
	DataStorage_type*	dsInstance;	    // reference to device that owns the channel
	SinkVChan_type*		VChan;			// virtual channel assigned to this module
	int					panHndl;		// panel handle to keep track of controls
	size_t			   	chanIdx;		// 1-based channel index 
	size_t				iterationnr;	//test, to keep track of number of iterations 
} DS_Channel_type;


//==============================================================================
// Global functions


DAQLabModule_type*	initalloc_DataStorage 	(DAQLabModule_type* mod, char className[], char instanceName[]);
void 				discard_DataStorage    	(DAQLabModule_type** mod);



#ifdef __cplusplus
    }
#endif


#endif  /* ndef __DataStorage_H__ */
