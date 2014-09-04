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
		
//==============================================================================
// Constants

//==============================================================================
// Types
		
typedef struct VChan		VChan_type;
		
typedef struct SinkVChan 	SinkVChan_type;

typedef struct SourceVChan 	SourceVChan_type;

typedef struct DataPacket	DataPacket_type;

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

	// Callback to discard data contained in a data packet
typedef void	(* DiscardDataFptr_type)		(void** dataPtrPtr);

//------------------------------------------------------------------------------
// VChan data types
//------------------------------------------------------------------------------
// Waveforms
typedef enum {
	Waveform_char,								// 8 bits	   			 			char
	Waveform_uchar,								// 8 bits			 				unsigned char
	Waveform_short,								// 16 bits							short
	Waveform_ushort,							// 16 bits							unsigned short
	Waveform_int,								// 32 bits							int
	Waveform_uint,								// 32 bits							unsigned int
	Waveform_ssize,								// 32 bits or 64 bits 				ssize_t
	Waveform_size,								// 32 bits or 64 bits				size_t			   			
	Waveform_float,								// 16 bits							float
	Waveform_double,							// 32 bits							double
} WaveformEnum_type;

// Images
typedef enum {
	Image_NIVIsion								// NI Vision image 32 or 64 bits, pointer, sizeof(void*).			
} ImageEnum_type;

// Waveform type
typedef struct {
	WaveformEnum_type	waveformType;			// Waveform data type.
    size_t 				n;						// Number of elements in the waveform.
    void* 				data;					// Waveform data of WaveformEnum_type. 
	double				rate;					// Sampling rate in [Hz]. If 0, sampling rate is not given.
	double				repeat;					// Number of times the waveform is repeated. If 0, waveform is repeat indefinitely.
} Waveform_type;

typedef struct {
	ImageEnum_type		imageType;				// Image data type.
	void*				data;					// Points to structure containing image data.
} Image_type;

// VChan data type
typedef enum {
	VChan_Waveform,								// Data packet contains Waveform_type data.
	VChan_Image									// Data packet contains Image_type data. 
} VChanData_type;


struct DataPacket {
	VChanData_type 				dataType; 				// Data type contained in the data packet.
	void*         				data;     				// Pointer to data of dataType
	CmtTSVHandle   				ctr;      				// Although there are multiple sinks that can receive a data packet, 
														// there is only one copy of the data in the memory. 
														// To de-allocate memory for the data, each sink must call ReleaseDataPacket 
														// which in the end frees the memory if ctr being a thread safe variable reaches 0 
	DiscardDataFptr_type 		discardFptr;
};

//==============================================================================
// Global functions

//------------------------------------------------------------------------------
// VChan Creation / Destruction and management functions
//------------------------------------------------------------------------------

SourceVChan_type*		init_SourceVChan_type		(char 						name[], 
										  	  	 	VChanData_type 				dataType,
										 	  	 	void* 						vChanOwner,
												 	Connected_CBFptr_type		Connected_CBFptr,
												 	Disconnected_CBFptr_type	Disconnected_CBFptr);


SinkVChan_type*			init_SinkVChan_type			(char 						name[], 
										  	  	 	VChanData_type 				dataType,
										 	  	 	void* 						vChanOwner,
												 	Connected_CBFptr_type		Connected_CBFptr,
												 	Disconnected_CBFptr_type	Disconnected_CBFptr);

	// Discard common to both types of VChan. Cast SourceVChan_type** and SinkVChan_type** to VChan_type**.
void 					discard_VChan_type 			(VChan_type** VChan); 

	// Checks if a VChan object exists among a list of VChans of VChan_type*.
	// If found and *idx is provided, it returns the 1-based VChan index in the list, otherwise *idx=0.
BOOL					VChanExists					(ListType VChanList, VChan_type* VChan, size_t* idx);

	// Searches for a given VChan name among a list of VChans of VChan_type*.
	// If found and *idx is provided, it returns the 1-based VChan index in the list, otherwise *idx=0.
VChan_type*				VChanNameExists				(ListType VChanList, char VChanName[], size_t* idx);

	// Returns a unique VChan name among a given list of VChans. The unique VChan name has a base name and a number.
	// VChanList of VChan_type*
char*					GetUniqueVChanName			(ListType VChanList, char baseVChanName[]);

//------------------------------------------------------------------------------
// VChan Connections
//------------------------------------------------------------------------------

	// Connects a Sink and a Source VChan .
BOOL					VChan_Connect				(SourceVChan_type* source, SinkVChan_type* sink);

	// Disconnects a VChan. 
BOOL					VChan_Disconnect			(VChan_type* vchan); 

//------------------------------------------------------------------------------
// VChan Set/Get
//------------------------------------------------------------------------------

	// Assigns new VChan name
void 					SetVChanName 				(VChan_type* vchan, char newName[]);

	// Returns pointer to dynamically allocated VChan name (null terminated string) 
char*					GetVChanName				(VChan_type* vchan);

	// Returns the thread safe queue handle of a sink VChan
CmtTSQHandle			GetSinkVChanTSQHndl			(SinkVChan_type* sink);

	// The maximum number of datapackets a Sink VChan can hold
void					SetSinkVChanTSQSize			(SinkVChan_type* sink, size_t nItems);

size_t					GetSinkVChanTSQSize			(SinkVChan_type* sink);

	// Time in [ms] to keep on trying to write a data packet to a Sink VChan TSQ		
void					SetSinkVChanWriteTimeout	(SinkVChan_type* sink, double time);	
double					GetSinkVChanWriteTimeout	(SinkVChan_type* sink);

void*					GetPtrToVChanOwner			(VChan_type* vchan);


//------------------------------------------------------------------------------
// Data Packet Management
//------------------------------------------------------------------------------

	// Initializes a data packet
int 					init_DataPacket_type		(DataPacket_type* 			dataPacket, 
												 	 VChanData_type 			dataType, 
												  	 void* 						data,
												 	 DiscardDataFptr_type 		discardFptr);

	// Discards the data from a data packet when it is not needed anymore
void 					ReleaseDataPacket			(DataPacket_type* a);

	// Sends a data packet from a Source VChan to its Sink VChans
FCallReturn_type* 		SendDataPacket 				(SourceVChan_type* source, DataPacket_type* dataPacket); 

//------------------------------------------------------------------------------
// VChan Data Types Management
//------------------------------------------------------------------------------
	// Waveforms
	// Note: the provided waveform must be allocated with malloc.
Waveform_type*			init_Waveform_type			(WaveformEnum_type waveformType, size_t n, void* waveform, double rate, double repeat);

	// Discards all waveform types
void					discard_Waveform_type		(void** waveform); // provide this to the discardFptr parameter from init_DataPacket_type

	// Images
Image_type*				init_Image_type				(ImageEnum_type imageType, void* image);

	// Discards all image types.
void					discard_Image_type			(void** image);


#ifdef __cplusplus
    }
#endif

#endif  /* ndef __VChannel_H__ */
