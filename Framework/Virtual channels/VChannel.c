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
#include "DAQLabUtility.h"
#include <ansi_c.h>
#include <formatio.h> 
#include "toolbox.h"
#include "utility.h"
#include "VChannel.h"
#include "DataPacket.h"

//==============================================================================
// Constants

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}  
#define DEFAULT_SinkVChan_QueueSize			1000
#define DEFAULT_SinkVChan_QueueWriteTimeout	1000.0	// number of [ms] to wait while trying to add a data packet to a Sink VChan TSQ

// Cmt library error macro
#define CmtErrChk(fCall) if (errorInfo.error = (fCall), errorInfo.line = __LINE__, errorInfo.error < 0) \ 
{goto CmtError;} else

// obtain Cmt error description and jumps to Error
#define Cmt_ERR { \
	if (errorInfo.error < 0) { \
		char CmtErrMsgBuffer[CMT_MAX_MESSAGE_BUF_SIZE] = ""; \
		errChk( CmtGetErrorMessage(errorInfo.error, CmtErrMsgBuffer) ); \
		nullChk( errorInfo.errMsg = StrDup(CmtErrMsgBuffer) ); \
	} \
	goto Error; \
}


//==============================================================================
// Types

// Discard VChan
typedef void	(* DiscardVChanFptr_type) 			(VChan_type** VChanPtr);

// Disconnect VChan
typedef BOOL	(* DisconnectVChanFptr_type)		(VChan_type* VChan);

// Checks if a VChan is connected to other VChans
typedef BOOL 	(* VChanIsConnectedFptr_type)		(VChan_type* VChan);  


//==============================================================================
// Globals

//---------------------------------------------------------------------------------------------------
// Base VChan
//---------------------------------------------------------------------------------------------------

struct VChan {
	
	//-----------------------
	// Identification
	//-----------------------
	
	char*							name;					// Name of virtual chanel. 
	VChanDataFlows 					dataFlow;   			// Direction of data flow into or out of the channel.
	BOOL							isActive;				// If True, the VChan is required by the module, False otherwise. Default: True.
	BOOL							isOpen;					// If True, data can be sent to/received from the VChan, False otherwise.
														
	//-----------------------	
	// Source code connection
	//-----------------------
	void*							VChanOwner;				// Reference to object that owns the VChan.
	
	//-----------------------
	// Methods
	//-----------------------
	
	DiscardVChanFptr_type			DiscardFptr;			// Discards VChan type-specific data. 
	
	DisconnectVChanFptr_type		DisconnectFptr;			// Disconnects a VChan.
	
	VChanIsConnectedFptr_type		VChanIsConnectedFptr;   // Determines if a VChan is connected to another VChan
	
	//-----------------------
	// Callbacks
	//-----------------------
	
	VChanStateChangeCBFptr_type		VChanStateChangeCBFptr; // Callback when a VChan opens/closes
	
};

//---------------------------------------------------------------------------------------------------
// Sink VChan
//---------------------------------------------------------------------------------------------------

struct SinkVChan {
	
	//-----------------------
	// Base class
	//-----------------------
	
	VChan_type						baseClass;				// Must be first member to allow inheritance from parent
	
	//-----------------------	
	// Data
	//-----------------------
	
	DLDataTypes*					dataTypes;				// Array of packet data types of DLDataTypes that the sink may receive.
	size_t							nDataTypes;				// Number of data types that the Sink VChan supports.
	double							readTimeout;			// Timeout in [ms] to wait for data to be available in the TSQ.
	double							writeTimeout;			
	SourceVChan_type*				sourceVChan;			// SourceVChan attached to this sink.
	CmtTSQHandle       				tsqHndl; 				// Thread safe queue handle to receive incoming data.
	
	
};

//---------------------------------------------------------------------------------------------------
// Source VChan
//---------------------------------------------------------------------------------------------------

struct SourceVChan {
	
	//-----------------------
	// Base class
	//-----------------------
	
	VChan_type						baseClass;				// Must be first member to allow inheritance from parent
	
	//-----------------------	
	// Data
	//-----------------------
	
	DLDataTypes						dataType;				// Type of data packet which goes through the channel.
	ListType						sinkVChans;				// Connected Sink VChans. List of SinkVChan_type*
											
};


//==============================================================================
// Static functions

static BOOL 				SourceVChanIsConnected 				(SourceVChan_type* srcVChan);
static BOOL 				SinkVChanIsConnected 				(SinkVChan_type* sinkVChan);
static size_t				GetNumActiveSinkVChans				(SourceVChan_type* srcVChan);
static size_t				GetNumOpenSinkVChans				(SourceVChan_type* srcVChan);


static BOOL SourceVChanIsConnected (SourceVChan_type* srcVChan)
{
	if (ListNumItems(srcVChan->sinkVChans))
		return TRUE;
	else
		return FALSE;
}
	
static BOOL SinkVChanIsConnected (SinkVChan_type* sinkVChan)
{
	if (sinkVChan->sourceVChan)
		return TRUE;
	else
		return FALSE;
}

/// HIFN Returns the number of active Sink VChans connected to a Source VChan.
static size_t GetNumActiveSinkVChans (SourceVChan_type* srcVChan)
{
	SinkVChan_type*		sinkVChan 				= NULL;
	size_t				nSinkVChans				= ListNumItems(srcVChan->sinkVChans);
	size_t				nActiveSinkVChans		= 0;
	
	for (size_t i = 1; i <= nSinkVChans; i++) {
		sinkVChan = *(SinkVChan_type**)ListGetPtrToItem(srcVChan->sinkVChans, i);
		if (sinkVChan->baseClass.isActive)
			nActiveSinkVChans++;
	}
	
	return nActiveSinkVChans;
}

static size_t GetNumOpenSinkVChans (SourceVChan_type* srcVChan)
{
	SinkVChan_type*		sinkVChan 				= NULL;
	size_t				nSinkVChans				= ListNumItems(srcVChan->sinkVChans);
	size_t				nOpenSinkVChans			= 0;
	
	for (size_t i = 1; i <= nSinkVChans; i++) {
		sinkVChan = *(SinkVChan_type**)ListGetPtrToItem(srcVChan->sinkVChans, i);
		if (sinkVChan->baseClass.isOpen)
			nOpenSinkVChans++;
	}
	
	return nOpenSinkVChans;
}


static int 					init_VChan_type 					(VChan_type* 					vchan, 
																char 							name[], 
										 						VChanDataFlows 					flowType, 
														 		void* 							VChanOwner, 	
										 				 		DiscardVChanFptr_type 			DiscardFptr,
										 				 		DisconnectVChanFptr_type		DisconnectFptr,
																VChanStateChangeCBFptr_type		VChanStateChangeCBFptr,
																VChanIsConnectedFptr_type		VChanIsConnectedFptr)
{
	// Data
	vchan->name   					= StrDup(name); 
	if (!vchan->name) return -1;
	vchan->dataFlow					= flowType;
	vchan->isActive					= TRUE;
	vchan->isOpen					= FALSE;
	
	vchan->VChanOwner				= VChanOwner;
	
	// Callbacks
	vchan->VChanStateChangeCBFptr	= VChanStateChangeCBFptr;
	
	// Methods
	vchan->DiscardFptr				= DiscardFptr;
	vchan->DisconnectFptr			= DisconnectFptr;
	vchan->VChanIsConnectedFptr		= VChanIsConnectedFptr;
	
	return 0;
}

static void discard_SourceVChan_type (SourceVChan_type** srcVChanPtr)
{
	SourceVChan_type* 		srcVChan	= *srcVChanPtr;
	
	if (!srcVChan) return;
	
	// disconnect source from its sinks if there are any
	VChan_Disconnect((VChan_type*)srcVChan);
	
	// discard Source VChan specific data
	ListDispose(srcVChan->sinkVChans);
	
	// discard base VChan data
	OKfree(srcVChan->baseClass.name);
	
	OKfree(*srcVChanPtr);
}

static void discard_SinkVChan_type (SinkVChan_type** sinkVChanPtr)
{
	SinkVChan_type* 	sinkVChan	= *sinkVChanPtr;
	char*				errMsg		= NULL;
	
	if (!sinkVChan) return;
	
	// disconnect Sink from Source if connected
	VChan_Disconnect((VChan_type*)sinkVChan);
	
	// release any data packets still in the VChan TSQ
	ReleaseAllDataPackets (sinkVChan, &errMsg); 
	
	// discard data packet types array
	OKfree(sinkVChan->dataTypes);
	
	// discard Sink VChan specific data 
	CmtDiscardTSQ(sinkVChan->tsqHndl);
	
	// discard base VChan data
	OKfree(sinkVChan->baseClass.name);
	
	OKfree(errMsg);
	OKfree(*sinkVChanPtr);
}

/// HIFN Disconnects a Source VChan from all its Sink VChans
static BOOL disconnectSourceVChan (SourceVChan_type* srcVChan)
{
	if (!srcVChan) return FALSE;
	
	SinkVChan_type*		sinkVChan	= NULL;
	
	for (size_t i = 1; i <= ListNumItems(srcVChan->sinkVChans); i++) {
		sinkVChan = *(SinkVChan_type**)ListGetPtrToItem(srcVChan->sinkVChans, i);
		sinkVChan->sourceVChan = NULL;
		// if Sink VChan is open, close it
		if (sinkVChan->baseClass.isOpen) {
			sinkVChan->baseClass.isOpen = VChan_Closed;
			if (sinkVChan->baseClass.VChanStateChangeCBFptr)
				(*sinkVChan->baseClass.VChanStateChangeCBFptr)	((VChan_type*)sinkVChan, sinkVChan->baseClass.VChanOwner, sinkVChan->baseClass.isOpen);
		} 
	}
	
	ListClear(srcVChan->sinkVChans);
	
	// if Source VChan is open, close it
	if (srcVChan->baseClass.isOpen) {
		srcVChan->baseClass.isOpen = VChan_Closed;
		if (srcVChan->baseClass.VChanStateChangeCBFptr)
			(*srcVChan->baseClass.VChanStateChangeCBFptr) ((VChan_type*)srcVChan, srcVChan->baseClass.VChanOwner, srcVChan->baseClass.isOpen);
			
	}
		
	return TRUE;
}

static BOOL disconnectSinkVChan (SinkVChan_type* sinkVChan)
{
	if (!sinkVChan) return FALSE;
	
	SinkVChan_type* 		connectedSinkVChan	= NULL;
	SourceVChan_type*		srcVChan			= sinkVChan->sourceVChan;
	
	
	if (!srcVChan) return TRUE; // do nothing if there is no source to disconnect
	// remove Source VChan from Sink VChan
	sinkVChan->sourceVChan = NULL;
	
	// if Sink VChan is open, close it
	if (sinkVChan->baseClass.isOpen) {
		sinkVChan->baseClass.isOpen = VChan_Closed;
		if (sinkVChan->baseClass.VChanStateChangeCBFptr)
			(*sinkVChan->baseClass.VChanStateChangeCBFptr) ((VChan_type*)sinkVChan, sinkVChan->baseClass.VChanOwner, sinkVChan->baseClass.isOpen);
	}
		
	// remove Sink VChan from Source VChan
	for (size_t i = 1; i <= ListNumItems(srcVChan->sinkVChans); i++) {
		connectedSinkVChan = *(SinkVChan_type**)ListGetPtrToItem(srcVChan->sinkVChans, i);
		if (connectedSinkVChan == sinkVChan) {
			ListRemoveItem(srcVChan->sinkVChans, 0, i);
			// if Source VChan is open and there are no more active Sinks, then close Source VChan
			if (srcVChan->baseClass.isOpen && !GetNumActiveSinkVChans(srcVChan)) {
				srcVChan->baseClass.isOpen = VChan_Closed; 
				if (srcVChan->baseClass.VChanStateChangeCBFptr)
					(*srcVChan->baseClass.VChanStateChangeCBFptr) ((VChan_type*)srcVChan, srcVChan->baseClass.VChanOwner, srcVChan->baseClass.isOpen);
			}
				
			return TRUE;
		}
	}
	
	return FALSE;
}

//==============================================================================
// Static global variables


//==============================================================================
// Global functions


//------------------------------------------------------------------------------
// VChan Creation / Destruction and management functions
//------------------------------------------------------------------------------

/// HIFN Creates a Source VChan of a certain data type.
SourceVChan_type* init_SourceVChan_type (char 							name[], 
										 DLDataTypes 					dataType,
										 void* 							VChanOwner,
										 VChanStateChangeCBFptr_type	VChanStateChangeCBFptr)
{
	SourceVChan_type*	vchan 	= malloc(sizeof(SourceVChan_type));
	if (!vchan) return NULL;
	
	// init base VChan type
	if (init_VChan_type ((VChan_type*) vchan, name, VChan_Source, VChanOwner, (DiscardVChanFptr_type)discard_SourceVChan_type, 
						 (DisconnectVChanFptr_type)disconnectSourceVChan, VChanStateChangeCBFptr, (VChanIsConnectedFptr_type) SourceVChanIsConnected) < 0) goto Error;
	
	// init list with Sink VChans
	if (!(vchan -> sinkVChans 	= ListCreate(sizeof(SinkVChan_type*)))) goto Error;
	
	// init data packet type
	vchan->dataType = dataType;
	
	return vchan;
	
Error:
	
	discard_VChan_type ((VChan_type**)&vchan);  // do this last
	return NULL;
}

/// HIFN Creates a Sink VChan which may support multiple data types. Provide an array of dataTypes of DLDataTypes elements.
SinkVChan_type* init_SinkVChan_type (char 							name[], 
									 DLDataTypes					dataTypes[],
									 size_t							nDataTypes,
									 void* 							VChanOwner,
									 double							readTimeout,
									 VChanStateChangeCBFptr_type	VChanStateChangeCBFptr)
{
	SinkVChan_type*	vchan 	= malloc(sizeof(SinkVChan_type));
	if (!vchan) return NULL;
	
	// init
	vchan->dataTypes		= NULL;
	
	// init base VChan type
	if (init_VChan_type ((VChan_type*) vchan, name, VChan_Sink, VChanOwner, (DiscardVChanFptr_type)discard_SinkVChan_type, 
						 (DisconnectVChanFptr_type)disconnectSinkVChan, VChanStateChangeCBFptr, (VChanIsConnectedFptr_type)SinkVChanIsConnected) < 0) goto Error;
	
	// INIT DATA
	vchan->sourceVChan 		= NULL;
	vchan->nDataTypes		= nDataTypes;
	vchan->readTimeout		= readTimeout;
	// copy data types
	vchan->dataTypes		= malloc(nDataTypes * sizeof(DLDataTypes));
	if (!vchan->dataTypes) goto Error;
	memcpy(vchan->dataTypes, dataTypes, nDataTypes*sizeof(DLDataTypes));
	
	// init thread safe queue
	CmtNewTSQ(DEFAULT_SinkVChan_QueueSize, sizeof(DataPacket_type*), 0, &vchan->tsqHndl); 
	// init write timeout (time to keep on trying to write a data packet to the queue)
	vchan->writeTimeout 	= DEFAULT_SinkVChan_QueueWriteTimeout;
	
	return vchan;
	
Error:
	
	OKfree(vchan->dataTypes);
	discard_VChan_type ((VChan_type**)&vchan);  // do this last
	return NULL;
}

/// HIFN Discards both Sink and Source VChans.
void discard_VChan_type (VChan_type** VChan)
{
	if (!*VChan) return;
	
	// call discard function specific to the VChan type
	(*(*VChan)->DiscardFptr)	(VChan);
}

/// HIFN Checks if a VChan object exists among a list of VChans of VChan_type*.
/// OUT idx 1-based index of the found VChan object within the VChan list.Pass 0 if this is not needed. If item is not found, *idx is 0.
/// HIRET TRUE if VChan exists, FALSE otherwise.  
BOOL VChanExists (ListType VChanList, VChan_type* VChan, size_t* idx)
{
	VChan_type* 	VChanItem 	= NULL;
	size_t			nVChans		= ListNumItems(VChanList);
	for (size_t i = 1; i <= nVChans; i++) {
		VChanItem = *(VChan_type**)ListGetPtrToItem(VChanList, i);
		if (VChanItem == VChan) {
			if (idx) *idx = i;
			return TRUE;
		}
	}
	if (idx) *idx = 0;
	return FALSE;
}

/// HIFN Searches for a given VChan name from a VChan list of VChan_type* and if found, returns a pointer to the VChan object.
/// OUT idx 1-based index of VChan object in the list of VChans. Pass 0 if this is not needed. If item is not found, *idx is 0.
/// HIRET Pointer to the found VChan_type* if VChan exists, NULL otherwise. 
VChan_type* VChanNameExists (ListType VChanList, char VChanName[], size_t* idx)
{
	VChan_type* 	VChan		= NULL;
	size_t			nVChans		= ListNumItems(VChanList);
	
	for (size_t i = 1; i <= nVChans; i++) {
		VChan = *(VChan_type**)ListGetPtrToItem(VChanList, i);
		if (!strcmp(VChan->name, VChanName)) {
			if (idx) *idx = i;
			return VChan;
		}
	}
	
	if (idx) *idx = 0;
	return NULL;
}

//------------------------------------------------------------------------------
// VChan Connections
//------------------------------------------------------------------------------

/// HIFN Determines if VChans have compatible data types.
/// HIRET True if compatible, False otherwise.
BOOL VChansAreCompatible (SourceVChan_type* srcVChan, SinkVChan_type* sinkVChan)
{
	for (size_t i = 0; i < sinkVChan->nDataTypes; i++)
		if (srcVChan->dataType == sinkVChan->dataTypes[i])
			return TRUE;
			
	return FALSE;		
}

/// HIFN Connects a Source VChan and Sink VChan. If the provided Sink VChan is connected to another Source VChan, it is first disconnected from that Source VChan.
/// HIFN If both Source and Sink VChans are active then they will be opened if they are closed.
/// HIRET True if successful, False otherwise.
BOOL VChan_Connect (SourceVChan_type* srcVChan, SinkVChan_type* sinkVChan)
{
	if (!srcVChan || !sinkVChan) return FALSE;
	
	SinkVChan_type* 	connectedSinkVChan  = NULL;
	size_t				nSinks				= ListNumItems(srcVChan->sinkVChans);
	
	// check if the provided Sink VChan is already connected to this Source VChan
	for (size_t i = 1; i <= nSinks; i++) {
		connectedSinkVChan = *(SinkVChan_type**)ListGetPtrToItem(srcVChan->sinkVChans, i);
		if (connectedSinkVChan == sinkVChan) return FALSE;
	}
	
	// if Sink VChan is already connected to another Source VChan, disconnect it from that Source VChan
	if (sinkVChan->sourceVChan) 
		if(!VChan_Disconnect((VChan_type*)sinkVChan) ) return FALSE;  // error
		
	// add sink to source's list of sinks
	if (!ListInsertItem(srcVChan->sinkVChans, &sinkVChan, END_OF_LIST)) return FALSE;  // error
	
	// add Source VChan to Sink VChan
	sinkVChan->sourceVChan = srcVChan;
	
	// if both Sink and Source VChans are active, then open Sink VChan and also Source VChan if it is not open already
	if (srcVChan->baseClass.isActive && sinkVChan->baseClass.isActive) {
		// open Sink VChan 
		sinkVChan->baseClass.isOpen = VChan_Open;
		if (sinkVChan->baseClass.VChanStateChangeCBFptr)
			(*sinkVChan->baseClass.VChanStateChangeCBFptr) ((VChan_type*)sinkVChan, sinkVChan->baseClass.VChanOwner, sinkVChan->baseClass.isOpen);
		
		// open Source VChan if not open already
		if (!srcVChan->baseClass.isOpen) {
			srcVChan->baseClass.isOpen = VChan_Open;
			if (srcVChan->baseClass.VChanStateChangeCBFptr)
				(*srcVChan->baseClass.VChanStateChangeCBFptr) ((VChan_type*)srcVChan, srcVChan->baseClass.VChanOwner, srcVChan->baseClass.isOpen);
		}
			
	}
	
	return TRUE;	
}

/// HIFN Disconnects a VChan from other VChans.
/// HIRET True if successful, False otherwise
BOOL VChan_Disconnect (VChan_type* VChan)
{
	return (*VChan->DisconnectFptr)	(VChan);
}

BOOL IsVChanConnected (VChan_type* VChan)
{
	return (*VChan->VChanIsConnectedFptr) (VChan);
}

//------------------------------------------------------------------------------
// VChan Set/Get/Is
//------------------------------------------------------------------------------

void SetVChanName (VChan_type* VChan, char newName[])
{ 
	OKfree(VChan->name);
	VChan->name = StrDup(newName);
}
 
char* GetVChanName (VChan_type* VChan)
{
	return StrDup(VChan->name);
}

/// HIFN Given a Source VChan, the function returns the name of the Sink VChan attached to the source having the 1-based index sinkIdx. If index is out of range it returns NULL.
char* GetSinkVChanName (SourceVChan_type* srcVChan, size_t sinkIdx)
{
	SinkVChan_type* sinkVChan = *(SinkVChan_type**)ListGetPtrToItem(srcVChan->sinkVChans, sinkIdx);
	
	if (!sinkVChan) return NULL;
	
	return GetVChanName((VChan_type*)sinkVChan);
}

VChanDataFlows GetVChanDataFlowType (VChan_type* VChan)
{
	return VChan->dataFlow;
}

void* GetVChanOwner (VChan_type* VChan)
{
	return VChan->VChanOwner;
}

void SetSourceVChanDataType (SourceVChan_type* srcVChan, DLDataTypes dataType)
{
	// disconnect Sink VChans if they are incompatible
	size_t 				nSinks 			= ListNumItems(srcVChan->sinkVChans);
	SinkVChan_type*		sinkVChan;
	BOOL				compatibleVChans;
	for (size_t i = 1; i <= nSinks; i++) {
		sinkVChan = *(SinkVChan_type**) ListGetPtrToItem(srcVChan->sinkVChans, i);
		
		compatibleVChans = FALSE;
		for (size_t j = 0; j < sinkVChan->nDataTypes; j++)
			if (srcVChan->dataType == sinkVChan->dataTypes[j]) {
				compatibleVChans = TRUE;
				break;
			}
		
		if (!compatibleVChans)
			VChan_Disconnect((VChan_type*)sinkVChan);
	}
	
	// change Source VChan data type
	srcVChan->dataType = dataType;
}

DLDataTypes GetSourceVChanDataType (SourceVChan_type* srcVChan)
{
	return srcVChan->dataType;
}

void SetSinkVChanDataTypes (SinkVChan_type* sinkVChan, size_t nDataTypes, DLDataTypes dataTypes[])
{
	BOOL	compatibleVChans	= FALSE;
	
	if (!nDataTypes) return;
	
	if (sinkVChan->sourceVChan) {
		for (size_t i = 0; i < nDataTypes; i++)
			if (dataTypes[i] == sinkVChan->sourceVChan->dataType) {
				compatibleVChans = TRUE;
				break;
			}
		
		if (!compatibleVChans)
			VChan_Disconnect((VChan_type*)sinkVChan);
	}
	
	// copy new data types
	OKfree(sinkVChan->dataTypes);
	sinkVChan->nDataTypes 	= nDataTypes;
	sinkVChan->dataTypes	= malloc(nDataTypes * sizeof(DLDataTypes));
	memcpy(sinkVChan->dataTypes, dataTypes, nDataTypes*sizeof(DLDataTypes));
}

void SetVChanActive (VChan_type* VChan, BOOL isActive)
{
	// if it has the same active/inactive status, do nothing
	if (VChan->isActive == isActive) return;
	
	// update active/inactive
	VChan->isActive = isActive;
	
	// open/close VChans if neccessary
	switch (VChan->dataFlow) {
			
		case VChan_Source:
			
			if (VChan->isActive) {   // Inactive -> Active
				// Source VChan is closed. Open Source VChan if it has at least an active Sink VChan
				if (GetNumActiveSinkVChans((SourceVChan_type*)VChan)) {
					VChan->isOpen = VChan_Open;
					if (VChan->VChanStateChangeCBFptr)
						(*VChan->VChanStateChangeCBFptr) (VChan, VChan->VChanOwner, VChan->isOpen);
					
				}
				
				// Sink VChans are closed. Open active Sink VChans.
				SinkVChan_type*		sinkVChan 	= NULL;
				size_t				nSinks		= ListNumItems(((SourceVChan_type*)VChan)->sinkVChans);
				for (size_t i = 1; i <= nSinks; i++) {
					sinkVChan = *(SinkVChan_type**)ListGetPtrToItem(((SourceVChan_type*)VChan)->sinkVChans, i);
					if (!sinkVChan->baseClass.isActive) continue; // skip inactive Sink VChans
					sinkVChan->baseClass.isOpen = VChan_Open;
					if (sinkVChan->baseClass.VChanStateChangeCBFptr)
						(*sinkVChan->baseClass.VChanStateChangeCBFptr) ((VChan_type*)sinkVChan, sinkVChan->baseClass.VChanOwner, sinkVChan->baseClass.isOpen);
				}
				
			} else {	// Active -> Inactive
				// If Source VChan is open, then close it
				if (VChan->isOpen) {
					VChan->isOpen = VChan_Closed;
					if (VChan->VChanStateChangeCBFptr)
						(*VChan->VChanStateChangeCBFptr) (VChan, VChan->VChanOwner, VChan->isOpen);
				}
				
				// Close all open Sink VChans
				SinkVChan_type*		sinkVChan 	= NULL;
				size_t				nSinks		= ListNumItems(((SourceVChan_type*)VChan)->sinkVChans);
				for (size_t i = 1; i <= nSinks; i++) {
					sinkVChan = *(SinkVChan_type**)ListGetPtrToItem(((SourceVChan_type*)VChan)->sinkVChans, i);
					if (!sinkVChan->baseClass.isOpen) continue; // skip closed Sink VChans
					sinkVChan->baseClass.isOpen = VChan_Closed;
					if (sinkVChan->baseClass.VChanStateChangeCBFptr)
						(*sinkVChan->baseClass.VChanStateChangeCBFptr) ((VChan_type*)sinkVChan, sinkVChan->baseClass.VChanOwner, sinkVChan->baseClass.isOpen);
				}
				
			}
			break;
			
		case VChan_Sink:
			
			if (VChan->isActive) {  // Inactive -> Active
				// Sink VChan is closed, open Sink VChan if a Source VChan is attached to it and is active
				SourceVChan_type*	srcVChan = ((SinkVChan_type*)VChan)->sourceVChan;
				if (srcVChan && srcVChan->baseClass.isActive) {
					VChan->isOpen = VChan_Open;
					if (VChan->VChanStateChangeCBFptr)
						(*VChan->VChanStateChangeCBFptr) (VChan, VChan->VChanOwner, VChan->isOpen);
				}
				
				// If Sink VChan has a closed active Source VChan, then open the Source VChan
				if (srcVChan && srcVChan->baseClass.isActive && !srcVChan->baseClass.isOpen) {
					srcVChan->baseClass.isOpen = VChan_Open;
					if (srcVChan->baseClass.VChanStateChangeCBFptr)
						(*srcVChan->baseClass.VChanStateChangeCBFptr) ((VChan_type*)srcVChan, srcVChan->baseClass.VChanOwner, srcVChan->baseClass.isOpen);
						
				}
					
			} else { // Active -> Inactive
				// Close Sink VChan if open
				if (VChan->isOpen) {
					VChan->isOpen = VChan_Closed;
					if (VChan->VChanStateChangeCBFptr)
						(*VChan->VChanStateChangeCBFptr) (VChan, VChan->VChanOwner, VChan->isOpen);
				}
				
				// If Sink has an open Source VChan and there are no more active Sinks, then close Source VChan
				SourceVChan_type*	srcVChan = ((SinkVChan_type*)VChan)->sourceVChan;
				if (srcVChan && srcVChan->baseClass.isOpen && !GetNumActiveSinkVChans(srcVChan)) {
					srcVChan->baseClass.isOpen = VChan_Closed;
					if (srcVChan->baseClass.VChanStateChangeCBFptr)
						(*srcVChan->baseClass.VChanStateChangeCBFptr) ((VChan_type*)srcVChan, srcVChan->baseClass.VChanOwner, srcVChan->baseClass.isOpen);
					
				}
				
			}
			break;
	}
	
}

BOOL GetVChanActive (VChan_type* VChan)
{
	return VChan->isActive;
}

BOOL IsVChanOpen (VChan_type* VChan)
{
	return VChan->isOpen;
}

SinkVChan_type*	GetSinkVChan (SourceVChan_type* srcVChan, size_t sinkIdx)
{
	SinkVChan_type* sinkVChan = *(SinkVChan_type**)ListGetPtrToItem(srcVChan->sinkVChans, sinkIdx);
	
	return sinkVChan;
}

SourceVChan_type* GetSourceVChan (SinkVChan_type* sinkVChan)
{
	return sinkVChan->sourceVChan;
}

size_t GetNSinkVChans (SourceVChan_type* srcVChan)
{
	return ListNumItems(srcVChan->sinkVChans);
}

ListType GetSinkVChanList (SourceVChan_type* srcVChan)
{
	return srcVChan->sinkVChans;
}

CmtTSQHandle GetSinkVChanTSQHndl (SinkVChan_type* sinkVChan)
{
	return sinkVChan->tsqHndl;
}

void SetSinkVChanTSQSize (SinkVChan_type* sinkVChan, size_t nItems)
{
	CmtSetTSQAttribute(sinkVChan->tsqHndl, ATTR_TSQ_QUEUE_SIZE, nItems); 
}

size_t GetSinkVChanTSQSize (SinkVChan_type* sinkVChan)
{
	size_t nItems;
	
	CmtGetTSQAttribute(sinkVChan->tsqHndl, ATTR_TSQ_QUEUE_SIZE, &nItems); 
	
	return nItems;
}

void SetSinkVChanReadTimeout (SinkVChan_type* sinkVChan, double time)
{
	sinkVChan->readTimeout = time;
}

double GetSinkVChanReadTimeout	(SinkVChan_type* sinkVChan)
{
	return sinkVChan->readTimeout;
}

//------------------------------------------------------------------------------
// Data Packet Management
//------------------------------------------------------------------------------

int ReleaseAllDataPackets (SinkVChan_type* sinkVChan, char** errorMsg)
{
INIT_ERR
	
	DataPacket_type**		dataPackets		= NULL;
	size_t					nPackets		= 0;

	errChk( GetAllDataPackets (sinkVChan, &dataPackets, &nPackets, &errorInfo.errMsg) );
	
	// release data packets
	for(size_t i = 0; i < nPackets; i++)
		ReleaseDataPacket(&dataPackets[i]);
	
Error:
	
	OKfree(dataPackets);
	
RETURN_ERR
}

int SendDataPacket (SourceVChan_type* srcVChan, DataPacket_type** dataPacketPtr, BOOL srcVChanNeedsPacket, char** errorMsg)
{
INIT_ERR
	
	size_t				nSinks				= ListNumItems(srcVChan->sinkVChans);
	SinkVChan_type* 	sinkVChan			= NULL;
	size_t				nPacketsSent		= 0;		// counts to how many Sink VChans the packet has been sent successfuly
	size_t				nPacketRecipients	= 0;		// counts the number of packet recipients
	
	
	// set packet counter
	// Note: Since the Source VChan is open, an active Sink VChan is also an open Sink VChan
	
	if (srcVChanNeedsPacket) {
		nPacketRecipients = GetNumOpenSinkVChans(srcVChan)+1;
		nPacketsSent++;
	} else
		nPacketRecipients = GetNumOpenSinkVChans(srcVChan);
		
	if (*dataPacketPtr)
		SetDataPacketCounter(*dataPacketPtr, nPacketRecipients);
		
	// if there are no recipients then dispose of the data packet
	if (!nPacketRecipients) {
		ReleaseDataPacket(dataPacketPtr);
		return 0; 
	}
	
	// send data packet
	for (size_t i = 1; i <= nSinks; i++) {
		sinkVChan = *(SinkVChan_type**)ListGetPtrToItem(srcVChan->sinkVChans,i);
		if (!sinkVChan->baseClass.isOpen) continue; // forward packet only to open Sink VChans connected to this Source VChan
		// put data packet into Sink VChan TSQ
		CmtErrChk( CmtWriteTSQData(sinkVChan->tsqHndl, dataPacketPtr, 1, (int)sinkVChan->writeTimeout, NULL) );
		nPacketsSent++;
	}
	
	*dataPacketPtr = NULL; 	// Data packet is considered to be consumed even if sending to some Sink VChans did not succeed
							// Sink VChans that did receive the data packet, can further process it and release it.
	return 0;

CmtError:
	
Cmt_ERR
	
Error:

	// cleanup
	for (size_t i = 0; i < nPacketRecipients - nPacketsSent; i++)
		ReleaseDataPacket(dataPacketPtr);
	
RETURN_ERR
}

int	SendNullPacket (SourceVChan_type* srcVChan, char** errorMsg)
{
	DataPacket_type*	nullPacket = NULL;
	
	return SendDataPacket(srcVChan, &nullPacket, FALSE, errorMsg);
}

/// HIFN Gets all data packets that are available from a Sink VChan. The function allocates dynamically a data packet array with nPackets elements of DataPacket_type*
/// HIFN If there are no data packets in the Sink VChan, dataPackets = NULL, nPackets = 0 and the function returns immediately with 0 (success).
/// HIFN If an error is encountered, the function returns a negative value with error information and sets dataPackets to NULL and nPackets to 0.
/// HIRET 0 if successful, and negative error code otherwise.
/// OUT dataPackets, nPackets
int GetAllDataPackets (SinkVChan_type* sinkVChan, DataPacket_type*** dataPackets, size_t* nPackets, char** errorMsg)
{
INIT_ERR
	
	CmtTSQHandle			sinkVChanTSQHndl 	= sinkVChan->tsqHndl;
	
	// init
	*dataPackets 	= NULL;
	*nPackets		= 0;
	
	// get number of available packets
	CmtErrChk( CmtGetTSQAttribute(sinkVChanTSQHndl, ATTR_TSQ_ITEMS_IN_QUEUE, nPackets) );
	if (!*nPackets) return 0;	// no data packets, return with success
	
	// get data packets
	nullChk( *dataPackets = malloc (*nPackets * sizeof(DataPacket_type*)) );
	CmtErrChk( CmtReadTSQData(sinkVChanTSQHndl, *dataPackets, *nPackets, 0, 0) );
	
	return 0;
	
CmtError:	

Cmt_ERR
	
Error:
	
	// cleanup
	OKfree(*dataPackets);
	*nPackets 	= 0;

RETURN_ERR
}

/// HIFN Attempts to get one data packet from a Sink VChan before the timeout period. 
/// HIFN On success, it return 0, otherwise a negative numbers. If an error is encountered, the function returns error information.
/// OUT dataPackets, nPackets
int GetDataPacket (SinkVChan_type* sinkVChan, DataPacket_type** dataPacketPtr, char** errorMsg)
{
#define GetDataPacket_Err_Timeout	-1

INIT_ERR
	
	CmtTSQHandle		sinkVChanTSQHndl 	= sinkVChan->tsqHndl;
	int					itemsRead			= 0;
	char*				msgBuff				= NULL;
	
	
	*dataPacketPtr = NULL;
	
	// get data packet
	CmtErrChk( itemsRead = CmtReadTSQData(sinkVChanTSQHndl, dataPacketPtr, 1, (int)sinkVChan->readTimeout, 0) );
	
	// check if timeout occured
	if (!itemsRead) {
		nullChk( msgBuff = StrDup("Waiting for ") );
		nullChk( AppendString(&msgBuff, sinkVChan->baseClass.name, -1) );
		nullChk( AppendString(&msgBuff, " Sink VChan data timed out.", -1) );
		SET_ERR(GetDataPacket_Err_Timeout, msgBuff);
	}
	
	return 0;		 

CmtError:
	
Cmt_ERR
	
Error:
	
	// cleanup
	OKfree(msgBuff);
	
RETURN_ERR
}

//------------------------------------------------------------------------------ 
// Waveform data management
//------------------------------------------------------------------------------ 

int	ReceiveWaveform (SinkVChan_type* sinkVChan, Waveform_type** waveform, WaveformTypes* waveformType, char** errorMsg)
{
#define		ReceiveWaveform_Err_NoWaveform			-1
#define		ReceiveWaveform_Err_WrongDataType		-2
#define		ReceiveWaveform_Err_NotSameDataType		-3
	
INIT_ERR	
	
	DataPacket_type*		dataPacket				= NULL;
	DLDataTypes				dataPacketType			= 0;
	DLDataTypes				firstDataPacketType		= 0;
	void*					dataPacketPtrToData		= NULL;
	char*					msgBuff					= NULL;
	
	
	// init
	*waveform = NULL;
	
	// get first data packet and check if it is a NULL packet
	errChk ( GetDataPacket(sinkVChan, &dataPacket, &errorInfo.errMsg) );
	
	if (!dataPacket) {
		nullChk( msgBuff = StrDup("Waveform received does not contain any data. This occurs if a NULL packet is encountered before any data packets") );
		SET_ERR(ReceiveWaveform_Err_NoWaveform, msgBuff);
	}
	
	// check if data packet type is supported by this function
	dataPacketPtrToData = GetDataPacketPtrToData(dataPacket, &firstDataPacketType);
	dataPacketType 		= firstDataPacketType;
	
	// check if data packet is of waveform type
	switch (firstDataPacketType) {
			
		case DL_Waveform_Char:
		case DL_Waveform_UChar:
		case DL_Waveform_Short:
		case DL_Waveform_UShort:
		case DL_Waveform_Int:
		case DL_Waveform_UInt:
		case DL_Waveform_Int64:
		case DL_Waveform_UInt64:	
		case DL_Waveform_SSize:
		case DL_Waveform_Size:
		case DL_Waveform_Float:
		case DL_Waveform_Double:
			break;
		
		default:
			
			nullChk( msgBuff = StrDup("Data packet received is not of a waveform type and cannot be retrieved by this function") );
			SET_ERR(ReceiveWaveform_Err_WrongDataType, msgBuff);
	}
	
	errChk( CopyWaveform(waveform, *(Waveform_type**)dataPacketPtrToData, &errorInfo.errMsg) );
	ReleaseDataPacket(&dataPacket);
	
	// get another data packet if any
	errChk ( GetDataPacket(sinkVChan, &dataPacket, &errorInfo.errMsg) );
	// assemble waveform from multiple packets until a NULL packet is encountered
	while (dataPacket) {
		
		// get waveform data from the first non-NULL data packet
		dataPacketPtrToData = GetDataPacketPtrToData(dataPacket, &dataPacketType);
		
		// check if retrieved data packet is of the same type as the first packet
		if (firstDataPacketType != dataPacketType) {
			nullChk( msgBuff = StrDup("Data packets must be all of the same type") );
			SET_ERR(ReceiveWaveform_Err_NotSameDataType, msgBuff);
		}
	
		errChk( AppendWaveform(*waveform, *(Waveform_type**)dataPacketPtrToData, &errorInfo.errMsg) );
	
		ReleaseDataPacket(&dataPacket);
		
		// get another packet
		errChk ( GetDataPacket(sinkVChan, &dataPacket, &errorInfo.errMsg) );
	}
	
	// check again if waveform is NULL
	if (!*waveform) {
		nullChk( msgBuff = StrDup("Waveform received does not contain any data. This occurs if a NULL packet is encountered before any data packets or the data packet doesn't have any data") );
		SET_ERR(ReceiveWaveform_Err_NoWaveform, msgBuff);
	}
	
	// assign waveform type
	if (waveformType)
		switch (dataPacketType) {
			
			case DL_Waveform_Char:
				*waveformType = Waveform_Char;
				break;
				
			case DL_Waveform_UChar:
				*waveformType = Waveform_UChar;
				break;
				
			case DL_Waveform_Short:
				*waveformType = Waveform_Short;
				break;
				
			case DL_Waveform_UShort:
				*waveformType = Waveform_UShort;
				break;
				
			case DL_Waveform_Int:
				*waveformType = Waveform_Int;
				break;
				
			case DL_Waveform_UInt:
				*waveformType = Waveform_UInt;
				break;
				
			case DL_Waveform_Int64:
				*waveformType = Waveform_Int64;
				break;
				
			case DL_Waveform_UInt64:
				*waveformType = Waveform_UInt64;
				break;
				
			case DL_Waveform_SSize:
				*waveformType = Waveform_SSize;
				break;
				
			case DL_Waveform_Size:
				*waveformType = Waveform_Size;
				break;
				
			case DL_Waveform_Float:
				*waveformType = Waveform_Float;
				break;
				
			case DL_Waveform_Double:
				*waveformType = Waveform_Double;
				break;
			
			default:
			
				nullChk( msgBuff = StrDup(" Data packet received is not of a waveform type and cannot be retrieved by this function") );
				SET_ERR(ReceiveWaveform_Err_WrongDataType, msgBuff);
		}
		
	return 0;
	
Error:
	
	// cleanup
	OKfree(msgBuff);
	ReleaseDataPacket(&dataPacket);
	discard_Waveform_type(waveform);
	
RETURN_ERR
}
