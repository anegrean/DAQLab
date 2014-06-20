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

/*
// discard function pointer type
typedef void	(* DiscardFptr_type) 		(VChan_type** vchan);
typedef BOOL	(* DisconnectFptr_type)	   	(VChan_type* vchan);

//---------------------------------------------------------------------------------------------------
// Base VChan
//---------------------------------------------------------------------------------------------------

struct VChan {
	
	//-----------------------
	// Identification
	//-----------------------
	
	char*						name;					// Name of virtual chanel. 
	VChanData_type				dataType;				// Type of data which goes through the channel.
	VChanDataFlow_type 			dataFlow;   			// Direction of data flow into or out of the channel.
														
	//-----------------------	
	// Source code connection
	//-----------------------
	void*						vChanOwner;				// Reference to object that owns the VChan.
	
	//-----------------------
	// Methods
	//-----------------------
	
	DiscardFptr_type			DiscardFptr;			// Discards VChan type-specific data. 
	
	DisconnectFptr_type			DisconnectFptr;			// Disconnects a VChan.
	
	//-----------------------
	// Callbacks
	//-----------------------
	
	Connected_CBFptr_type		Connected_CBFptr;		// Callback when another VChan has been connected to this one.
	
	Disconnected_CBFptr_type	Disconnected_CBFptr;   	// Callback when another VChan has been disconnected from this one.
	
}; */

//---------------------------------------------------------------------------------------------------
// Sink VChan
//---------------------------------------------------------------------------------------------------

struct SinkVChan {
	
	//-----------------------
	// Base class
	//-----------------------
	
	VChan_type					baseClass;				// Must be first member to allow inheritance from parent
	
	//-----------------------	
	// Communication
	//-----------------------
	
	SourceVChan_type*			sourceVChan;			// SourceVChan attached to this sink.
	CmtTSQHandle       			tsqHndl; 				// Thread safe queue handle to receive incoming data.
	
};

//---------------------------------------------------------------------------------------------------
// Source VChan
//---------------------------------------------------------------------------------------------------

struct SourceVChan {
	
	//-----------------------
	// Base class
	//-----------------------
	
	VChan_type					baseClass;				// must be first member to allow inheritance from parent
	
	//-----------------------	
	// Communication
	//-----------------------
	
	ListType					sinkVChans;				// Connected Sink VChans. List of SinkVChan_type*
											
};

//==============================================================================
// Static functions

static int 	init_VChan_type 			(VChan_type* 				vchan, 
										 char 						name[], 
										 VChanData_type 			dataType, 
										 VChanDataFlow_type 		flowType, 
										 void* 						vChanOwner, 	
										 DiscardFptr_type 			DiscardFptr,
										 DisconnectFptr_type		DisconnectFptr,
										 Connected_CBFptr_type		Connected_CBFptr,
										 Disconnected_CBFptr_type	Disconnected_CBFptr)
{
	// Data
	vchan -> name   				= StrDup(name); 
	if (!vchan->name) return -1;
	vchan -> dataType				= dataType;
	vchan -> dataFlow				= flowType;
	vchan -> vChanOwner				= vChanOwner;
	
	// Callbacks
	vchan -> Connected_CBFptr		= Connected_CBFptr;
	vchan -> Disconnected_CBFptr	= Disconnected_CBFptr;
	
	// Methods
	vchan -> DiscardFptr			= DiscardFptr;
	vchan -> DisconnectFptr			= DisconnectFptr;
	
	return 0;
}

static void discard_SourceVChan_type (VChan_type** vchan)
{
	SourceVChan_type* 		src 		= *(SourceVChan_type**) vchan;
	
	if (!*vchan) return;
	
	// disconnect source from its sinks if there are any
	VChan_Disconnect(*vchan);
	
	// discard Source VChan specific data
	ListDispose(src->sinkVChans);
	
	// discard base VChan data
	OKfree((*vchan)->name);
	
	OKfree(*vchan);
}

static void discard_SinkVChan_type (VChan_type** vchan)
{
	SinkVChan_type* 		sink 		= *(SinkVChan_type**) vchan;
	
	if (!*vchan) return;
	
	// disconnect Sink from Source if connected
	VChan_Disconnect(*vchan);  
	
	// discard Sink VChan specific data 
	CmtDiscardTSQ(sink->tsqHndl);
	
	// discard base VChan data
	OKfree((*vchan)->name);
	
	OKfree(*vchan);
}

static BOOL disconnectSourceVChan (VChan_type* vchan)
{
	if (!vchan) return FALSE;
	
	SourceVChan_type* 	src 		= (SourceVChan_type*) vchan; 
	SinkVChan_type**	sinkPtrPtr;
	
	for (int i = 1; i <= ListNumItems(src->sinkVChans); i++) {
		sinkPtrPtr = ListGetPtrToItem(src->sinkVChans, i);
		(*sinkPtrPtr)->sourceVChan = NULL;
		// call sink disconnected callbacks
		if ((*sinkPtrPtr)->baseClass.Disconnected_CBFptr)
			(*(*sinkPtrPtr)->baseClass.Disconnected_CBFptr)	((VChan_type*)(*sinkPtrPtr), (VChan_type*) src);
		
		// call source disconnected callback
		if (src->baseClass.Disconnected_CBFptr)
			(*src->baseClass.Disconnected_CBFptr)	((VChan_type*)src, (VChan_type*)(*sinkPtrPtr));
	}
	
	ListClear(src->sinkVChans);
	
	return TRUE;
}

static BOOL disconnectSinkVChan (VChan_type* vchan)
{
	SinkVChan_type* 		sink			= (SinkVChan_type*) vchan; 
	SinkVChan_type**		sinkPtrPtr;
	SourceVChan_type*		src				= sink->sourceVChan;
	
	// remove source from sink
	sink->sourceVChan = NULL;
	
	// call sink disconnected callback
	if (sink->baseClass.Disconnected_CBFptr)
			(*sink->baseClass.Disconnected_CBFptr)	((VChan_type*)sink, (VChan_type*) src);
	
	// remove sink from source's sink list
	for (int i = 1; i <= ListNumItems(src->sinkVChans); i++) {
		sinkPtrPtr = ListGetPtrToItem(src->sinkVChans, i);
		if (*sinkPtrPtr == sink) {
			ListRemoveItem(src->sinkVChans, 0, i);
			// call source disconnected callback
			if (src->baseClass.Disconnected_CBFptr)
				(*src->baseClass.Disconnected_CBFptr)	((VChan_type*)src, (VChan_type*)(*sinkPtrPtr));
			return TRUE;
		}
	}
	
	return FALSE;
}

//==============================================================================
// Static global variables


//==============================================================================
// Global functions


SourceVChan_type* init_SourceVChan_type	(char 						name[], 
										 VChanData_type 			dataType,
										 void* 						vChanOwner,
										 Connected_CBFptr_type		Connected_CBFptr,
										 Disconnected_CBFptr_type	Disconnected_CBFptr)
{
	SourceVChan_type*	vchan 	= malloc(sizeof(SourceVChan_type));
	if (!vchan) return NULL;
	
	// init base VChan type
	if (init_VChan_type ((VChan_type*) vchan, name, dataType, VCHAN_SOURCE, vChanOwner, 
						 discard_SourceVChan_type, disconnectSourceVChan, Connected_CBFptr, Disconnected_CBFptr) < 0) goto Error;
	
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
									 size_t						dataSize,
									 Connected_CBFptr_type		Connected_CBFptr,
									 Disconnected_CBFptr_type	Disconnected_CBFptr)
{
	SinkVChan_type*	vchan = malloc(sizeof(SinkVChan_type));
	if (!vchan) return NULL;
	
	// init base VChan type
	if (init_VChan_type ((VChan_type*) vchan, name, dataType, VCHAN_SINK, vChanOwner, 
						 discard_SinkVChan_type, disconnectSourceVChan, Connected_CBFptr, Disconnected_CBFptr) < 0) goto Error;
	
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

/// HIFN Connects a Source and Sink VChan and if provided, calls their Connected_CBFptr callbacks to signal this event.
/// HIFN The Source VChan's Connected_CBFptr will be called before the Sink's Connected_CBFptr. 
/// HIRET TRUE if successful, FALSE otherwise
BOOL VChan_Connect (SourceVChan_type* source, SinkVChan_type* sink)
{
	SinkVChan_type** sinkPtrPtr;
	
	if (!source || !sink) return FALSE;
	// check if sink is already connected
	for (int i = 1; i <= ListNumItems(source->sinkVChans); i++) {
		sinkPtrPtr = ListGetPtrToItem(source->sinkVChans, i);
		if (*sinkPtrPtr == sink) return FALSE;
	}
	// if sink is already connected to another source, disconnect that sink from source
	if (sink->sourceVChan) 
		if(!VChan_Disconnect((VChan_type*)sink) ) return FALSE;
		
	// add sink to source's list of sinks
	if (!ListInsertItem(source->sinkVChans, &sink, END_OF_LIST)) return FALSE;
	// add source reference to sink
	sink->sourceVChan = source;
	
	// call source and sink connect callbacks if provided
	if (source->baseClass.Connected_CBFptr)
		(*source->baseClass.Connected_CBFptr)	((VChan_type*)source, (VChan_type*) sink);
	
	if (sink->baseClass.Connected_CBFptr)
		(*sink->baseClass.Connected_CBFptr)	((VChan_type*)sink, (VChan_type*)source);
	
	return TRUE;	
}

/// HIFN Disconnects a VChan and if provided, calls its Disconnected_CBFptr callback to signal this event.
/// HIFN When disconnecting a Sink, its Source is detached and it is also removed from the Source's list of Sinks.
/// HIFN When disconnecting a Source, it is removed from all its Sinks.
/// HIRET TRUE if successful, FALSE otherwise
BOOL VChan_Disconnect (VChan_type* vchan)
{
	return (*vchan->DisconnectFptr)	(vchan);
}


//==============================================================================
// Set/Get functions

void SetVChanName (VChan_type* vchan, char newName[])
{ 
	OKfree(vchan->name);
	vchan->name = StrDup(newName);
}
 
char* GetVChanName (VChan_type* vchan)
{
	return StrDup(vchan->name);
}

