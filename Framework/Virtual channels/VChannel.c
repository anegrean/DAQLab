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

// Discard VChan function pointer type
typedef void	(* DiscardVChanFptr_type) 			(VChan_type** vchan);

// Disconnect VChan function pointer type
typedef BOOL	(* DisconnectVChanFptr_type)		(VChan_type* vchan);

// Discard Data Packet function pointer type
typedef void	(* DiscardDataPacketFptr_type)		(DataPacket_type** dataPacket);



//==============================================================================
// Globals

	// local global list of VChans linking UpdateSwitchboard to Switchboard_CB
ListType						SwitchboardVChanList	= 0;  

//---------------------------------------------------------------------------------------------------
// Base VChan
//---------------------------------------------------------------------------------------------------

struct VChan {
	
	//-----------------------
	// Identification
	//-----------------------
	
	char*						name;					// Name of virtual chanel. 
	VChanDataFlow_type 			dataFlow;   			// Direction of data flow into or out of the channel.
	BOOL						useAsReference;			// If TRUE and VChan is a Source type, then all Sinks pick up additional VChan properties from this VChan.
														// If TRUE and VChan is Sink type, a connected Source and all its Sinks pick up additional properties from this VChan.
														// If FALSE (default), this VChan can pick up additional properties from other VChans. 
														
	//-----------------------	
	// Source code connection
	//-----------------------
	void*						VChanOwner;				// Reference to object that owns the VChan.
	
	//-----------------------
	// Methods
	//-----------------------
	
	DiscardVChanFptr_type		DiscardFptr;			// Discards VChan type-specific data. 
	
	DisconnectVChanFptr_type	DisconnectFptr;			// Disconnects a VChan.
	
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
	// Data
	//-----------------------
	
	Packet_type*				dataTypes;				// Array of packet data types of Packet_type that the sink may receive.
	size_t						nDataTypes;				// Number of data types that the Sink VChan supports.
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
	// Data
	//-----------------------
	
	Packet_type					dataType;				// Type of data packet which goes through the channel.
	ListType					sinkVChans;				// Connected Sink VChans. List of SinkVChan_type*
											
};

//---------------------------------------------------------------------------------------------------
// Base Data Packet
//---------------------------------------------------------------------------------------------------
struct DataPacket {
	Packet_type 				dataType; 		// Data type contained in the data packet.
	size_t		 				n;				// Number of elements in the packet with specified dataType.	
	void*         				data;     		// Pointer to data array of dataType elements.
	CmtTSVHandle   				ctr;      		// Data Packet in-use counter. Although there are multiple sinks that can receive a data packet, 
												// there is only one copy of the data in the memory. To de-allocate memory for the data, each sink must 
												// call ReleaseDataPacket which in the end frees the memory if ctr reaches 0. 
	DiscardDataPacketFptr_type 	discardFptr;	// Function pointer which will be called to discard the data pointer when ctr reaches 0.
};

//---------------------------------------------------------------------------------------------------
// Waveform Data Packet
//---------------------------------------------------------------------------------------------------
struct WaveformPacket {
	DataPacket_type				baseClass;		// DataPacket_type base class.
	Waveform_type				waveformType;	// Waveform data type
	double						rate;			// Sampling rate in [Hz]. If 0, sampling rate is not given.
	double						nRepeat;		// Number of times the waveform is repeated. If 0, waveform is repeat indefinitely.
};

//---------------------------------------------------------------------------------------------------
// Image Data Packet
//---------------------------------------------------------------------------------------------------
struct ImagePacket {
	DataPacket_type				baseClass;		// DataPacket_type base class.
											// add here more properties that apply
};




//==============================================================================
// Static functions

static void 				init_DataPacket_type				(DataPacket_type* dataPacket, Packet_type dataType, size_t n, void* data, DiscardDataPacketFptr_type discardFptr);

static void 				discard_WaveformPacket_type 		(DataPacket_type** waveformPacket);

static void					discard_NIVisionImagePacket_type	(DataPacket_type** imagePacket);

static int CVICALLBACK 		Switchboard_CB 						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


static int 					init_VChan_type 					(VChan_type* 					vchan, 
																char 							name[], 
										 						VChanDataFlow_type 				flowType, 
														 		void* 							VChanOwner, 	
										 				 		DiscardVChanFptr_type 			DiscardFptr,
										 				 		DisconnectVChanFptr_type		DisconnectFptr,
																Connected_CBFptr_type			Connected_CBFptr,
										 				 		Disconnected_CBFptr_type		Disconnected_CBFptr)
{
	// Data
	vchan -> name   				= StrDup(name); 
	if (!vchan->name) return -1;
	vchan -> dataFlow				= flowType;
	vchan -> useAsReference			= FALSE;
	vchan -> VChanOwner				= VChanOwner;
	
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
	
	// release any data packets still in the VChan TSQ
	ReleaseAllDataPackets (sink); 
	
	// discard data packet types array
	OKfree(sink->dataTypes);
	
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
	
	if (!src) return TRUE; // do nothing if there is no source to disconnect
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


SourceVChan_type* init_SourceVChan_type	(char 							name[], 
										 Packet_type 					dataType,
										 void* 							VChanOwner,
										 Connected_CBFptr_type			Connected_CBFptr,
										 Disconnected_CBFptr_type		Disconnected_CBFptr)
{
	SourceVChan_type*	vchan 	= malloc(sizeof(SourceVChan_type));
	if (!vchan) return NULL;
	
	// init base VChan type
	if (init_VChan_type ((VChan_type*) vchan, name, VChan_Source, VChanOwner, 
						 discard_SourceVChan_type, disconnectSourceVChan, Connected_CBFptr, Disconnected_CBFptr) < 0) goto Error;
	
	// init list with Sink VChans
	if (!(vchan -> sinkVChans 	= ListCreate(sizeof(SinkVChan_type*)))) goto Error;
	
	// init data packet type
	vchan->dataType = dataType;
	
	
	return vchan;
	Error:
	
	discard_VChan_type ((VChan_type**)&vchan);  // do this last
	return NULL;
}

SinkVChan_type* init_SinkVChan_type	(char 						name[], 
									 Packet_type	 			dataTypes[],
									 size_t						nDataTypes,
									 void* 						VChanOwner,
									 Connected_CBFptr_type		Connected_CBFptr,
									 Disconnected_CBFptr_type	Disconnected_CBFptr)
{
	SinkVChan_type*	vchan = malloc(sizeof(SinkVChan_type));
	if (!vchan) return NULL;
	
	// init
	vchan->dataTypes	= NULL;
	
	// init base VChan type
	if (init_VChan_type ((VChan_type*) vchan, name, VChan_Sink, VChanOwner, 
						 discard_SinkVChan_type, disconnectSinkVChan, Connected_CBFptr, Disconnected_CBFptr) < 0) goto Error;
	
		// INIT DATA
		
	vchan->sourceVChan 	= NULL;
	vchan->nDataTypes	= nDataTypes;
	// copy data types
	vchan->dataTypes	= malloc(nDataTypes * sizeof(Packet_type));
	if (!vchan->dataTypes) goto Error;
	memcpy(vchan->dataTypes, dataTypes, nDataTypes*sizeof(Packet_type));
	
	// init thread safe queue
	CmtNewTSQ(DEFAULT_SinkVChan_QueueSize, sizeof(DataPacket_type*), 0, &vchan->tsqHndl); 
	// init write timeout (time to keep on trying to write a data packet to the queue)
	vchan->writeTimeout = DEFAULT_SinkVChan_QueueWriteTimeout;
	
	return vchan;
	Error:
	
	OKfree(vchan->dataTypes);
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
static void init_DataPacket_type (DataPacket_type* dataPacket, Packet_type dataType, size_t n, void* data, DiscardDataPacketFptr_type discardFptr)
{
	dataPacket -> dataType 		= dataType;
	dataPacket -> n				= n;
	dataPacket -> data     		= data;
	dataPacket -> discardFptr   = discardFptr;
	
	// create counter to keep track of how many sinks still need this data packet
	CmtNewTSV(sizeof(int), &dataPacket->ctr);
	// set counter to 0
	int* 	ctrTSVptr;
	
	CmtGetTSVPtr(dataPacket->ctr, &ctrTSVptr);
	*ctrTSVptr = 0;
	CmtReleaseTSVPtr(dataPacket->ctr);
}

void discard_DataPacket_type(DataPacket_type** dataPacket)
{
	if (!*dataPacket) return;
	
	(*(*dataPacket)->discardFptr) (dataPacket);
}

void ReleaseDataPacket (DataPacket_type** dataPacket)
{
	int* ctrTSVptr;
	
	if (!*dataPacket) return;
	
	CmtGetTSVPtr((*dataPacket)->ctr, &ctrTSVptr);
	if (*ctrTSVptr > 1) {
		(*ctrTSVptr)--;
		CmtReleaseTSVPtr((*dataPacket)->ctr);
	} else {CmtReleaseTSVPtr((*dataPacket)->ctr); discard_DataPacket_type(dataPacket);} 
}

FCallReturn_type* ReleaseAllDataPackets (SinkVChan_type* sinkVChan)
{
	DataPacket_type**		dataPackets		= NULL;
	size_t					nPackets;
	FCallReturn_type*		fCallReturn		= NULL;
	
	if ((fCallReturn = GetAllDataPackets (sinkVChan, &dataPackets, &nPackets))) 
		return fCallReturn; // error
	
	// release data packets
	for(size_t i = 0; i < nPackets; i++)
		ReleaseDataPacket(&dataPackets[i]);
		
	OKfree(dataPackets);
	
	return NULL;
}

FCallReturn_type* SendDataPacket (SourceVChan_type* source, DataPacket_type* dataPacket)
{
#define SendDataPacket_Err_TSQWrite		-1
	
	int		nSinks	= ListNumItems(source->sinkVChans);
	
	// if there are no Sink VChans, then dispose of the data and do nothing
	if (!nSinks) {
		ReleaseDataPacket(&dataPacket);
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
		itemsWritten = CmtWriteTSQData((*sinkPtrPtr)->tsqHndl, &dataPacket, 1, (*sinkPtrPtr)->writeTimeout, NULL);
		
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

/// HIFN Gets all data packets from a Sink VChan. The function allocates dynamically a data packet array with nPackets elements of DataPacket_type*
/// HIFN If there are no data packets in the Sink VChan, dataPackets = NULL, nPackets = 0 and the function returns NULL (success).
/// HIFN If an error is encountered, the function returns an FCallReturn_type* with error information and sets dataPackets to NULL and nPackets to 0.
/// HIRET 0 if successful, and negative error code otherwise.
/// OUT dataPackets, nPackets
FCallReturn_type* GetAllDataPackets (SinkVChan_type* sinkVChan, DataPacket_type*** dataPackets, size_t* nPackets)
{
#define ReleaseAllDataPackets_Err_OutOfMem		-1
	
	int						error				= 0;
	CmtTSQHandle			sinkVChanTSQHndl 	= sinkVChan->tsqHndl;
	
	// get number of available packets
	errChk(CmtGetTSQAttribute(sinkVChanTSQHndl, ATTR_TSQ_ITEMS_IN_QUEUE, nPackets));
	if (!*nPackets) {
		*dataPackets = NULL;
		return NULL;
	}
	
	// get data packets
	*dataPackets = malloc (*nPackets * sizeof(DataPacket_type*));
	if (!*dataPackets) return init_FCallReturn_type(ReleaseAllDataPackets_Err_OutOfMem, "GetAllDataPackets", "Out of memory");
	
	errChk(CmtReadTSQData(sinkVChanTSQHndl, *dataPackets, *nPackets, 0, 0));
	
	return NULL;
	
Error:
	char	errMsg[CMT_MAX_MESSAGE_BUF_SIZE];
	CmtGetErrorMessage (error, errMsg);
	return init_FCallReturn_type(error, "GetAllDataPackets", errMsg);
}

void* GetDataPacketDataPtr (DataPacket_type* dataPacket, size_t* n)  
{
	if (n) *n = dataPacket->n;
	
	return dataPacket->data;
}

BOOL CompatibleVChans (SourceVChan_type* srcVChan, SinkVChan_type* sinkVChan)
{
	for (size_t i = 0; i < sinkVChan->nDataTypes; i++)
		if (srcVChan->dataType == sinkVChan->dataTypes[i])
			return TRUE;
			
	return FALSE;		
}


/// HIFN Connects a Source and Sink VChan and if provided, calls their Connected_CBFptr callbacks to signal this event.
/// HIFN The Source VChan's Connected_CBFptr will be called before the Sink's Connected_CBFptr. 
/// HIRET TRUE if successful, FALSE otherwise
BOOL VChan_Connect (SourceVChan_type* srcVChan, SinkVChan_type* sinkVChan)
{
	SinkVChan_type** sinkPtrPtr;
	
	if (!srcVChan || !sinkVChan) return FALSE;
	// check if sink is already connected
	for (int i = 1; i <= ListNumItems(srcVChan->sinkVChans); i++) {
		sinkPtrPtr = ListGetPtrToItem(srcVChan->sinkVChans, i);
		if (*sinkPtrPtr == sinkVChan) return FALSE;
	}
	// if sink is already connected to another source, disconnect that sink from source
	if (sinkVChan->sourceVChan) 
		if(!VChan_Disconnect((VChan_type*)sinkVChan) ) return FALSE;
		
	// add sink to source's list of sinks
	if (!ListInsertItem(srcVChan->sinkVChans, &sinkVChan, END_OF_LIST)) return FALSE;
	// add source reference to sink
	sinkVChan->sourceVChan = srcVChan;
	
	// call source and sink connect callbacks if provided
	if (srcVChan->baseClass.Connected_CBFptr)
		(*srcVChan->baseClass.Connected_CBFptr)	((VChan_type*)srcVChan, (VChan_type*)sinkVChan);
	
	if (sinkVChan->baseClass.Connected_CBFptr)
		(*sinkVChan->baseClass.Connected_CBFptr)	((VChan_type*)sinkVChan, (VChan_type*)srcVChan);
	
	return TRUE;	
}

/// HIFN Disconnects a VChan and if provided, calls its Disconnected_CBFptr callback to signal this event.
/// HIFN When disconnecting a Sink, its Source is detached and it is also removed from the Source's list of Sinks.
/// HIFN When disconnecting a Source, it is removed from all its Sinks.
/// HIRET TRUE if successful, FALSE otherwise
BOOL VChan_Disconnect (VChan_type* VChan)
{
	return (*VChan->DisconnectFptr)	(VChan);
}


//==============================================================================
// Set/Get functions

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
	SinkVChan_type** sinkVChanPtr = ListGetPtrToItem(srcVChan->sinkVChans, sinkIdx);
	
	if (!*sinkVChanPtr) return NULL;
	
	return GetVChanName((VChan_type*)*sinkVChanPtr);
}

SinkVChan_type*	GetSinkVChan (SourceVChan_type* srcVChan, size_t sinkIdx)
{
	SinkVChan_type** sinkVChanPtr = ListGetPtrToItem(srcVChan->sinkVChans, sinkIdx);
	
	return *sinkVChanPtr;
}

SourceVChan_type* GetSourceVChan (SinkVChan_type* sinkVChan)
{
	return sinkVChan->sourceVChan;
}

VChanDataFlow_type GetVChanDataFlowType (VChan_type* VChan)
{
	return VChan->dataFlow;
}

size_t GetNSinkVChans (SourceVChan_type* srcVChan)
{
	return ListNumItems(srcVChan->sinkVChans);
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

void SetSinkVChanWriteTimeout (SinkVChan_type* sinkVChan, double time)
{
	sinkVChan->writeTimeout = time;
}

double GetSinkVChanWriteTimeout	(SinkVChan_type* sinkVChan)
{
	return sinkVChan->writeTimeout;
}

void* GetPtrToVChanOwner (VChan_type* VChan)
{
	return VChan->VChanOwner;
}

//==============================================================================
// VChan data types management

DataPacket_type* init_WaveformPacket_type (Waveform_type waveformType, size_t n, void* waveform, double rate, double nRepeat)
{
	WaveformPacket_type* 	packet = malloc (sizeof(WaveformPacket_type));
	if (!packet) return NULL;
	
	// map waveform data type to packet data type
	Packet_type		packetType;
	switch (waveformType) {
		case Waveform_Char:
			packetType = WaveformPacket_Char;
			break;
		
		case Waveform_UChar:
			packetType = WaveformPacket_UChar;
			break;
			
		case Waveform_Short:
			packetType = WaveformPacket_Short;
			break;
			
		case Waveform_UShort:
			packetType = WaveformPacket_UShort;
			break;
			
		case Waveform_Int:
			packetType = WaveformPacket_Int;
			break;
			
		case Waveform_UInt:
			packetType = WaveformPacket_UInt;
			break;
			
		case Waveform_SSize:
			packetType = WaveformPacket_SSize;
			break;
			
		case Waveform_Size:
			packetType = WaveformPacket_Size;
			break;
			
		case Waveform_Float:
			packetType = WaveformPacket_Float;
			break;
			
		case Waveform_Double:
			packetType = WaveformPacket_Double;
			break;
			
		default:
			// mapping error
			free(packet);
			return NULL;
	}
	
	// init DataPacket base class
	init_DataPacket_type(&packet->baseClass, packetType, n, waveform, discard_WaveformPacket_type); 
	
	// init WaveformPacket
	
	packet -> rate		= rate;
	packet -> nRepeat	= nRepeat;
	
	return (DataPacket_type*) packet;
}

double GetWaveformPacketRepeat (WaveformPacket_type* waveformPacket)
{
	return waveformPacket->nRepeat;
}

static void discard_WaveformPacket_type (DataPacket_type** waveformPacket)
{
	if (!*waveformPacket) return;
	
	//-----------------------------------
	// Discard WaveformPacket data
	//-----------------------------------
	OKfree((*waveformPacket)->data);
	
	//-----------------------------------
	// Discard DataPacket base class data
	//-----------------------------------
	CmtDiscardTSV((*waveformPacket)->ctr);
	
	OKfree(*waveformPacket);
}

DataPacket_type* init_NIVisionImagePacket_type (size_t n, Image* images)
{
	ImagePacket_type* imagePacket = malloc (sizeof(ImagePacket_type));
	if (!imagePacket) return NULL;
	
	// init DataPacket base class
	init_DataPacket_type(&imagePacket->baseClass, ImagePacket_NIVision, n, images, discard_NIVisionImagePacket_type);
	
	return (DataPacket_type*) imagePacket;
}

static void	discard_NIVisionImagePacket_type (DataPacket_type** imagePacket)
{
	size_t		nImages 	= (*imagePacket)->n;
	Image**		images		= (*imagePacket)->data;
	
	//-----------------------------------
	// Discard ImagePacket data
	//-----------------------------------
	// discard images
	for (size_t i = 0; i < nImages; i++) {
		imaqDispose(images[i]);
		images[i] = NULL;
	}
	// discard array
	OKfree((*imagePacket)->data);
			
	//-----------------------------------
	// Discard DataPacket base class data
	//-----------------------------------
	CmtDiscardTSV((*imagePacket)->ctr);
	
	OKfree(*imagePacket);	
}

/// HIFN Updates a table control with connections between a given list of VChans.
void UpdateSwitchboard (ListType VChans, int panHandle, int tableControlID)
{
	size_t          		nColumns				= 0; 
	size_t          		nRows					= 0;
	int             		nItems;
	int             		colWidth;
	int  					colIdx;
	SourceVChan_type** 		sourceVChanPtr			= NULL;
	VChan_type**			VChanPtr1				= NULL;
	SinkVChan_type**		sinkVChanPtr			= NULL;
	Point           		cell;
	Rect            		cellRange;
	BOOL            		assigned;
	BOOL					referenceExists;
	BOOL					extraRow;
	size_t					nVChans					= ListNumItems(VChans);
	size_t					nSinks;
	
	if (!panHandle) return; // do nothing if panel is not loaded or list is not initialized
	if (!VChans) return;
	
	// store a reference to the VChan list in a global variable within the scope of this file
	SwitchboardVChanList = VChans;
	// attach callback function to table control
	SetCtrlAttribute(panHandle, tableControlID, ATTR_CALLBACK_FUNCTION_POINTER, Switchboard_CB);
	
	DeleteTableColumns(panHandle, tableControlID, 1, -1);
	DeleteTableRows(panHandle, tableControlID, 1, -1);
	
	// insert source VChans as columns
	for (size_t chanIdx = 1; chanIdx <= nVChans; chanIdx++) {
		sourceVChanPtr = ListGetPtrToItem(VChans, chanIdx);
		if ((*sourceVChanPtr)->baseClass.dataFlow == VChan_Source) { 
			InsertTableColumns(panHandle, tableControlID, -1, 1, VAL_CELL_COMBO_BOX);
			nColumns++;
			
			// adjust table display
			SetTableColumnAttribute(panHandle, tableControlID, nColumns, ATTR_USE_LABEL_TEXT, 1);
			SetTableColumnAttribute(panHandle, tableControlID, nColumns, ATTR_LABEL_JUSTIFY, VAL_CENTER_CENTER_JUSTIFIED);
			SetTableColumnAttribute(panHandle, tableControlID, nColumns, ATTR_CELL_TYPE, VAL_CELL_COMBO_BOX);
			SetTableColumnAttribute(panHandle, tableControlID, nColumns, ATTR_LABEL_TEXT, (*sourceVChanPtr)->baseClass.name); 
			
			// add rows and create combo boxes for the sinks of this source VChan if there are not enough rows already
			nSinks = GetNSinkVChans(*sourceVChanPtr);
			if (nSinks > nRows) {
				InsertTableRows(panHandle, tableControlID, -1, nSinks - nRows, VAL_CELL_COMBO_BOX);
				nRows += nSinks - nRows;
			}
			
			// add assigned sink VChans
			for (size_t idx = 0; idx < nSinks; idx++) {
				cell.x = nColumns;
				cell.y = idx + 1;
				sinkVChanPtr = ListGetPtrToItem((*sourceVChanPtr)->sinkVChans, idx+1);
				InsertTableCellRingItem(panHandle, tableControlID, cell, -1, (*sinkVChanPtr)->baseClass.name);
				// select the assigned sink VChan
				SetTableCellValFromIndex(panHandle, tableControlID, cell, 0); 
			}
			
			// add unassigned sink VChans of the same type
			extraRow = 0;
			for (size_t vchanIdx = 1; vchanIdx <= nVChans; vchanIdx++) {
				VChanPtr1 = ListGetPtrToItem(VChans, vchanIdx);
				if ((*VChanPtr1)->dataFlow != VChan_Sink) continue; // select only sinks
				
				if (!GetSourceVChan((SinkVChan_type*)*VChanPtr1)) assigned = FALSE;
				else assigned = TRUE;
						
				// add Sink VChans only if:
				// a) useAsReference is FALSE (in which case it picks up VChan properties of the source if useAsReference of the Source is set to TRUE).
				// b) useAsReference is TRUE and the following conditions are simultaneously met:
				//		1) Source VChan to which the Sink can be connected has useAsReference FALSE. In this case the Source and all its Sinks will pick up the same VChan properties from the Sink VChan.
				//		2) There are no other Sink VChans assigned to the source that have useAsReference TRUE.
				
				referenceExists = (*sourceVChanPtr)->baseClass.useAsReference; // if Source VChan has useAsReference TRUE, then there is already a reference.
				nSinks = GetNSinkVChans(*sourceVChanPtr); 
				if (!referenceExists)
					// if Source VChan is not used as a reference, then check if any of its Sinks is used as a reference
					for (size_t i = 1; i <= nSinks; i++) {
						sinkVChanPtr = ListGetPtrToItem((*sourceVChanPtr)->sinkVChans, i);
						// check if any of the sinks has a useAsReference set to TRUE
						if ((*sinkVChanPtr)->baseClass.useAsReference) {referenceExists = TRUE; break;} 
					}
				
				if (!assigned && CompatibleVChans(*sourceVChanPtr, (SinkVChan_type*)*VChanPtr1) && (!(*VChanPtr1)->useAsReference || ((*VChanPtr1)->useAsReference && !referenceExists))) {
					if (!extraRow && (nRows <= nSinks)) {
						InsertTableRows(panHandle, tableControlID, -1, 1, VAL_CELL_COMBO_BOX);
						extraRow = 1;
						nRows++;
					}
					cellRange.top    = 1;
					cellRange.left   = nColumns;
					cellRange.height = nSinks + 1; 
					cellRange.width  = 1;
					InsertTableCellRangeRingItem(panHandle, tableControlID, cellRange, -1, (*VChanPtr1)->name);
				}
			} 
			
			if (nRows) {
				// insert empty selection
				cellRange.top    = 1;
				cellRange.left   = nColumns;
				cellRange.height = nRows; 
				cellRange.width  = 1;
				InsertTableCellRangeRingItem(panHandle, tableControlID, cellRange, 0, "");
			}
			
			// make column slightly wider than contents for better visibility
			SetColumnWidthToWidestCellContents(panHandle, tableControlID, nColumns);
			GetTableColumnAttribute(panHandle, tableControlID, nColumns, ATTR_COLUMN_ACTUAL_WIDTH, &colWidth);
			SetTableColumnAttribute(panHandle, tableControlID, nColumns, ATTR_COLUMN_WIDTH, colWidth + 20); 
			
		}
		
	}
	
	// dim ring controls with only empty selection
	for (size_t i = 1; i <= nRows; i++) {
		colIdx = 0;
		for (size_t j = 1; j <= nVChans	; j++) {
			VChanPtr1 = ListGetPtrToItem(VChans, j);
			if ((*VChanPtr1)->dataFlow == VChan_Sink) continue;   // count only source VChans
			colIdx++;
			cell.x = colIdx; 
			cell.y = i;
			GetNumTableCellRingItems(panHandle, tableControlID, cell, &nItems);
			if (nItems <= 1) SetTableCellAttribute(panHandle, tableControlID, cell, ATTR_CELL_DIMMED, 1);
		}
	}
}

// callback to operate the VChan switchboard
static int CVICALLBACK Switchboard_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	if (event != EVENT_COMMIT) return 0; // continue only if event is commit
	
	// eventData1 - 1-based row index
	// eventData2 - 1-based column index
	SourceVChan_type*		sourceVChan;
	SinkVChan_type*			sinkVChan;
	char*         			sourceVChanName;
	char*         			sinkVChanName;
	int	        			nChars;
	Point        			cell;
	
	// find source VChan from the table column in the list
	GetTableColumnAttribute(panel, control, eventData2, ATTR_LABEL_TEXT_LENGTH, &nChars);
	sourceVChanName = malloc((nChars+1) * sizeof(char));
	if (!sourceVChanName) return 0;
	
	GetTableColumnAttribute(panel, control, eventData2, ATTR_LABEL_TEXT, sourceVChanName);
	sourceVChan = (SourceVChan_type*)VChanNameExists (SwitchboardVChanList, sourceVChanName, 0);
	// read in new sink VChan name from the ring control
	cell.x = eventData2;
	cell.y = eventData1;
	GetTableCellValLength(panel, control, cell, &nChars);
	sinkVChanName = malloc((nChars+1) * sizeof(char));
	if (!sinkVChanName) return 0;
	
	GetTableCellVal(panel, control, cell, sinkVChanName);
	
	size_t nSinks = GetNSinkVChans(sourceVChan);
			
	// disconnect selected Sink VChan from Source VChan
	if (nSinks && (eventData1 <= nSinks)) {
		sinkVChan = GetSinkVChan(sourceVChan, eventData1);
		VChan_Disconnect((VChan_type*)sinkVChan);
	}
		
	// reconnect new Sink VChan to the Source Vchan
	if (strcmp(sinkVChanName, "")) {
		sinkVChan =	(SinkVChan_type*)VChanNameExists(SwitchboardVChanList, sinkVChanName, 0);
		VChan_Connect(sourceVChan, sinkVChan);
	}
			
	// update table
	UpdateSwitchboard(SwitchboardVChanList, panel, control);
			
	OKfree(sourceVChanName);
	OKfree(sinkVChanName);
	
	return 0;
}
