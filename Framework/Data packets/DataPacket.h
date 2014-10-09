//==============================================================================
//
// Title:		DataPacket.h
// Purpose:		A short description of the interface.
//
// Created on:	10/8/2014 at 3:14:26 PM by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __DataPacket_H__
#define __DataPacket_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "DAQLabUtility.h"
#include "nivision.h" 
#include "DataTypes.h"

//==============================================================================
// Constants

//==============================================================================
// Types

	// Data Packet types
typedef struct DataPacket		DataPacket_type; 			// base class
typedef struct WaveformPacket	WaveformPacket_type;
typedef struct ImagePacket		ImagePacket_type;

// Data packet types
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
	
} PacketTypes;

//==============================================================================
// External variables

//==============================================================================
// Global functions

	// Creates a waveform data packet. Note: the provided waveform must be allocated with malloc.
DataPacket_type*		init_WaveformPacket_type			(WaveformTypes waveformType, size_t n, void* waveform, double rate, double nRepeat);

double					GetWaveformPacketRepeat				(WaveformPacket_type* waveformPacket);

	// Creates an NI Vision image data packet. Note: the image array must be allocated with malloc.
DataPacket_type*		init_NIVisionImagePacket_type		(size_t n, Image* images);

	// Discards a data packet.
void					discard_DataPacket_type				(DataPacket_type** dataPacket);

	// Sets number of times ReleaseDataPacket must be called before the data contained in the data packet is discarded
void					SetDataPacketCounter				(DataPacket_type* dataPacket, size_t count);

	// Releases a data packet. If it is not needed anymore, it is discarded and dataPacket set to NULL.
void 					ReleaseDataPacket					(DataPacket_type** dataPacket);

	// Returns a pointer to the data packet buffer with n elements of Packet_type contained in the data packet.
	// If the number of elements is not needed, pass 0 for n.
void*					GetDataPacketDataPtr				(DataPacket_type* dataPacket, size_t* n);


#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DataPacket_H__ */
