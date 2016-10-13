//==============================================================================
//
// Title:		DataStorage.h
// Purpose:		Used for writing data to disk.
//
// Created on:	12-9-2014 at 12:08:22 by Lex van der Gracht and Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
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

#define MOD_DataStorage_NAME 		"Data Storage"
	



//==============================================================================
// Types

typedef struct DataStorage		DataStorage_type;
typedef struct DS_Channel		DS_Channel_type;


struct DS_Channel {
	DataStorage_type*	dsInstance;	    // reference to device that owns the channel
	SinkVChan_type*		VChan;			// virtual channel assigned to this module
	int					panHndl;		// panel handle to keep track of controls
//	int 				iteration;		// local iteration counter (?)
	
	// METHODS
	void (*Discard) 	(DS_Channel_type** channel);
};



							    

//==============================================================================
// Global functions


DAQLabModule_type*	initalloc_DataStorage 	(DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl);
void 				discard_DataStorage    	(DAQLabModule_type** mod);



#ifdef __cplusplus
    }
#endif


#endif  /* ndef __DataStorage_H__ */
