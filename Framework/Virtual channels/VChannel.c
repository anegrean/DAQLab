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
#define DEFAULT_SinkVChan_QueueWriteTimeout	1000.0	// number of [ms] to wait while trying
													// to add a data packet to a Sink VChan TSQ

//==============================================================================
// Types

// Discard VChan function pointer type
typedef void	(* DiscardVChanFptr_type) 			(VChan_type** vchan);

// Disconnect VChan function pointer type
typedef BOOL	(* DisconnectVChanFptr_type)		(VChan_type* vchan);

// Determines if a VChan is connected to other VChans or not
typedef BOOL 	(* VChanIsConnected_type)			(VChan_type* VChan);  


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
	VChanDataFlow_type 				dataFlow;   			// Direction of data flow into or out of the channel.
														
	//-----------------------	
	// Source code connection
	//-----------------------
	void*							VChanOwner;				// Reference to object that owns the VChan.
	
	//-----------------------
	// Methods
	//-----------------------
	
	DiscardVChanFptr_type			DiscardFptr;			// Discards VChan type-specific data. 
	
	DisconnectVChanFptr_type		DisconnectFptr;			// Disconnects a VChan.
	
	VChanIsConnected_type			VChanIsConnectedFptr;   // Determines if a VChan is connected to another VChan
	
	//-----------------------
	// Callbacks
	//-----------------------
	
	Connected_CBFptr_type			Connected_CBFptr;		// Callback when another VChan has been connected to this one.
	
	Disconnected_CBFptr_type		Disconnected_CBFptr;   	// Callback when another VChan has been disconnected from this one.
	
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

static BOOL 				SourceVChanIsConnected 				(VChan_type* VChan);
static BOOL 				SinkVChanIsConnected 				(VChan_type* VChan);


static int 					init_VChan_type 					(VChan_type* 					vchan, 
																char 							name[], 
										 						VChanDataFlow_type 				flowType, 
														 		void* 							VChanOwner, 	
										 				 		DiscardVChanFptr_type 			DiscardFptr,
										 				 		DisconnectVChanFptr_type		DisconnectFptr,
																Connected_CBFptr_type			Connected_CBFptr,
										 				 		Disconnected_CBFptr_type		Disconnected_CBFptr,
																VChanIsConnected_type			VChanIsConnectedFptr)
{
	// Data
	vchan -> name   				= StrDup(name); 
	if (!vchan->name) return -1;
	vchan -> dataFlow				= flowType;
	vchan -> VChanOwner				= VChanOwner;
	
	// Callbacks
	vchan -> Connected_CBFptr		= Connected_CBFptr;
	vchan -> Disconnected_CBFptr	= Disconnected_CBFptr;
	
	// Methods
	vchan -> DiscardFptr			= DiscardFptr;
	vchan -> DisconnectFptr			= DisconnectFptr;
	vchan -> VChanIsConnectedFptr	= VChanIsConnectedFptr;
	
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
	char*					errMsg		= NULL;
	
	if (!*vchan) return;
	
	// disconnect Sink from Source if connected
	VChan_Disconnect(*vchan);
	
	// release any data packets still in the VChan TSQ
	ReleaseAllDataPackets (sink, &errMsg); 
	
	// discard data packet types array
	OKfree(sink->dataTypes);
	
	// discard Sink VChan specific data 
	CmtDiscardTSQ(sink->tsqHndl);
	
	// discard base VChan data
	OKfree((*vchan)->name);
	
	OKfree(*vchan);
	OKfree(errMsg);
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
			(*(*sinkPtrPtr)->baseClass.Disconnected_CBFptr)	((VChan_type*)(*sinkPtrPtr), (*sinkPtrPtr)->baseClass.VChanOwner, (VChan_type*) src);
		
		// call source disconnected callback
		if (src->baseClass.Disconnected_CBFptr)
			(*src->baseClass.Disconnected_CBFptr)	((VChan_type*)src, src->baseClass.VChanOwner, (VChan_type*)(*sinkPtrPtr));
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
			(*sink->baseClass.Disconnected_CBFptr)	((VChan_type*)sink, sink->baseClass.VChanOwner, (VChan_type*) src);
	
	// remove sink from source's sink list
	for (int i = 1; i <= ListNumItems(src->sinkVChans); i++) {
		sinkPtrPtr = ListGetPtrToItem(src->sinkVChans, i);
		if (*sinkPtrPtr == sink) {
			ListRemoveItem(src->sinkVChans, 0, i);
			// call source disconnected callback
			if (src->baseClass.Disconnected_CBFptr)
				(*src->baseClass.Disconnected_CBFptr)	((VChan_type*)src, src->baseClass.VChanOwner, (VChan_type*)(*sinkPtrPtr));
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
										 DLDataTypes 					dataType,
										 void* 							VChanOwner,
										 Connected_CBFptr_type			Connected_CBFptr,
										 Disconnected_CBFptr_type		Disconnected_CBFptr)
{
	SourceVChan_type*	vchan 	= malloc(sizeof(SourceVChan_type));
	if (!vchan) return NULL;
	
	// init base VChan type
	if (init_VChan_type ((VChan_type*) vchan, name, VChan_Source, VChanOwner, 
						 discard_SourceVChan_type, disconnectSourceVChan, Connected_CBFptr, Disconnected_CBFptr, SourceVChanIsConnected) < 0) goto Error;
	
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
									 DLDataTypes	 			dataTypes[],
									 size_t						nDataTypes,
									 void* 						VChanOwner,
									 double						readTimeout,
									 Connected_CBFptr_type		Connected_CBFptr,
									 Disconnected_CBFptr_type	Disconnected_CBFptr)
{
	SinkVChan_type*	vchan 	= malloc(sizeof(SinkVChan_type));
	if (!vchan) return NULL;
	
	// init
	vchan->dataTypes		= NULL;
	
	// init base VChan type
	if (init_VChan_type ((VChan_type*) vchan, name, VChan_Sink, VChanOwner, 
						 discard_SinkVChan_type, disconnectSinkVChan, Connected_CBFptr, Disconnected_CBFptr, SinkVChanIsConnected) < 0) goto Error;
	
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
	size_t			nVChans		= ListNumItems(VChanList);
	
	for (size_t i = 1; i <= nVChans; i++) {
		VChanPtrPtr = ListGetPtrToItem(VChanList, i);
		if (!strcmp((*VChanPtrPtr)->name, VChanName)) {
			if (idx) *idx = i;
			return *VChanPtrPtr;
		}
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

int ReleaseAllDataPackets (SinkVChan_type* sinkVChan, char** errorInfo)
{
#define	ReleaseAllDataPackets_Err_CouldNotGetDataPackets	-1
	
	DataPacket_type**		dataPackets		= NULL;
	size_t					nPackets;
	char*					errMsg			= NULL;
	
	if (GetAllDataPackets (sinkVChan, &dataPackets, &nPackets, &errMsg) < 0) {
		if (errorInfo)
			*errorInfo = FormatMsg(ReleaseAllDataPackets_Err_CouldNotGetDataPackets, "ReleaseAllDataPackets", errMsg);
		OKfree(errMsg);
		return ReleaseAllDataPackets_Err_CouldNotGetDataPackets;
	}
	
	// release data packets
	for(size_t i = 0; i < nPackets; i++)
		ReleaseDataPacket(&dataPackets[i]);
		
	OKfree(dataPackets);
	
	return 0;
}

int SendDataPacket (SourceVChan_type* source, DataPacket_type** ptrToDataPacket, BOOL sourceNeedsPacket, char** errorInfo)
{
#define SendDataPacket_Err_TSQWrite		-1
	
	int		nSinks	=  ListNumItems(source->sinkVChans);
	int		error	= 0;
	
	// if there are no Sink VChans, then dispose of the data and do nothing
	if (!nSinks) {
		if (*ptrToDataPacket) ReleaseDataPacket(ptrToDataPacket);
		return 0; 
	}
	
	// set sinks counter
	if (*ptrToDataPacket)
		if (sourceNeedsPacket)
			SetDataPacketCounter(*ptrToDataPacket, nSinks+1);
		else
			SetDataPacketCounter(*ptrToDataPacket, nSinks);
	
	SinkVChan_type** 	sinkPtrPtr;
	int					itemsWritten;
	for (size_t i = 1; i <= nSinks; i++) {
		sinkPtrPtr = ListGetPtrToItem(source->sinkVChans,i);
		// put data packet into Sink VChan TSQ
		itemsWritten = CmtWriteTSQData((*sinkPtrPtr)->tsqHndl, ptrToDataPacket, 1, (*sinkPtrPtr)->writeTimeout, NULL);
		
		// check if writing the items to the sink queue succeeded
		if (itemsWritten < 0) {
			char* 				errMsg 										= StrDup("Writing data to ");
			char*				sinkName									= GetVChanName((VChan_type*)*sinkPtrPtr);
			char				cmtStatusMessage[CMT_MAX_MESSAGE_BUF_SIZE];
			
			AppendString(&errMsg, sinkName, -1); 
			AppendString(&errMsg, " failed. Reason: ", -1); 
			CmtGetErrorMessage(itemsWritten, cmtStatusMessage);
			AppendString(&errMsg, cmtStatusMessage, -1); 
			if (errorInfo)
				*errorInfo = FormatMsg(SendDataPacket_Err_TSQWrite, "SendDataPacket", errMsg);
			OKfree(errMsg);
			OKfree(sinkName);
			ReleaseDataPacket(ptrToDataPacket);
			error = SendDataPacket_Err_TSQWrite;
			
		} else
		   // check if the number of written elements is the same as what was requested
			if (!itemsWritten) {
				char* 				errMsg 										= StrDup("Sink VChan ");
				char*				sinkName									= GetVChanName((VChan_type*)*sinkPtrPtr);
			
				AppendString(&errMsg, sinkName, -1); 
				AppendString(&errMsg, " is full or write operation timed out", -1);
				if (errorInfo)
					*errorInfo = FormatMsg(SendDataPacket_Err_TSQWrite, "SendDataPacket", errMsg);
				OKfree(errMsg);
				OKfree(sinkName);
				ReleaseDataPacket(ptrToDataPacket);
				error = SendDataPacket_Err_TSQWrite;
			}
		
	}
	
	*ptrToDataPacket = NULL; 	// Data packet is considered to be consumed even if sending to some Sink VChans did not succeed
								// Sink VChans that did receive the data packet, can further process it and release it.
	
	return error;
}

/// HIFN Gets all data packets that are available from a Sink VChan. The function allocates dynamically a data packet array with nPackets elements of DataPacket_type*
/// HIFN If there are no data packets in the Sink VChan, dataPackets = NULL, nPackets = 0 and the function returns immediately with 0 (success).
/// HIFN If an error is encountered, the function returns a negative value with error information and sets dataPackets to NULL and nPackets to 0.
/// HIRET 0 if successful, and negative error code otherwise.
/// OUT dataPackets, nPackets
int GetAllDataPackets (SinkVChan_type* sinkVChan, DataPacket_type*** dataPackets, size_t* nPackets, char** errorInfo)
{
#define GetAllDataPackets_Err_OutOfMem			-1
#define GetAllDataPackets_Err_ReadDataPackets	-2  
	
	int						error				= 0;
	CmtTSQHandle			sinkVChanTSQHndl 	= sinkVChan->tsqHndl;
	
	// init
	*dataPackets 	= NULL;
	*nPackets		= 0;
	
	// get number of available packets
	errChk(CmtGetTSQAttribute(sinkVChanTSQHndl, ATTR_TSQ_ITEMS_IN_QUEUE, nPackets));
	if (!*nPackets) return 0;	// no data packets, return with success
	
	// get data packets
	*dataPackets = malloc (*nPackets * sizeof(DataPacket_type*));
	if (!*dataPackets) {
		if (errorInfo)
			*errorInfo 	= FormatMsg(GetAllDataPackets_Err_OutOfMem, "GetAllDataPackets", "Out of memory");
		*nPackets 	= 0;
		return GetAllDataPackets_Err_OutOfMem;
	}
	
	errChk(CmtReadTSQData(sinkVChanTSQHndl, *dataPackets, *nPackets, 0, 0));
	
	return 0;
	
Error:
	char	errMsg[CMT_MAX_MESSAGE_BUF_SIZE];
	CmtGetErrorMessage (error, errMsg);
	OKfree(*dataPackets);
	*nPackets 	= 0;
	if (errorInfo)
		*errorInfo 	= FormatMsg(GetAllDataPackets_Err_ReadDataPackets, "GetAllDataPackets", errMsg);
	return GetAllDataPackets_Err_ReadDataPackets;
}

/// HIFN Attempts to get one data packet from a Sink VChan before the timeout period. 
/// HIFN On success, it return 0, otherwise negative numbers. If an error is encountered, the function returns error information.
/// OUT dataPackets, nPackets
int GetDataPacket (SinkVChan_type* sinkVChan, DataPacket_type** dataPacketPtr, char** errorInfo)
{
#define GetDataPacket_Err_Timeout	-1
	
	int					error				= 0;
	CmtTSQHandle		sinkVChanTSQHndl 	= sinkVChan->tsqHndl;
	char*				errMsg				= NULL;
	
	*dataPacketPtr = NULL;
	
	// get data packet
	if ( (error = CmtReadTSQData(sinkVChanTSQHndl, dataPacketPtr, 1, sinkVChan->readTimeout, 0)) < 0) goto CmtError;
	
	// check if timeout occured
	if (!error) {
		errMsg = StrDup("Waiting for ");
		AppendString(&errMsg, sinkVChan->baseClass.name, -1);
		AppendString(&errMsg, " Sink VChan data timed out", -1);
		if (errorInfo)
			*errorInfo = FormatMsg(GetDataPacket_Err_Timeout, "GetDataPacket", errMsg);
		OKfree(errMsg);
		return GetDataPacket_Err_Timeout;
	}
	
	return 0;		 

CmtError:
	
	char	cmtErrMsg[CMT_MAX_MESSAGE_BUF_SIZE];
	CmtGetErrorMessage (error, cmtErrMsg);
	if (errorInfo)
		*errorInfo = FormatMsg(error, "GetDataPacket", cmtErrMsg);
	return error;
}

int	ReceiveWaveform (SinkVChan_type* sinkVChan, Waveform_type** waveform, WaveformTypes* waveformType, char** errorInfo)
{
#define		ReceiveWaveform_Err_NoWaveform		-1
#define		ReceiveWaveform_Err_WrongDataType	-2
	
	int						error				= 0;
	DataPacket_type*		dataPacket			= NULL;
	Waveform_type**			waveformPacketData	= NULL;
	DLDataTypes				dataPacketType;
	void*					dataPacketPtrToData	= NULL;
	char*					errMsg				= NULL;
	
	// init
	*waveform = NULL;
	
	// get first data packet and check if it is a NULL packet
	errChk ( GetDataPacket(sinkVChan, &dataPacket, &errMsg) );
	
	if (!dataPacket) {
		errMsg = FormatMsg(ReceiveWaveform_Err_NoWaveform, "ReceiveWaveform", "Waveform received does not contain any data. This occurs if \
								   a NULL packet is encountered before any data packets");
		error = ReceiveWaveform_Err_NoWaveform;
		goto Error;
	}
	
	// assemble waveform from multiple packets until a NULL packet is encountered
	while (dataPacket) {
		
		// get waveform data from the first non-NULL data packet and
		dataPacketPtrToData = GetDataPacketPtrToData(dataPacket, &dataPacketType); 
	
		// check if data packet is of waveform type
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
			
				errMsg = FormatMsg(ReceiveWaveform_Err_WrongDataType, "ReceiveWaveform", " Data packet received is not of a waveform type and cannot \
								   be retrieved by this function");
				ReleaseDataPacket(&dataPacket);
				error = ReceiveWaveform_Err_WrongDataType;
				goto Error;
		}
	
		waveformPacketData = dataPacketPtrToData;
		
		if (!*waveform)
			errChk( CopyWaveform(waveform, *waveformPacketData, &errMsg) );
		else
			errChk( AppendWaveform(*waveform, *waveformPacketData, &errMsg) );
	
		ReleaseDataPacket(&dataPacket);
		
		// get another packet
		errChk ( GetDataPacket(sinkVChan, &dataPacket, &errMsg) );
	}
	
	// check again if waveform is NULL
	
	if (!*waveform) {
		errMsg = FormatMsg(ReceiveWaveform_Err_NoWaveform, "ReceiveWaveform", "Waveform received does not contain any data. This occurs if \
								   a NULL packet is encountered before any data packets or the data packet doesn't have any data");
		error = ReceiveWaveform_Err_NoWaveform;
		goto Error;
	}
		
	return 0;
	
Error:
	
	ReleaseDataPacket(&dataPacket);
	if (errorInfo)
		*errorInfo = FormatMsg(error, "ReceiveWaveform", errMsg);
	OKfree(errMsg);
	
	return error;
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
		(*srcVChan->baseClass.Connected_CBFptr)	((VChan_type*)srcVChan, srcVChan->baseClass.VChanOwner, (VChan_type*)sinkVChan);
	
	if (sinkVChan->baseClass.Connected_CBFptr)
		(*sinkVChan->baseClass.Connected_CBFptr)	((VChan_type*)sinkVChan, sinkVChan->baseClass.VChanOwner, (VChan_type*)srcVChan);
	
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

BOOL IsVChanConnected (VChan_type* VChan)
{
	return (*VChan->VChanIsConnectedFptr) (VChan);
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

void SetSinkVChanDataTypes (SinkVChan_type* sinkVChan, size_t nDataTypes, DLDataTypes dataTypes[])
{
	if (!nDataTypes) return;
	
	if (sinkVChan->sourceVChan)
		for (size_t i = 0; i < nDataTypes; i++)
			if (dataTypes[i] != sinkVChan->sourceVChan->dataType) {
				VChan_Disconnect((VChan_type*)sinkVChan);
				break;
			}
	// copy new data types
	OKfree(sinkVChan->dataTypes);
	sinkVChan->nDataTypes 	= nDataTypes;
	sinkVChan->dataTypes	= malloc(nDataTypes * sizeof(DLDataTypes));
	memcpy(sinkVChan->dataTypes, dataTypes, nDataTypes*sizeof(DLDataTypes));
}

VChanDataFlow_type GetVChanDataFlowType (VChan_type* VChan)
{
	return VChan->dataFlow;
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

void* GetVChanOwner (VChan_type* VChan)
{
	return VChan->VChanOwner;
}

static BOOL SourceVChanIsConnected (VChan_type* VChan)
{
	if (ListNumItems(((SourceVChan_type*)VChan)->sinkVChans) )
		return TRUE;
	else
		return FALSE;
}
	
static BOOL SinkVChanIsConnected (VChan_type* VChan)
{
	if ( ((SinkVChan_type*)VChan)->sourceVChan )
		return TRUE;
	else
		return FALSE;
}
