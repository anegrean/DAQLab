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

#include "DataTypes.h"

//==============================================================================
// Constants

//==============================================================================
// Types

	// Data Packet types
typedef struct DataPacket 		DataPacket_type;

	// Function pointer type for discarding data from data packets if other than free()
typedef void (*DiscardPacketDataFptr_type)	(void** data);

//==============================================================================
// External variables

//==============================================================================
// Global functions

	// Adds data to a data packet. Depending on the instance counter, calling ReleaseDataPacket repeatedly, the dscardPacketDataFptr is called to discard the provided data. 
	// If data* has been allocated with malloc then for discardPacketDataFptr provide NULL. Otherwise provide the specific data type discard function.
DataPacket_type* 		init_DataPacket_type 				(DLDataTypes dataType, void* data, DiscardPacketDataFptr_type discardPacketDataFptr);
	// Discards a data packet.
void					discard_DataPacket_type				(DataPacket_type** dataPacket);
	// Sets number of times ReleaseDataPacket must be called before the data contained in the data packet is discarded
void					SetDataPacketCounter				(DataPacket_type* dataPacket, size_t count);
	// Releases a data packet. If it is not needed anymore, it is discarded and dataPacket set to NULL.
void 					ReleaseDataPacket					(DataPacket_type** dataPacket);
	// Returns a pointer to the data in the packet
DLDataTypes				GetDataPacketDataType				(DataPacket_type* dataPacket);
void*					GetDataPacketDataPtr				(DataPacket_type* dataPacket, DLDataTypes* dataType);


#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DataPacket_H__ */
