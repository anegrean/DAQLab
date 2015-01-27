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
	Iterator_type*				currentiter;            // data belongs to this iteration 
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

DataPacket_type* init_DataPacket_type (DLDataTypes dataType, void** ptrToData, Iterator_type* currentiter,DiscardPacketDataFptr_type discardPacketDataFptr) 
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
	
	dataPacket -> dataType 					= dataType;
	dataPacket -> data     					= *ptrToData;
	*ptrToData								= NULL;
	dataPacket -> discardPacketDataFptr   	= discardPacketDataFptr;
	
	dataPacket -> currentiter				= currentiter;
	
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
									  
	discard_Iterator_type(&((*dataPacket)->currentiter),FALSE,TRUE);
	
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

void** GetDataPacketPtrToData (DataPacket_type* dataPacket, DLDataTypes* dataType)  
{
	if (dataType)
		*dataType = dataPacket->dataType;
	
	return &dataPacket->data;
}

Iterator_type* GetDataPacketCurrentIter (DataPacket_type* dataPacket)  
{
	return dataPacket->currentiter;
}

				  
