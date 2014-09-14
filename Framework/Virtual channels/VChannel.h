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
#include "nivision.h"
		
//==============================================================================
// Constants

//==============================================================================
// Types

	// VChan types
typedef struct VChan			VChan_type;					// base class
typedef struct SinkVChan 		SinkVChan_type;
typedef struct SourceVChan 		SourceVChan_type;

	// Data Packet types
typedef struct DataPacket		DataPacket_type; 			// base class
typedef struct WaveformPacket	WaveformPacket_type;
typedef struct ImagePacket		ImagePacket_type;

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
typedef void	(* Connected_CBFptr_type) 		(VChan_type* self, VChan_type* connectedVChan);

	// Callback when a VChan connection is broken by calling VChan_Disconnect. When disconnecting
	// a Source type VChan, this callback will be called for each Sink.
typedef void	(* Disconnected_CBFptr_type) 	(VChan_type* self, VChan_type* disconnectedVChan);

//------------------------------------------------------------------------------
// VChan data types
//------------------------------------------------------------------------------
typedef enum {
	
	Waveform_Char,						// 8 bits	   			 			char
	Waveform_UChar,						// 8 bits			 				unsigned char
	Waveform_Short,						// 16 bits							short
	Waveform_UShort,					// 16 bits							unsigned short
	Waveform_Int,						// 32 bits							int
	Waveform_UInt,						// 32 bits							unsigned int
	Waveform_SSize,						// 32 bits or 64 bits 				ssize_t
	Waveform_Size,						// 32 bits or 64 bits				size_t			   			
	Waveform_Float,						// 16 bits							float
	Waveform_Double,					// 32 bits							double
	
} Waveform_type;

typedef enum {
	
	WaveformPacket_Char,						
	WaveformPacket_UChar,						
	WaveformPacket_Short,						
	WaveformPacket_UShort,					
	WaveformPacket_Int,						
	WaveformPacket_UInt,						
	WaveformPacket_SSize,						
	WaveformPacket_Size,						
	WaveformPacket_Float,						
	WaveformPacket_Double,
	ImagePacket_NIVision
	
} Packet_type;
	


//==============================================================================
// Global functions

//------------------------------------------------------------------------------
// VChan Creation / Destruction and management functions
//------------------------------------------------------------------------------

SourceVChan_type*		init_SourceVChan_type				(char 						name[], 
										  	  	 	 		Packet_type 				dataType,
										 	  	 			void* 						VChanOwner,
												 	 		Connected_CBFptr_type		Connected_CBFptr,
												 	 		Disconnected_CBFptr_type	Disconnected_CBFptr);

	// Creates a Sink VChan which may support multiple data types. Provide an array of dataTypes of Packet_type elements.
SinkVChan_type*			init_SinkVChan_type					(char 						name[], 
										  	  	 	 		Packet_type					dataTypes[],
															size_t						nDataTypes,
										 	  	 	 		void* 						VChanOwner,
												 	 		Connected_CBFptr_type		Connected_CBFptr,
												 	 		Disconnected_CBFptr_type	Disconnected_CBFptr);

	// Discard common to both types of VChan. Cast SourceVChan_type** and SinkVChan_type** to VChan_type**.
void 					discard_VChan_type 					(VChan_type** VChan); 

	// Checks if a VChan object exists among a list of VChans of VChan_type*.
	// If found and *idx is provided, it returns the 1-based VChan index in the list, otherwise *idx=0.
BOOL					VChanExists							(ListType VChanList, VChan_type* VChan, size_t* idx);

	// Searches for a given VChan name among a list of VChans of VChan_type*.
	// If found and *idx is provided, it returns the 1-based VChan index in the list, otherwise *idx=0.
VChan_type*				VChanNameExists						(ListType VChanList, char VChanName[], size_t* idx);

	// Returns a unique VChan name among a given list of VChans. The unique VChan name has a base name and a number.
	// VChanList of VChan_type*
char*					GetUniqueVChanName					(ListType VChanList, char baseVChanName[]);

//------------------------------------------------------------------------------
// VChan Connections
//------------------------------------------------------------------------------

	// Checks if the data packet types supported by the Sink and Source VChan are compatible. If compatible, returns TRUE, FALSE otherwise
BOOL					CompatibleVChans					(SourceVChan_type* srcVChan, SinkVChan_type* sinkVChan);

	// Connects a Sink and a Source VChan .
BOOL					VChan_Connect						(SourceVChan_type* srcVChan, SinkVChan_type* sinkVChan);

	// Disconnects a VChan. 
BOOL					VChan_Disconnect					(VChan_type* VChan); 

	// provides UI to connect/disconnect VChans
void 					UpdateSwitchboard 					(ListType VChans, int panHandle, int tableControlID); 

//------------------------------------------------------------------------------
// VChan Set/Get
//------------------------------------------------------------------------------

	// Assigns new VChan name
void 					SetVChanName 						(VChan_type* VChan, char newName[]);

	// Returns pointer to dynamically allocated VChan name (null terminated string) 
char*					GetVChanName						(VChan_type* VChan);

char*					GetSinkVChanName					(SourceVChan_type* srcVChan, size_t sinkIdx);

SinkVChan_type*			GetSinkVChan						(SourceVChan_type* srcVChan, size_t sinkIdx);

SourceVChan_type*		GetSourceVChan						(SinkVChan_type* sinkVChan);

VChanDataFlow_type		GetVChanDataFlowType				(VChan_type* VChan);

size_t					GetNSinkVChans						(SourceVChan_type* srcVChan);

	// Returns the thread safe queue handle of a sink VChan
CmtTSQHandle			GetSinkVChanTSQHndl					(SinkVChan_type* sinkVChan);

	// The maximum number of datapackets a Sink VChan can hold
void					SetSinkVChanTSQSize					(SinkVChan_type* sinkVChan, size_t nItems);

size_t					GetSinkVChanTSQSize					(SinkVChan_type* sinkVChan);

	// Time in [ms] to keep on trying to write a data packet to a Sink VChan TSQ		
void					SetSinkVChanWriteTimeout			(SinkVChan_type* sinkVChan, double time);	
double					GetSinkVChanWriteTimeout			(SinkVChan_type* sinkVChan);

void*					GetPtrToVChanOwner					(VChan_type* VChan);

								
//------------------------------------------------------------------------------
// Data Packet Management
//------------------------------------------------------------------------------

 	// Creates a waveform data packet. Note: the provided waveform must be allocated with malloc.
DataPacket_type*		init_WaveformPacket_type			(Waveform_type waveformType, size_t n, void* waveform, double rate, double nRepeat);

double					GetWaveformPacketRepeat				(WaveformPacket_type* waveformPacket);

	// Creates an NI Vision image data packet. Note: the image array must be allocated with malloc.
DataPacket_type*		init_NIVisionImagePacket_type		(size_t n, Image* images);

	// Discards a data packet.
void					discard_DataPacket_type				(DataPacket_type** dataPacket);

	// Releases a data packet. If it is not needed anymore, it is discarded and dataPacket set to NULL.
void 					ReleaseDataPacket					(DataPacket_type** dataPacket);

	
	// Releases all data packets from a Sink VChan. 
	// If successful, returns NULL, otherwise it returns error information which must be disposed of with discard_FCallReturn_type
FCallReturn_type*		ReleaseAllDataPackets				(SinkVChan_type* sinkVChan);

	// Sends a data packet from a Source VChan to its Sink VChans.
FCallReturn_type* 		SendDataPacket 						(SourceVChan_type* source, DataPacket_type* dataPacket);

	// Gets all data packets from a Sink VChan. The function allocates dynamically a data packet array with nPackets elements of DataPacket_type*
	// If there are no data packets in the Sink VChan, dataPackets = NULL, nPackets = 0 and the function returns NULL (success).
	// If an error is encountered, the function returns an FCallReturn_type* with error information and sets dataPackets to NULL and nPackets to 0.
	// For datapackets pass the address of a DataPacket_type** variable.
FCallReturn_type*		GetAllDataPackets					(SinkVChan_type* sinkVChan, DataPacket_type*** dataPackets, size_t* nPackets);

	// Returns a pointer to the data packet buffer with n elements of Packet_type contained in the data packet.
	// If the number of elements is not needed, pass 0 for n.
void*					GetDataPacketDataPtr				(DataPacket_type* dataPacket, size_t* n);




#ifdef __cplusplus
    }
#endif

#endif  /* ndef __VChannel_H__ */
