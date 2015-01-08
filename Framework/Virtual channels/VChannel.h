//==============================================================================
//
// Title:		VChannel.h
// Purpose:		A short description of the interface.
//
// Created on:	18-6-2014 at 19:11:05 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __VChannel_H__
#define __VChannel_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files
		
#include "DAQLabUtility.h"
#include "DataPacket.h"
#include "DataTypes.h"

//==============================================================================
// Constants

//==============================================================================
// Types

	// VChan types
typedef struct VChan			VChan_type;					// base class
typedef struct SinkVChan 		SinkVChan_type;
typedef struct SourceVChan 		SourceVChan_type;

//------------------------------------------------------------------------------
// Virtual channel data flow direction
//------------------------------------------------------------------------------

typedef enum {
	VChan_Sink, 						 
	VChan_Source
} VChanDataFlow_type;

//------------------------------------------------------------------------------
// Callback functions
//------------------------------------------------------------------------------

	// Callback when a VChan connection is established between a Source and Sink
typedef void	(* Connected_CBFptr_type) 		(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);

	// Callback when a VChan connection is broken by calling VChan_Disconnect. When disconnecting
	// a Source type VChan, this callback will be called for each Sink.
typedef void	(* Disconnected_CBFptr_type) 	(VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan);


//==============================================================================
// Global functions

//------------------------------------------------------------------------------
// VChan Creation / Destruction and management functions
//------------------------------------------------------------------------------

SourceVChan_type*			init_SourceVChan_type				(char 						name[], 
										  	  	 	 			DLDataTypes 				dataType,
										 	  	 				void* 						VChanOwner,
												 	 			Connected_CBFptr_type		Connected_CBFptr,
												 	 			Disconnected_CBFptr_type	Disconnected_CBFptr);

	// Creates a Sink VChan which may support multiple data types. Provide an array of dataTypes of DLDataTypes elements.
SinkVChan_type*				init_SinkVChan_type					(char 						name[], 
										  	  	 	 			DLDataTypes					dataTypes[],
																size_t						nDataTypes,
										 	  	 	 			void* 						VChanOwner,
																double						readTimeout,
												 	 			Connected_CBFptr_type		Connected_CBFptr,
												 	 			Disconnected_CBFptr_type	Disconnected_CBFptr);

	// Discard common to both types of VChan. Cast SourceVChan_type** and SinkVChan_type** to VChan_type**.
void 						discard_VChan_type 					(VChan_type** VChan); 

	// Checks if a VChan object exists among a list of VChans of VChan_type*.
	// If found and *idx is provided, it returns the 1-based VChan index in the list, otherwise *idx=0.
BOOL						VChanExists							(ListType VChanList, VChan_type* VChan, size_t* idx);

	// Searches for a given VChan name among a list of VChans of VChan_type*.
	// If found and *idx is provided, it returns the 1-based VChan index in the list, otherwise *idx=0.
VChan_type*					VChanNameExists						(ListType VChanList, char VChanName[], size_t* idx);

	// Returns a unique VChan name among a given list of VChans. The unique VChan name has a base name and a number.
	// VChanList of VChan_type*
char*						GetUniqueVChanName					(ListType VChanList, char baseVChanName[]);

//------------------------------------------------------------------------------
// VChan Connections
//------------------------------------------------------------------------------

	// Checks if the data packet types supported by the Sink and Source VChan are compatible. If compatible, returns TRUE, FALSE otherwise
BOOL						CompatibleVChans					(SourceVChan_type* srcVChan, SinkVChan_type* sinkVChan);

	// Connects a Sink and a Source VChan .
BOOL						VChan_Connect						(SourceVChan_type* srcVChan, SinkVChan_type* sinkVChan);

	// Disconnects a VChan. 
BOOL						VChan_Disconnect					(VChan_type* VChan); 

	// Determines whether the VChan is connected to another VChan
BOOL						IsVChanConnected					(VChan_type* VChan);

//------------------------------------------------------------------------------
// VChan Set/Get
//------------------------------------------------------------------------------

	// Assigns new VChan name
void 						SetVChanName 						(VChan_type* VChan, char newName[]);

	// Returns pointer to dynamically allocated VChan name (null terminated string) 
char*						GetVChanName						(VChan_type* VChan);

char*						GetSinkVChanName					(SourceVChan_type* srcVChan, size_t sinkIdx);

SinkVChan_type*				GetSinkVChan						(SourceVChan_type* srcVChan, size_t sinkIdx);

SourceVChan_type*			GetSourceVChan						(SinkVChan_type* sinkVChan);

	// Changes the data types a SinkVChan accepts. If there is a Source VChan attached to the Sink VChan, and the data type
	// of the Source VChan is incompatible with the provided types, then the Sink VChan is disconnected from the Source VChan.
void						SetSinkVChanDataTypes				(SinkVChan_type* sinkVChan, size_t nDataTypes, DLDataTypes dataTypes[]);

VChanDataFlow_type			GetVChanDataFlowType				(VChan_type* VChan);

size_t						GetNSinkVChans						(SourceVChan_type* srcVChan);

ListType					GetSinkVChanList					(SourceVChan_type* srcVChan);

	// Returns the thread safe queue handle of a sink VChan
CmtTSQHandle				GetSinkVChanTSQHndl					(SinkVChan_type* sinkVChan);

	// The maximum number of datapackets a Sink VChan can hold
void						SetSinkVChanTSQSize					(SinkVChan_type* sinkVChan, size_t nItems);

size_t						GetSinkVChanTSQSize					(SinkVChan_type* sinkVChan);

	// Time in [ms] to keep on trying to read a data packet from a Sink VChan TSQ		
void						SetSinkVChanReadTimeout				(SinkVChan_type* sinkVChan, double time);	
double						GetSinkVChanReadTimeout				(SinkVChan_type* sinkVChan);

void*						GetVChanOwner						(VChan_type* VChan);

//------------------------------------------------------------------------------
// Data Packet Management
//------------------------------------------------------------------------------

	// Releases all data packets from a Sink VChan. 
	// If successful, returns NULL, otherwise it returns error information which must be disposed of with discard_FCallReturn_type
int							ReleaseAllDataPackets				(SinkVChan_type* sinkVChan, char** errorInfo);

	// Sends a data packet from a Source VChan to its Sink VChans. If the Source VChan also needs to use the data packet after it was sent
	// then set sourceNeedsPacket = TRUE. This prevents the release of the data packet by the sinks alone.
int				 			SendDataPacket 						(SourceVChan_type* source, DataPacket_type** ptrToDataPacket, BOOL sourceNeedsPacket, char** errorInfo);

	// Gets all data packets from a Sink VChan. The function allocates dynamically a data packet array with nPackets elements of DataPacket_type*
	// If there are no data packets in the Sink VChan, dataPackets = NULL, nPackets = 0 and the function returns NULL (success).
	// If an error is encountered, the function returns an FCallReturn_type* with error information and sets dataPackets to NULL and nPackets to 0.
	// For datapackets pass the address of a DataPacket_type** variable.
int							GetAllDataPackets					(SinkVChan_type* sinkVChan, DataPacket_type*** dataPackets, size_t* nPackets, char** errorInfo);

int				 			GetDataPacket 						(SinkVChan_type* sinkVChan, DataPacket_type** dataPacketPtr, char** errorInfo);

//------------------------------------------------------------------------------ 
// Waveform data management
//------------------------------------------------------------------------------ 

	// Receives waveform data from a Sink VChan which is assembled from multiple data packets until a NULL packets is encountered. The function returns the 
	// dynamically allocated waveform in case there is waveform data or NULL if the first data packet read from the VChan is NULL. On success, the function 
	// return 0 and negative number plus an error message if it fails.
int							ReceiveWaveform						(SinkVChan_type* sinkVChan, Waveform_type** waveform, WaveformTypes* waveformType, char** errorInfo);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __VChannel_H__ */
