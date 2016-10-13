//==============================================================================
//
// Title:		DataPacket.h
// Purpose:		Passes data through virtual channels.
//
// Created on:	10/8/2014 at 3:14:26 PM by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
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
#include "Iterator.h"

//==============================================================================
// Constants

//==============================================================================
// Types

	// Data Packet types
typedef struct DataPacket 		DataPacket_type;


//==============================================================================
// External variables

//==============================================================================
// Global functions

	// Adds data to a data packet. Depending on the instance counter, calling ReleaseDataPacket repeatedly, the dscardPacketDataFptr is called to discard the provided data. 
	// If data* has been allocated with malloc then for discardPacketDataFptr provide NULL. Otherwise provide the specific data type discard function.
DataPacket_type* 		init_DataPacket_type 				(DLDataTypes dataType, void** ptrToData, DSInfo_type** dsDataPtr, DiscardFptr_type discardPacketDataFptr);
	// Discards a data packet.
void					discard_DataPacket_type				(DataPacket_type** dataPacketPtr);
	// Sets number of times ReleaseDataPacket must be called before the data contained in the data packet is discarded
void					SetDataPacketCounter				(DataPacket_type* dataPacket, size_t count);
	// Releases a data packet. If it is not needed anymore, it is discarded and dataPacket set to NULL.
void 					ReleaseDataPacket					(DataPacket_type** dataPacket);
	// Returns a pointer to the data in the packet
DLDataTypes				GetDataPacketDataType				(DataPacket_type* dataPacket);
void**					GetDataPacketPtrToData				(DataPacket_type* dataPacket, DLDataTypes* dataType);
	// Returns a pointer to the data storage data
DSInfo_type* 		GetDataPacketDSData 				(DataPacket_type* dataPacket);


#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DataPacket_H__ */
