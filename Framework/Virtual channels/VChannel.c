//==============================================================================
//
// Title:		VChannel.c
// Purpose:		A short description of the implementation.
//
// Created on:	18-6-2014 at 19:11:05 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "toolbox.h"
#include <ansi_c.h>
#include "VChannel.h"

//==============================================================================
// Constants

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}  

//==============================================================================
// Types

// discard function pointer type
typedef void	(* DiscardFptr_type) 	(VChan_type** vchan);

//---------------------------------------------------------------------------------------------------
// Base VChan
//---------------------------------------------------------------------------------------------------

struct VChan {
	
	//-----------------------
	// Identification
	//-----------------------
	
	char*					name;			// Name of virtual chanel. 
	VChanData_type			dataType;		// Type of data which goes through the channel.
	VChanDataFlow_type 		dataFlow;   	// Direction of data flow into or out of the channel.
	
	//-----------------------	
	// Source code connection
	//-----------------------
	void*					vChanOwner;		// Reference to object that owns the VChan.
	
	//-----------------------
	// Methods
	//-----------------------
	
	DiscardFptr_type		DiscardFptr;	// Discards VChan type-specific data.  
	
};

//---------------------------------------------------------------------------------------------------
// Sink VChan
//---------------------------------------------------------------------------------------------------

struct SinkVChan {
	
	//-----------------------
	// Base class
	//-----------------------
	
	VChan_type				baseClass;		// Must be first member to allow inheritance from parent
	
	//-----------------------	
	// Communication
	//-----------------------
	
	SourceVChan_type*		sourceVChan;	// SourceVChan attached to this sink.
	CmtTSQHandle       		tsqHndl; 		// Thread safe queue handle to receive incoming data.
	
};

//---------------------------------------------------------------------------------------------------
// Source VChan
//---------------------------------------------------------------------------------------------------

struct SourceVChan {
	
	//-----------------------
	// Base class
	//-----------------------
	
	VChan_type				baseClass;		// must be first member to allow inheritance from parent
	
	//-----------------------	
	// Communication
	//-----------------------
	
	ListType				sinkVChans;		// Connected Sink VChans. List of SinkVChan_type*
											
};

//==============================================================================
// Static functions

static int 			init_VChan_type 			(VChan_type* vchan, char name[], VChanData_type dataType, 
												 VChanDataFlow_type flowType, void* vChanOwner, DiscardFptr_type DiscardFptr);

static void 		discard_SourceVChan_type 	(VChan_type** vchan);

static void 		discard_SinkVChan_type 		(VChan_type** vchan); 



static int init_VChan_type (VChan_type* 			vchan, 
							char 					name[], 
							VChanData_type 			dataType, 
							VChanDataFlow_type 		flowType,
							void*					vChanOwner,
							DiscardFptr_type 		DiscardFptr)
{
	// Data
	
	vchan -> name   				= StrDup(name); 
	if (!vchan->name) return -1;
	vchan -> dataType				= dataType;
	vchan -> dataFlow				= flowType;
	vchan -> vChanOwner				= vChanOwner;
	
	// Methods
	
	vchan -> DiscardFptr			= DiscardFptr;
	
	
	return 0;
}

static void discard_SourceVChan_type (VChan_type** vchan)
{
	SourceVChan_type* 		srcPtr 	= *(SourceVChan_type**) vchan;
	SinkVChan_type**		sinkPtrPtr;
	
	if (!*vchan) return;
	
	// disconnect Source from Sinks if there are any connected
	for (int i = 1; i <= ListNumItems(srcPtr->sinkVChans); i++ ){
		sinkPtrPtr = ListGetPtrToItem(srcPtr->sinkVChans, i);
		// call the Sink's disconnect function callback  
		
	}
	
	// discard Source VChan specific data 
	
	// discard base VChan data
	OKfree((*vchan)->name);
	
	OKfree(*vchan);
}

static void discard_SinkVChan_type (VChan_type** vchan)
{
	SinkVChan_type* 		sinkVChanPtr 	= *(SinkVChan_type**) vchan;
	
	if (!*vchan) return;
	
	// disconnect Sink from Source if connected
	
	// discard Sink VChan specific data    
	
	// discard base VChan data
	OKfree((*vchan)->name);
	
	OKfree(*vchan);
}

//==============================================================================
// Static global variables


//==============================================================================
// Global functions


SourceVChan_type* init_SourceVChan_type	(char 						name[], 
										 VChanData_type 			dataType,
										 void* 						vChanOwner)
{
	SourceVChan_type*	vchan 	= malloc(sizeof(SourceVChan_type));
	if (!vchan) return NULL;
	
	// init base VChan type
	if (init_VChan_type ((VChan_type*) vchan, name, dataType, VCHAN_SOURCE, vChanOwner, discard_SourceVChan_type) < 0) goto Error;
	
	// init list with Sink VChans
	if (!(vchan -> sinkVChans 	= ListCreate(sizeof(SinkVChan_type*)))) goto Error;  
	
	
	return vchan;
	Error:
	
	discard_VChan_type ((VChan_type**)&vchan);  // do this last
	return NULL;
}

SinkVChan_type* init_SinkVChan_type	(char 						name[], 
									 VChanData_type 			dataType,
									 void* 						vChanOwner,
									 int 						queueSize,
									 size_t						dataSize)
{
	SinkVChan_type*	vchan = malloc(sizeof(SinkVChan_type));
	if (!vchan) return NULL;
	
	// init base VChan type
	if (init_VChan_type ((VChan_type*) vchan, name, dataType, VCHAN_SINK, vChanOwner, discard_SinkVChan_type) < 0) goto Error;
	
		// INIT DATA
		
	vchan->sourceVChan = NULL;
	// init thread safe queue
	CmtNewTSQ(queueSize, dataSize, 0, &vchan->tsqHndl); 
	
	
	
	return vchan;
	Error:
	
	discard_VChan_type ((VChan_type**)&vchan);  // do this last
	return NULL;
	
}

void discard_VChan_type (VChan_type** vchan)
{
	if (!*vchan) return;
	
	// call discard function specific to the VChan type
	(*(*vchan)->DiscardFptr)	(vchan);
}

//==============================================================================
// Set/Get functions


 
