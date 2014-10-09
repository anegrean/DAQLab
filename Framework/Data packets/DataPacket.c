//==============================================================================
//
// Title:		DataPacket.c
// Purpose:		A short description of the implementation.
//
// Created on:	10/8/2014 at 3:14:26 PM by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DAQLabUtility.h" 
#include "DataPacket.h"
#include "DataTypes.h"
#include "toolbox.h"
#include "nivision.h" 

//==============================================================================
// Constants

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;} 

//==============================================================================
// Types

// Discard Data Packet function pointer type
typedef void	(* DiscardDataPacketFptr_type)		(DataPacket_type** dataPacket);

//---------------------------------------------------------------------------------------------------
// Base Data Packet
//---------------------------------------------------------------------------------------------------
struct DataPacket {
	PacketTypes 				dataType; 		// Data type contained in the data packet.
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
	WaveformTypes				waveformType;	// Waveform data type
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
// Static global variables

//==============================================================================
// Static functions

static void 				init_DataPacket_type				(DataPacket_type* dataPacket, PacketTypes dataType, size_t n, void* data, DiscardDataPacketFptr_type discardFptr);

static void 				discard_WaveformPacket_type 		(DataPacket_type** waveformPacket);

static void					discard_NIVisionImagePacket_type	(DataPacket_type** imagePacket);



//==============================================================================
// Global variables

//==============================================================================
// Global functions




// implementation

static void init_DataPacket_type (DataPacket_type* dataPacket, PacketTypes dataType, size_t n, void* data, DiscardDataPacketFptr_type discardFptr)
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

void SetDataPacketCounter (DataPacket_type* dataPacket, size_t count)
{
	int* ctrTSVptr;
	CmtGetTSVPtr(dataPacket->ctr, &ctrTSVptr);
	*ctrTSVptr = count;
	CmtReleaseTSVPtr(dataPacket->ctr);
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

void* GetDataPacketDataPtr (DataPacket_type* dataPacket, size_t* n)  
{
	if (n) *n = dataPacket->n;
	
	return dataPacket->data;
}

DataPacket_type* init_WaveformPacket_type (WaveformTypes waveformType, size_t n, void* waveform, double rate, double nRepeat)
{
	WaveformPacket_type* 	packet = malloc (sizeof(WaveformPacket_type));
	if (!packet) return NULL;
	
	// map waveform data type to packet data type
	PacketTypes		packetType;
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
