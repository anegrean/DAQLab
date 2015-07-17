//==============================================================================
//
// Title:		DataStorage.h
// Purpose:		A short description of the interface.
//
// Created on:	12-9-2014 at 12:08:22 by Lex van der Gracht.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
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

typedef struct DatStore		DataStorage_type;
typedef struct DS_Channel	DS_Channel_type;


struct DS_Channel{
	DataStorage_type*	dsInstance;	    // reference to device that owns the channel
	SinkVChan_type*		VChan;			// virtual channel assigned to this module
	int					panHndl;		// panel handle to keep track of controls
//	int 				iteration;		// local iteration counter (?)
	// METHODS
	void (*Discard) 	(DS_Channel_type** channel);
} ;



							    

//==============================================================================
// Global functions


DAQLabModule_type*	initalloc_DataStorage 	(DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl);
void 				discard_DataStorage    	(DAQLabModule_type** mod);



#ifdef __cplusplus
    }
#endif


#endif  /* ndef __DataStorage_H__ */
