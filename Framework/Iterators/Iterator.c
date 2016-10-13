//==============================================================================
//
// Title:		Iterator.c
// Purpose:		Provides data indexing.
//
// Created on:	10/8/2014 at 3:07:21 PM by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
//
//==============================================================================

//==============================================================================
// Include files

#include <formatio.h>
#include "Iterator.h"
#include "toolbox.h"
#include "DAQLabErrHandling.h"

//==============================================================================
// Constants

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;} 
#define MAXGROUPNAME			MAX_PATHNAME_LEN
#define MAXDATARANK				255	

//==============================================================================
// Types

struct Iterator {
	Iterator_type*			parent;					// Pointer to parent iterator, otherwise  NULL if root iterator.
	char*					name;					// Iterator name.  
	IterTypes				iterType;				// Iteratable objects type.
	size_t					currentIterIdx; 		// 0-based iteration index used to iterate over iterObjects
	size_t					totalIter;				// Total number of iterations
	DiscardDataFptr_type 	discardDataFptr;		// For data type iterators only, function callback to discard data from an iterator.
	ListType				iterObjects;			// List of iteratable data objects such as Waveform_type* or Iterator_type* elements specified by iterType.
	BOOL					stackdata;				// Combine generated data into a one-dimension higher stack of datasets  
													// TEMPORARY, must be removed! It is the resposibility of the root iterator to stack the data or not.
													
};

// Task controller data packet indexing and used by the the datastorage module
struct DSInfo{
	char*					groupName;  			// Create an HDF5 group for the dataset.
	BOOL					stackeddata;			// True if data is stacked, False otherwise.
	unsigned int 			datasetrank;			// Dataset rank, i.e. # dimensions 
	unsigned int 			datarank;				// Data element rank (1 for waveform, 2 for Images), i.e. # dimensions
	unsigned int*			iterIndices;   			// Dataset index array.
};

   
//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static DSInfo_type* 			init_DSInfo_type						(void);       

//==============================================================================
// Global variables

//==============================================================================
// Global functions




Iterator_type* init_Iterator_type (char name[])
{
	Iterator_type* iterator = malloc (sizeof(Iterator_type));
	if (!iterator) return NULL;
	
	iterator->name 					= StrDup(name);
	iterator->currentIterIdx		= 0;
	iterator->totalIter				= 0;
	iterator->iterType				= Iterator_None;
	iterator->iterObjects   		= 0;
	iterator->discardDataFptr		= NULL;
	iterator->parent				= NULL;
	iterator->stackdata				= FALSE;
	
	if ( !(iterator->iterObjects   	= ListCreate(sizeof(void*))) )	goto Error;
	
	return iterator;
	
Error:
	if (iterator->iterObjects) ListDispose(iterator->iterObjects);
	free(iterator);
	return NULL;
}

void discard_Iterator_type (Iterator_type** iteratorPtr)
{
	Iterator_type* 		iterator		= *iteratorPtr;
	size_t 				nObjects		= 0;
	Iterator_type**		iterObjectPtr	= NULL;

	if (!iterator) return;
	if (!iterator->name) return;
	
	OKfree(iterator->name);
	
	nObjects 		= ListNumItems(iterator->iterObjects);   
	for (size_t i = 1; i <= nObjects; i++) {
		iterObjectPtr = ListGetPtrToItem(iterator->iterObjects, i);
		if (iterator->iterType == Iterator_Iterator)
			discard_Iterator_type(iterObjectPtr);   							// discard recursively if object is another iterator
		else
			(*(*iterObjectPtr)->discardDataFptr)	((void**)iterObjectPtr);	// discard data if data type iterator
	}   
		
	ListDispose(iterator->iterObjects); 
	
	OKfree(*iteratorPtr);
}   


void RemoveFromParentIterator(Iterator_type* iterator)
{
	size_t 				nObjects 		= ListNumItems(iterator->iterObjects);   
	Iterator_type**		iterObjectPtr	= NULL; 
	
	// remove from parent iterator if any
	if (iterator->parent) {
		nObjects = ListNumItems(iterator->parent->iterObjects);
		for (size_t i = 1; i <= nObjects; i++) {
			iterObjectPtr = ListGetPtrToItem(iterator->parent->iterObjects, i);
			if (*iterObjectPtr == iterator) {
				ListRemoveItem(iterator->parent->iterObjects, 0, i);
				iterator->parent = NULL;
				break;
			}
			
		}
	}	
}

IterTypes GetIteratorType (Iterator_type* iterator)
{
	return iterator->iterType;
}

Iterator_type* GetIteratorParent (Iterator_type* iterator)
{
	return iterator->parent;
}

void SetIteratorType (Iterator_type* iterator, IterTypes itertype)
{
	iterator->iterType = itertype;
}

size_t GetIteratorSize (Iterator_type* iterator)
{
	return ListNumItems(iterator->iterObjects);
}

void SetTotalIterations (Iterator_type* iterator, size_t nIterations)
{
	iterator->totalIter = nIterations;
}

size_t GetTotalIterations (Iterator_type* iterator)
{
	return iterator->totalIter;
}

size_t GetCurrentIterIndex(Iterator_type* iterator)
{
	return iterator->currentIterIdx;	
}

void SetIteratorStackData (Iterator_type* iterator, BOOL stackdata)
{
	iterator->stackdata = stackdata;
}

BOOL GetIteratorStackData(Iterator_type* iterator)
{
	return iterator->stackdata;	
}

char* GetCurrentIterName(Iterator_type* iterator)
{
	return StrDup(iterator->name);	
}

void SetCurrentIterName(Iterator_type* iterator, char name[])
{
	OKfree(iterator->name);
	iterator->name = StrDup(name);
}

void  SetCurrentIterIndex	(Iterator_type* iterator, size_t index)
{
	iterator->currentIterIdx = index;
}

ListType GetCurrentIteratorSet	(Iterator_type* iterator)
{
	Iterator_type**		iterObjectPtr	= &iterator;
	ListType			iteratorSet		= ListCreate(sizeof(Iterator_type*));
	
	ListInsertItem(iteratorSet, &iterator, END_OF_LIST); 
	
	while ((*iterObjectPtr)->iterType == Iterator_Iterator) {
		// get next iterator
		iterObjectPtr = ListGetPtrToItem((*iterObjectPtr)->iterObjects, (*iterObjectPtr)->currentIterIdx+1); 
		ListInsertItem(iteratorSet, iterObjectPtr, END_OF_LIST);
	}
	
	return iteratorSet;
}

Iterator_type* GetRootIterator (Iterator_type* iterator)
{
	Iterator_type* rootIterator		= iterator;
	
	while (rootIterator->parent!=NULL)
		rootIterator = rootIterator->parent;
	
	return rootIterator; 
}

BOOL IsRootIterator	(Iterator_type* iterator)
{
	if (iterator->parent) 
		return TRUE;
	else
		return FALSE;
}

int	IteratorAddWaveform (Iterator_type* iterator, Waveform_type* waveform)
{
#define IteratorAddWaveform_Err_WrongIteratorType		-1
	
	// Adding a waveform to an iterator determines its type. Once an iterator is of waveform type
	// only waveform can be added to it.
	
	if (iterator->iterType == Iterator_None) {
		iterator->iterType 			= Iterator_Waveform;
		iterator->discardDataFptr 	= (DiscardDataFptr_type) discard_Waveform_type; 
	}
	else
		if (iterator->iterType != Iterator_Waveform) return IteratorAddWaveform_Err_WrongIteratorType;
	
	ListInsertItem(iterator->iterObjects, &waveform, END_OF_LIST);
	
	return 0; // success
}


int	IteratorAddIterator	(Iterator_type* iterator, Iterator_type* iteratorToAdd, char** errorMsg)
{
#define IteratorAddIterator_Err_WrongIteratorType					-1
#define IteratorAddIterator_Err_ChildIteratorAlreadyAssigned		-2 	// iterator to be added as a child has been already added to another iterator
	
INIT_ERR
	
	// check parent iterator type
	if (iterator->iterType == Iterator_None)  
		iterator->iterType = Iterator_Iterator;
	else
		if (iterator->iterType != Iterator_Iterator)
			SET_ERR(IteratorAddIterator_Err_WrongIteratorType, "Wrong iterator type.");
	
	// check if iteratorToAdd has already a parent iterator
	if (iteratorToAdd->parent)
		SET_ERR(IteratorAddIterator_Err_ChildIteratorAlreadyAssigned, "Child iterator is already assigned.");
	
	// add child iterator to parent iterator
	ListInsertItem(iterator->iterObjects, &iteratorToAdd, END_OF_LIST);
	
	// add parent iterator to child iterator
	iteratorToAdd->parent = iterator;
	
Error:
	
RETURN_ERR	
}

void ResetIterators (Iterator_type* iterator)
{
	size_t 				nObjects 		= ListNumItems(iterator->iterObjects);
	Iterator_type**		iterObjectPtr;
	
	// reset iteration indices recursively for iterators of iterators	
	if (iterator->iterType == Iterator_Iterator) 
		for (size_t i = 1; i <= nObjects; i++) {
			iterObjectPtr = ListGetPtrToItem(iterator->iterObjects, i);
			ResetIterators(*iterObjectPtr);   				
		}
	
	// reset current iteration index of this iterator (regardless of iterator type)
	iterator->currentIterIdx = 0;
}

ListType IterateOverIterators (Iterator_type* iterator)
{
	Iterator_type**		iteratorPtr			= &iterator;    // start with given iterator
	
	// make sure it's not an empty iterator
	if (iterator->iterType == Iterator_None) return 0;
	
	// get to the leaf node
	while ((*iteratorPtr)->iterType == Iterator_Iterator)
		iteratorPtr = ListGetPtrToItem((*iteratorPtr)->iterObjects, (*iteratorPtr)->currentIterIdx+1); 
	
	// go one level higher if selected iterator type is none
	if ((*iteratorPtr)->iterType == Iterator_None)
		iteratorPtr = &(*iteratorPtr)->parent;
	
	// try to find an iterator that can be still incremented
	while ((*iteratorPtr)->currentIterIdx == ListNumItems((*iteratorPtr)->iterObjects) - 1 && *iteratorPtr != iterator)
			iteratorPtr = &(*iteratorPtr)->parent; 
		
	// check if there are iterations left in the iterator set
	if ((*iteratorPtr == iterator) && ((*iteratorPtr)->currentIterIdx == ListNumItems((*iteratorPtr)->iterObjects) - 1))
		return 0;
	else {
		(*iteratorPtr)->currentIterIdx++;
		return GetCurrentIteratorSet(iterator);
	}
	
}

// get DataStorage data from the iterator
DSInfo_type* GetIteratorDSData (Iterator_type* iterator, unsigned int datarank)
{
INIT_ERR

	int 				rank						= 0;
	DSInfo_type* 		ds_data						= NULL;  
	Iterator_type* 		parentIterator				= NULL;
	Iterator_type* 		childIterator				= NULL; 
	char*				tcName						= NULL;
	char*				name						= NULL;
	char* 				fullGroupName				= NULL;
	size_t				iterIdx						= 0;
	BOOL				determining_rank			= TRUE;
	unsigned int		indices[MAXDATARANK]		= {0};   
	size_t				totalIter					= 0;
	BOOL				stackData					= FALSE;
	
	if (!iterator) return NULL;
	
	nullChk( ds_data = init_DSInfo_type() );

	parentIterator = GetIteratorParent(iterator);
	// no parent
	//if (!parentIterator)  
	//	return NULL;
	
	// TEST: if the function returns a NULL it signals that it is out of memory, so using this instead
	if (!parentIterator)
		return ds_data;
	
	iterIdx 	= GetCurrentIterIndex(iterator);  
	totalIter 	= GetTotalIterations(iterator);
	
	// send indices of sending tc as well
	indices[rank] = iterIdx;	
	rank++;  
	
	tcName			= GetCurrentIterName(parentIterator);
	iterIdx			= GetCurrentIterIndex(parentIterator); 
	stackData		= GetIteratorStackData(parentIterator);
	nullChk( fullGroupName = malloc(MAXGROUPNAME * sizeof(char)) );
	
	if (!stackData) 
		Fmt(fullGroupName, "%s<%s[w3]#%i", tcName, iterIdx); 
	else 
		Fmt(fullGroupName, "%s<%s[w3]", tcName); 
	
	OKfree(tcName);
	
	while (parentIterator) {
		
		if (determining_rank && stackData) {
			indices[rank] = GetCurrentIterIndex(parentIterator);
			rank++;
		} else
			determining_rank = FALSE;	
		
		childIterator 	= parentIterator;
		parentIterator	= GetIteratorParent(childIterator);
		
		if (!parentIterator)
			break;  //no more parent
		
		tcName			= GetCurrentIterName(parentIterator);
		iterIdx			= GetCurrentIterIndex(parentIterator);
		stackData		= GetIteratorStackData(parentIterator); 
		nullChk( name 	= malloc(MAXGROUPNAME * sizeof(char)) );
		
		if (!stackData) 
			Fmt(name, "%s<%s[w3]#%i",tcName,iterIdx); 
		else 
			Fmt(name, "%s<%s[w3]",tcName);
		
		AddStringPrefix(&fullGroupName,"/",-1);    
		AddStringPrefix(&fullGroupName,name,-1);
		OKfree(tcName);
		OKfree(name);
	}
	
	if (rank){
		OKfree(ds_data->iterIndices);
		nullChk( ds_data->iterIndices = malloc(rank * sizeof(unsigned int)) );
		//rearrange rank; Most significant rank first
		for (int i = 0; i < rank; i++) 
			ds_data->iterIndices[i] = indices[rank-i-1];
	} else 
		ds_data->iterIndices = NULL;
	
	ds_data->groupName		= StrDup(fullGroupName);
	ds_data->datasetrank	= rank;
	ds_data->stackeddata	= iterator->stackdata; 
	ds_data->datarank		= datarank;
	
	OKfree(fullGroupName);  
	
	return ds_data;
	
Error:
	
	OKfree(ds_data);
	return NULL;
}


static DSInfo_type* init_DSInfo_type(void)
{
	DSInfo_type* ds_data		= malloc(sizeof(DSInfo_type));
	if (!ds_data) return NULL;
	
	ds_data->groupName			= NULL;
	ds_data->stackeddata		= FALSE;
	ds_data->datasetrank		= 0;
	ds_data->datarank			= 0;
	ds_data->iterIndices		= NULL;
	
	return ds_data;
}

void discard_DSInfo_type (DSInfo_type** dsInfoPtr)
{ 
	DSInfo_type*	dsInfo = *dsInfoPtr;
	if (!dsInfo) return;
	
	OKfree(dsInfo->groupName);
	OKfree(dsInfo->iterIndices); 
	
	OKfree(*dsInfoPtr);
}   

void SetDSInfoGroupName	(DSInfo_type* dsInfo, char groupName[])
{
	OKfree(dsInfo->groupName);
	dsInfo->groupName = StrDup(groupName);
}

char* GetDSInfoGroupName (DSInfo_type* dsInfo)
{
	return StrDup(dsInfo->groupName);
}

void SetDSInfoIterIndices (DSInfo_type* dsInfo, unsigned int** iterIndicesPtr)
{
	dsInfo->iterIndices = *iterIndicesPtr;
	*iterIndicesPtr = NULL; // consume data
}

unsigned int* GetDSInfoIterIndices (DSInfo_type* dsInfo)
{
	return dsInfo->iterIndices;
}

void SetDSInfoDatasetRank	(DSInfo_type* dsInfo,unsigned int rank)
{
	dsInfo->datasetrank = rank;
}

unsigned int GetDSInfoDatasetRank (DSInfo_type* dsInfo)
{
	return dsInfo->datasetrank;
}

void SetDSInfoStackData	(DSInfo_type* dsInfo,BOOL stackdata)
{
	dsInfo->stackeddata = stackdata;
}

BOOL GetDSInfoStackData (DSInfo_type* dsInfo)
{
	return dsInfo->stackeddata;
}

unsigned int GetDSDataRank (DSInfo_type* dsInfo)
{
	return dsInfo->datarank;
}



