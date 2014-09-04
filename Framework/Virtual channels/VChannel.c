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
#include "nivision.h"
#include "utility.h"
#include "VChannel.h"

//==============================================================================
// Constants

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}  
#define DEFAULT_SinkVChan_QueueSize			1000
#define DEFAULT_SinkVChan_QueueWriteTimeout	1000.0	// number of [ms] to wait while trying
													// to add a data packet to a Sink VChan TSQ

//==============================================================================
// Types


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
	
};

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
	double						writeTimeout;
	
};

//---------------------------------------------------------------------------------------------------
// Source VChan
//---------------------------------------------------------------------------------------------------

struct SourceVChan {
	
	//-----------------------
	// Base class
	//-----------------------
	
	VChan_type					baseClass;				// Must be first member to allow inheritance from parent
	
	//-----------------------	
	// Communication
	//-----------------------
	
	ListType					sinkVChans;				// Connected Sink VChans. List of SinkVChan_type*
											
};

//---------------------------------------------------------------------------------------------------
// Data Packet
//---------------------------------------------------------------------------------------------------
/*
struct DataPacket {
	VChanData_type 				dataType; 				// Data type contained in the data packet.
	void*         				data;     				// Pointer to data of dataType
	CmtTSVHandle   				ctr;      				// Although there are multiple sinks that can receive a data packet, 
														// there is only one copy of the data in the memory. 
														// To de-allocate memory for the data, each sink must call ReleaseDataPacket 
														// which in the end frees the memory if ctr being a thread safe variable reaches 0 
	DiscardDataFptr_type 		discardFptr;
};
*/
//==============================================================================
// Static functions

static void 				discard_DataPacket_type		(DataPacket_type* a);
							
static int 					init_VChan_type 			(VChan_type* 				vchan, 
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
	if (init_VChan_type ((VChan_type*) vchan, name, dataType, VChan_Source, vChanOwner, 
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
									 Connected_CBFptr_type		Connected_CBFptr,
									 Disconnected_CBFptr_type	Disconnected_CBFptr)
{
	SinkVChan_type*	vchan = malloc(sizeof(SinkVChan_type));
	if (!vchan) return NULL;
	
	// init base VChan type
	if (init_VChan_type ((VChan_type*) vchan, name, dataType, VChan_Sink, vChanOwner, 
						 discard_SinkVChan_type, disconnectSourceVChan, Connected_CBFptr, Disconnected_CBFptr) < 0) goto Error;
	
		// INIT DATA
		
	vchan->sourceVChan = NULL;
	// init thread safe queue
	CmtNewTSQ(DEFAULT_SinkVChan_QueueSize, sizeof(DataPacket_type), 0, &vchan->tsqHndl); 
	// init write timeout (time to keep on trying to write a data packet to the queue)
	vchan->writeTimeout = DEFAULT_SinkVChan_QueueWriteTimeout;
	
	return vchan;
	Error:
	
	discard_VChan_type ((VChan_type**)&vchan);  // do this last
	return NULL;
	
}

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
	VChan_type** 	VChanPtrPtr;
	size_t			nVChans			= ListNumItems(VChanList);
	for (size_t i = 1; i <= nVChans; i++) {
		VChanPtrPtr = ListGetPtrToItem(VChanList, i);
		if (*VChanPtrPtr == VChan) {
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
	VChan_type** 	VChanPtrPtr;
	char*			listVChanName;
	size_t			nVChans		= ListNumItems(VChanList);
	for (size_t i = 1; i <= nVChans; i++) {
		VChanPtrPtr = ListGetPtrToItem(VChanList, i);
		listVChanName = GetVChanName(*VChanPtrPtr);
		if (!strcmp(listVChanName, VChanName)) {
			if (idx) *idx = i;
			OKfree(listVChanName);
			return *VChanPtrPtr;
		}
		OKfree(listVChanName);
	}
	
	if (idx) *idx = 0;
	return NULL;
}

char* GetUniqueVChanName (ListType VChanList, char baseVChanName[])
{
	size_t n        = 2;
	char*  name;   
	char   countstr [500];
	
	name = StrDup(baseVChanName);
	AppendString(&name, " 1", -1);
	while (VChanNameExists (VChanList, name, 0)) {
		OKfree(name);
		name = StrDup(baseVChanName);
		Fmt(countstr, "%s<%d", n);
		AppendString(&name, " ", -1);
		AppendString(&name, countstr, -1);
		n++;
	}
	
	return name;
}

//---------------------------------
// Data Packet management functions
//---------------------------------
int init_DataPacket_type(DataPacket_type* dataPacket, VChanData_type dataType, void* data, DiscardDataFptr_type discardFptr)
{
	if (!dataPacket) return -1;
	
	dataPacket -> dataType 		= dataType;
	dataPacket -> data     		= data;
	dataPacket -> discardFptr   = discardFptr;
	
	// create counter to keep track of how many sinks still need this data packet
	if (CmtNewTSV(sizeof(int), &dataPacket->ctr) < 0) return -1;
	// set counter to 0
	int* ctrTSVptr;
	CmtGetTSVPtr(dataPacket->ctr, &ctrTSVptr);
	*ctrTSVptr = 0;
	CmtReleaseTSVPtr(dataPacket->ctr);
	
	return 0;
}

static void discard_DataPacket_type(DataPacket_type* a)
{
	if (!a) return;
	
	// discard counter
	CmtDiscardTSV(a->ctr);
	
	// discard data
	(*a->discardFptr)	(&a->data);
	
}

void ReleaseDataPacket(DataPacket_type* a)
{
	int* ctrTSVptr;
	
	if (!a) return;
	
	CmtGetTSVPtr(a->ctr, &ctrTSVptr);
	if (*ctrTSVptr > 1) {
		(*ctrTSVptr)--;
		CmtReleaseTSVPtr(a->ctr);
	} else {CmtReleaseTSVPtr(a->ctr); discard_DataPacket_type(a);} 
	
}

FCallReturn_type* SendDataPacket (SourceVChan_type* source, DataPacket_type* dataPacket)
{
#define SendDataPacket_Err_TSQWrite		-1
	
	int		nSinks	= ListNumItems(source->sinkVChans);
	
	// if there are no Sink VChans, then dispose of the data and do nothing
	if (!nSinks) {
		ReleaseDataPacket(dataPacket);
		return NULL; 
	} 
	// set sinks counter
	int* ctrTSVptr;
	CmtGetTSVPtr(dataPacket->ctr, &ctrTSVptr);
	*ctrTSVptr = nSinks;
	CmtReleaseTSVPtr(dataPacket->ctr);
	
	// send data to sinks
	SinkVChan_type** 	sinkPtrPtr;
	int					itemsWritten;
	for (size_t i = 1; i <= nSinks; i++) {
		sinkPtrPtr = ListGetPtrToItem(source->sinkVChans,i);
		// put data packet into Sink VChan TSQ
		itemsWritten = CmtWriteTSQData((*sinkPtrPtr)->tsqHndl, dataPacket, 1, (*sinkPtrPtr)->writeTimeout, NULL);
		
		// check if writing the items to the sink queue succeeded
		if (itemsWritten < 0) {
			char* 				errMsg 										= StrDup("Writing data to ");
			char*				sinkName									= GetVChanName((VChan_type*)*sinkPtrPtr);
			char				cmtStatusMessage[CMT_MAX_MESSAGE_BUF_SIZE];
			FCallReturn_type*   fCallReturn;
			
			AppendString(&errMsg, sinkName, -1); 
			AppendString(&errMsg, " failed. Reason: ", -1); 
			CmtGetErrorMessage(itemsWritten, cmtStatusMessage);
			AppendString(&errMsg, cmtStatusMessage, -1); 
			fCallReturn = init_FCallReturn_type(SendDataPacket_Err_TSQWrite, "SendDataPacket", errMsg);
			OKfree(errMsg);
			OKfree(sinkName);
			return fCallReturn;
		}
		
		// check if the number of written elements is the same as what was requested
		if (!itemsWritten) {
			char* 				errMsg 										= StrDup("Sink VChan ");
			char*				sinkName									= GetVChanName((VChan_type*)*sinkPtrPtr);
			FCallReturn_type*   fCallReturn;
			
			AppendString(&errMsg, sinkName, -1); 
			AppendString(&errMsg, " is full", -1); 
			fCallReturn = init_FCallReturn_type(SendDataPacket_Err_TSQWrite, "SendDataPacket", errMsg);
			OKfree(errMsg);
			OKfree(sinkName);
			return fCallReturn;
		}
	}
	
	return NULL; // no error
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

CmtTSQHandle GetSinkVChanTSQHndl	(SinkVChan_type* sink)
{
	return sink->tsqHndl;
}

void SetSinkVChanTSQSize (SinkVChan_type* sink, size_t nItems)
{
	CmtSetTSQAttribute(sink->tsqHndl, ATTR_TSQ_QUEUE_SIZE, nItems); 
}

size_t GetSinkVChanTSQSize (SinkVChan_type* sink)
{
	size_t nItems;
	
	CmtGetTSQAttribute(sink->tsqHndl, ATTR_TSQ_QUEUE_SIZE, &nItems); 
	
	return nItems;
}

void SetSinkVChanWriteTimeout (SinkVChan_type* sink, double time)
{
	sink->writeTimeout = time;
}

double GetSinkVChanWriteTimeout	(SinkVChan_type* sink)
{
	return sink->writeTimeout;
}

void* GetPtrToVChanOwner (VChan_type* vchan)
{
	return vchan->vChanOwner;
}

//==============================================================================
// VChan data types management

Waveform_type* init_Waveform_type (WaveformEnum_type waveformType, size_t n, void* waveform, double rate, double repeat)
{
	Waveform_type* a = malloc (sizeof(Waveform_type));
	if (!a) return NULL;
	
	a->waveformType	= waveformType; 
	a->data			= waveform;
	a->rate			= rate;
	a->repeat		= repeat;
	
	return a;
}

void discard_Waveform_type (void** waveform)
{
	Waveform_type** waveformPtrPtr =(Waveform_type**)waveform;
	if (!*waveformPtrPtr) return;
	
	OKfree((*waveformPtrPtr)->data);
	OKfree(*waveformPtrPtr);
}

Image_type* init_Image_type	(ImageEnum_type imageType, void* image)
{
	Image_type* a = malloc (sizeof(Image_type));
	if (!a) return NULL;
	
	a->imageType	= imageType; 
	a->data			= image;
	
	return a;
}

void discard_Image_type	(void** image)
{
	Image_type** imagePtrPtr	= (Image_type**)image;
	
	// dispose here of different image types
	switch ((*imagePtrPtr)->imageType) {
			
		case Image_NIVIsion:
			
			imaqDispose((*imagePtrPtr)->data);
			
			break;
		// add below more cases for other image types
	}
	
	OKfree(*image);
}
