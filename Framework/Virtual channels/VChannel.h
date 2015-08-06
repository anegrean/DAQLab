//==============================================================================
//
// Title:		VChannel.h
// Purpose:		Virtual Channels used for data exchange between DAQLab modules.
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
		
#include "DataPacket.h"
#include "DataTypes.h"

//==============================================================================
// Constants

//==============================================================================
// Types

	// VChan types
typedef struct VChan		VChan_type;					// base class
typedef struct SinkVChan 	SinkVChan_type;
typedef struct SourceVChan 	SourceVChan_type;


// Virtual channel data flow direction
typedef enum {
	VChan_Sink, 						 
	VChan_Source
} VChanDataFlows;

// Virtual channel states
typedef enum {
	VChan_Closed	= FALSE,
	VChan_Open		= TRUE
} VChanStates;

// Callback when a VChan opens/closes.
typedef void				(*VChanStateChangeCBFptr_type)		(VChan_type* self, void* VChanOwner, VChanStates state);


//==============================================================================
// Global functions

//------------------------------------------------------------------------------
// VChan Creation / Destruction and management functions
//------------------------------------------------------------------------------

	// Creates a Source VChan of a certain data type.
SourceVChan_type*			init_SourceVChan_type				(char 							name[], 
										  	  	 	 			 DLDataTypes 					dataType,
										 	  	 				 void* 							VChanOwner,
												 	 			 VChanStateChangeCBFptr_type	VChanStateChangeCBFptr);

	// Creates a Sink VChan which may support multiple data types. Provide an array of dataTypes of DLDataTypes elements.
SinkVChan_type*				init_SinkVChan_type					(char 							name[], 
										  	  	 	 			 DLDataTypes					dataTypes[],
																 size_t							nDataTypes,
										 	  	 	 			 void* 							VChanOwner,
																 double							readTimeout,
												 	 			 VChanStateChangeCBFptr_type	VChanStateChangeCBFptr);

	// Discards both Sink and Source VChans.
void 						discard_VChan_type 					(VChan_type** VChan); 

	// Checks if a VChan object exists among a list of VChans of VChan_type*.
	// If found and *idx is provided, it returns the 1-based VChan index in the list, otherwise *idx=0.
BOOL						VChanExists							(ListType VChanList, VChan_type* VChan, size_t* idx);

	// Searches for a given VChan name among a list of VChans of VChan_type*.
	// If found and *idx is provided, it returns the 1-based VChan index in the list, otherwise *idx=0.
VChan_type*					VChanNameExists						(ListType VChanList, char VChanName[], size_t* idx);

//------------------------------------------------------------------------------
// VChan Connections
//------------------------------------------------------------------------------

	// Determines if VChans have compatible data types.
BOOL						VChansAreCompatible					(SourceVChan_type* srcVChan, SinkVChan_type* sinkVChan);

	// Connects a Sink and a Source VChan.
BOOL						VChan_Connect						(SourceVChan_type* srcVChan, SinkVChan_type* sinkVChan);

	// Disconnects a VChan. 
BOOL						VChan_Disconnect					(VChan_type* VChan); 

	// Determines whether the VChan is connected to another VChan
BOOL						IsVChanConnected					(VChan_type* VChan);

//------------------------------------------------------------------------------
// VChan Set/Get/Is
//------------------------------------------------------------------------------

	// Name
void 						SetVChanName 						(VChan_type* VChan, char newName[]);
char*						GetVChanName						(VChan_type* VChan);
char*						GetSinkVChanName					(SourceVChan_type* srcVChan, size_t sinkIdx);

	// Data flow
VChanDataFlows				GetVChanDataFlowType				(VChan_type* VChan);

	// VChan owner
void*						GetVChanOwner						(VChan_type* VChan);

	// Data type
	// Changes the data types of a Source VChan. If there is a Sink VChan attached, and none of its data types are compatible 
	// with the Source VChan, then the Sink VChan is disconnected from the Source VChan.
void						SetSourceVChanDataType				(SourceVChan_type* srcVChan, DLDataTypes dataType);
DLDataTypes					GetSourceVChanDataType				(SourceVChan_type* srcVChan);
	// Changes the data types a SinkVChan accepts. If there is a Source VChan attached to the Sink VChan, and the data type
	// of the Source VChan is incompatible with the provided types, then the Sink VChan is disconnected from the Source VChan.
void						SetSinkVChanDataTypes				(SinkVChan_type* sinkVChan, size_t nDataTypes, DLDataTypes dataTypes[]);

	// Activate/Inactivate
void						SetVChanActive						(VChan_type* VChan, BOOL isActive);
BOOL						GetVChanActive						(VChan_type* VChan);

	// Is Opened/Closed?
BOOL						IsVChanOpen							(VChan_type* VChan);

	// Retrieve Sink & Source
SinkVChan_type*				GetSinkVChan						(SourceVChan_type* srcVChan, size_t sinkIdx);
SourceVChan_type*			GetSourceVChan						(SinkVChan_type* sinkVChan);
size_t						GetNSinkVChans						(SourceVChan_type* srcVChan);
ListType					GetSinkVChanList					(SourceVChan_type* srcVChan);

	// Sink VChan thread safe queue handle
CmtTSQHandle				GetSinkVChanTSQHndl					(SinkVChan_type* sinkVChan);

	// Maximum number of datapackets a Sink VChan can hold
void						SetSinkVChanTSQSize					(SinkVChan_type* sinkVChan, size_t nItems);
size_t						GetSinkVChanTSQSize					(SinkVChan_type* sinkVChan);

	// Time in [ms] to keep on trying to read a data packet from a Sink VChan TSQ		
void						SetSinkVChanReadTimeout				(SinkVChan_type* sinkVChan, double time);	
double						GetSinkVChanReadTimeout				(SinkVChan_type* sinkVChan);

//------------------------------------------------------------------------------
// Data Packet Management
//------------------------------------------------------------------------------

	// Releases all data packets from a Sink VChan.
int							ReleaseAllDataPackets				(SinkVChan_type* sinkVChan, char** errorMsg);

	// Sends a data packet from an open Source VChan to its active (and open) Sink VChans. If the Source VChan also needs to use the data packet after it was sent
	// then set sourceNeedsPacket = TRUE.
int				 			SendDataPacket 						(SourceVChan_type* srcVChan, DataPacket_type** dataPacketPtr, BOOL sourceNeedsPacket, char** errorMsg);

	// Sends a NULL data packet to mark the end of a transmission.
int							SendNullPacket						(SourceVChan_type* srcVChan, char** errorMsg);

	// Gets all data packets from a Sink VChan. The function allocates dynamically a data packet array with nPackets elements of DataPacket_type*
	// If there are no data packets in the Sink VChan, dataPackets = NULL, nPackets = 0 and the function returns NULL (success).
	// If an error is encountered, the function returns an FCallReturn_type* with error information and sets dataPackets to NULL and nPackets to 0.
	// For datapackets pass the address of a DataPacket_type** variable.
int							GetAllDataPackets					(SinkVChan_type* sinkVChan, DataPacket_type*** dataPackets, size_t* nPackets, char** errorMsg);

	// Attempts to get one data packet from a Sink VChan before the timeout period. 
int				 			GetDataPacket 						(SinkVChan_type* sinkVChan, DataPacket_type** dataPacketPtr, char** errorMsg);

//------------------------------------------------------------------------------ 
// Waveform data management
//------------------------------------------------------------------------------ 

	// Receives waveform data from a Sink VChan which is assembled from multiple data packets until a NULL packets is encountered. The function returns the 
	// dynamically allocated waveform in case there is waveform data or NULL if the first data packet read from the VChan is NULL. On success, the function 
	// return 0 and negative number plus an error message if it fails.
int							ReceiveWaveform						(SinkVChan_type* sinkVChan, Waveform_type** waveform, WaveformTypes* waveformType, char** errorMsg);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __VChannel_H__ */
