//==============================================================================
//
// Title:		DataPacket.c
// Purpose:		Passes data through virtual channels.
//
// Created on:	10/8/2014 at 3:14:26 PM by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
//
//==============================================================================

//==============================================================================
// Include files

#include "DataPacket.h"
#include "DataTypes.h"
#include "toolbox.h"
#include "Iterator.h"

//==============================================================================
// Constants

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;} 

//==============================================================================
// Types

//---------------------------------------------------------------------------------------------------
// Data Packet
//---------------------------------------------------------------------------------------------------
struct DataPacket {
	DLDataTypes 				dataType; 				// Data type contained in the data packet.
	void*         				data;     				// Pointer to data of dataType elements.
	CmtTSVHandle   				ctr;      				// Data Packet in-use counter. Although there are multiple sinks that can receive a data packet, 
														// there is only one copy of the data in the memory. To de-allocate memory for the data, each sink must 
														// call ReleaseDataPacket which in the end frees the memory if ctr reaches 0. 
	DSInfo_type*		    	dsInfo;                 // data storage data belongs to this iteration 
	DiscardFptr_type 			discardPacketDataFptr;	// Function pointer which will be called to discard the data pointer when ctr reaches 0.
};




  

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables

//==============================================================================
// Global functions

DataPacket_type* init_DataPacket_type (DLDataTypes dataType, void** ptrToData, DSInfo_type** dsDataPtr, DiscardFptr_type discardPacketDataFptr) 
{
	DataPacket_type* dataPacket = malloc (sizeof(DataPacket_type));
	if (!dataPacket) return NULL;
	
	// create counter to keep track of how many sinks still need this data packet
	CmtNewTSV(sizeof(int), &dataPacket->ctr);
	// set counter to 1
	int* 	ctrTSVptr = NULL;
	
	CmtGetTSVPtr(dataPacket->ctr, &ctrTSVptr);
	*ctrTSVptr = 1;
	CmtReleaseTSVPtr(dataPacket->ctr);
	
	dataPacket->dataType 					= dataType;
	dataPacket->data     					= *ptrToData;
	*ptrToData								= NULL;			// consume data
	dataPacket->discardPacketDataFptr   	= discardPacketDataFptr;
	
	// indexing info
	if (dsDataPtr) {
		dataPacket->dsInfo     				= *dsDataPtr;
		*dsDataPtr							= NULL; 		// consume object
	} else
		dataPacket->dsInfo					= NULL;
		
	
	return dataPacket;
}

void discard_DataPacket_type (DataPacket_type** dataPacketPtr)
{
	DataPacket_type*	dataPacket = *dataPacketPtr;
	if (!dataPacket) return;
	
	// discard data
	if (dataPacket->discardPacketDataFptr)
		(*dataPacket->discardPacketDataFptr) (&dataPacket->data);
	else
		OKfree(dataPacket->data);
	
	// discard instance counter
	CmtDiscardTSV(dataPacket->ctr);
	
	// discard indexing
	discard_DSInfo_type(&dataPacket->dsInfo);
	
	// discard data packet
	OKfree(*dataPacketPtr);
}

void SetDataPacketCounter (DataPacket_type* dataPacket, size_t count)
{
	int* ctrTSVptr;
	CmtGetTSVPtr(dataPacket->ctr, &ctrTSVptr);
	*ctrTSVptr = count;
	CmtReleaseTSVPtr(dataPacket->ctr);
}

void ReleaseDataPacket (DataPacket_type** dataPacketPtr)
{
	DataPacket_type*	dataPacket = *dataPacketPtr;
	int* ctrTSVptr;
	
	if (!dataPacket) return;
	
	CmtGetTSVPtr(dataPacket->ctr, &ctrTSVptr);
	if (*ctrTSVptr > 1) {
		(*ctrTSVptr)--;
		CmtReleaseTSVPtr(dataPacket->ctr);
	} else {
		CmtReleaseTSVPtr(dataPacket->ctr); 
		discard_DataPacket_type(dataPacketPtr);
	} 
}

DLDataTypes	GetDataPacketDataType (DataPacket_type* dataPacket)
{
	return dataPacket->dataType;
}

void** GetDataPacketPtrToData (DataPacket_type* dataPacket, DLDataTypes* dataType)  
{
	if (dataType)
		*dataType = dataPacket->dataType;
	
	return &dataPacket->data;
}

DSInfo_type* GetDataPacketDSData (DataPacket_type* dataPacket)  
{
	return dataPacket->dsInfo;
}
				  
