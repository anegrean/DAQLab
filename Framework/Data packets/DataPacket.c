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

#include "DataPacket.h"
#include "DataTypes.h"
#include "toolbox.h"

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
	void*         				data;     				// Pointer to data array of dataType elements.
	CmtTSVHandle   				ctr;      				// Data Packet in-use counter. Although there are multiple sinks that can receive a data packet, 
														// there is only one copy of the data in the memory. To de-allocate memory for the data, each sink must 
														// call ReleaseDataPacket which in the end frees the memory if ctr reaches 0. 
	DiscardPacketDataFptr_type 	discardPacketDataFptr;	// Function pointer which will be called to discard the data pointer when ctr reaches 0.
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables

//==============================================================================
// Global functions

DataPacket_type* init_DataPacket_type (DLDataTypes dataType, void* data, DiscardPacketDataFptr_type discardPacketDataFptr) 
{
	DataPacket_type* dataPacket = malloc (sizeof(DataPacket_type));
	if (!dataPacket) return NULL;
	
	dataPacket -> dataType 					= dataType;
	dataPacket -> data     					= data;
	dataPacket -> discardPacketDataFptr   	= discardPacketDataFptr;
	
	// create counter to keep track of how many sinks still need this data packet
	CmtNewTSV(sizeof(int), &dataPacket->ctr);
	// set counter to 1
	int* 	ctrTSVptr;
	
	CmtGetTSVPtr(dataPacket->ctr, &ctrTSVptr);
	*ctrTSVptr = 1;
	CmtReleaseTSVPtr(dataPacket->ctr);
	
	return dataPacket;
}

void discard_DataPacket_type (DataPacket_type** dataPacket)
{
	if (!*dataPacket) return;
	
	// discard data
	if ((*dataPacket)->discardPacketDataFptr)
		(*(*dataPacket)->discardPacketDataFptr) (&(*dataPacket)->data);
	else
		OKfree((*dataPacket)->data);
	
	// discard instance counter
	CmtDiscardTSV((*dataPacket)->ctr);
	
	// discard data packet
	OKfree(*dataPacket);
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

DLDataTypes	GetDataPacketDataType (DataPacket_type* dataPacket)
{
	return dataPacket->dataType;
}

void* GetDataPacketDataPtr (DataPacket_type* dataPacket, DLDataTypes* dataType)  
{
	*dataType = dataPacket->dataType;
	return dataPacket->data;
}
