//==============================================================================
//
// Title:		Iterator.c
// Purpose:		A short description of the implementation.
//
// Created on:	10/8/2014 at 3:07:21 PM by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "Iterator.h"
#include "toolbox.h"

//==============================================================================
// Constants

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}   

//==============================================================================
// Types

struct Iterator {
	char*					name;					// Iterator name.  
	IterTypes				iterType;				// Iteratable objects type.
	size_t					currentIterIdx;			// 0-based iteration index used to iterate over iterObjects
	size_t					totalIter;				// Total number of iterations
	DiscardDataFptr_type 	discardDataFptr;		// For data type iterators only, function callback to discard data from an iterator.
	ListType				iterObjects;			// List of iteratable data objects such as Waveform_type* or Iterator_type* elements specified by iterType.
	Iterator_type*			parent;					// Pointer to parent iterator, otherwise  NULL if root iterator.
};

   
//==============================================================================
// Static global variables

//==============================================================================
// Static functions

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
	
	if ( !(iterator->iterObjects   	= ListCreate(sizeof(void*))) )	goto Error;
	
	return iterator;
	
Error:
	if (iterator->iterObjects) ListDispose(iterator->iterObjects);
	free(iterator);
	return NULL;
}

void discard_Iterator_type (Iterator_type** iterator)
{
	if (!*iterator) return;
	
	OKfree((*iterator)->name);
	
	size_t 				nObjects 		= ListNumItems((*iterator)->iterObjects);
	Iterator_type**		iterObjectPtr;
	
	for (size_t i = 1; i <= nObjects; i++) {
		iterObjectPtr = ListGetPtrToItem((*iterator)->iterObjects, i);
		if ((*iterator)->iterType == Iterator_Iterator)
			discard_Iterator_type(iterObjectPtr);   							// discard recursively if object is another iterator
		else
			(*(*iterObjectPtr)->discardDataFptr)	((void**)iterObjectPtr);	// discard data if data type iterator
	}
	
	ListDispose((*iterator)->iterObjects);
	
	// remove from parent iterator if any
	if ((*iterator)->parent) {
		nObjects = ListNumItems((*iterator)->parent->iterObjects);
		for (size_t i = 1; i <= nObjects; i++) {
			iterObjectPtr = ListGetPtrToItem((*iterator)->parent->iterObjects, i);
			if (*iterObjectPtr == *iterator) {
				ListRemoveItem((*iterator)->parent->iterObjects, 0, i);
				break;
			}
			
		}
	}
	
	OKfree(*iterator);
}

IterTypes GetIteratorType (Iterator_type* iterator)
{
	return iterator->iterType;
}

size_t GetIteratorSize (Iterator_type* iterator)
{
	return ListNumItems(iterator->iterObjects);
}

void SetIteratorIterations (Iterator_type* iterator, size_t nIterations)
{
	iterator->totalIter = nIterations;
}

void GetCurrentIterationIndices	(Iterator_type* iterator, size_t currentIterIndex[])
{
	Iterator_type**		iterObjectPtr	= &iterator;
	size_t				idx				= 0;
	
	currentIterIndex[idx] = (*iterObjectPtr)->currentIterIdx; 
	
	while ((*iterObjectPtr)->iterType == Iterator_Iterator) {
		// get next iterator
		iterObjectPtr = ListGetPtrToItem((*iterObjectPtr)->iterObjects, currentIterIndex[idx]+1); 
		idx++;
		// get current iteration index of this iterator
		currentIterIndex[idx] = (*iterObjectPtr)->currentIterIdx;
	}
}

size_t GetIteratorDimensions (Iterator_type* iterator)
{
	Iterator_type**		iterObject	= &iterator;
	size_t				nDim;
		
	if (ListNumItems(iterator->iterObjects))
		nDim = 1;
	else
		nDim = 0;
	
	while ((*iterObject)->iterType == Iterator_Iterator && ListNumItems((*iterObject)->iterObjects)) {
		iterObject = ListGetPtrToItem((*iterObject)->iterObjects, 1);
		nDim++;
	}
	
	return nDim;
}

Iterator_type* GetRootIterator (Iterator_type* iterator)
{
	Iterator_type* rootIterator		= iterator;
	
	while (rootIterator->parent)
		rootIterator = iterator->parent;
	
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

int	IteratorAddIterator	(Iterator_type* iterator, Iterator_type* iteratorToAdd)
{
#define IteratorAddIterator_Err_WrongIteratorType					-1
	// iterator to be added as a child has been already added to another iterator
#define IteratorAddIterator_Err_ChildIteratorAlreadyAssigned		-2
	
	// check parent iterator type
	if (iterator->iterType == Iterator_None)  
		iterator->iterType 			= Iterator_Iterator;
	else
		if (iterator->iterType != Iterator_Iterator) return IteratorAddIterator_Err_WrongIteratorType;
	
	// check if iteratorToAdd has already a parent iterator
	if (iteratorToAdd->parent) return IteratorAddIterator_Err_ChildIteratorAlreadyAssigned;
	
	// add child iterator to parent iterator
	ListInsertItem(iterator->iterObjects, &iteratorToAdd, END_OF_LIST);
	
	// add parent iterator to child iterator
	iteratorToAdd->parent = iterator;
	
	return 0;
}

void ResetAllIterators (Iterator_type* iterator)
{
	size_t 				nObjects 		= ListNumItems(iterator->iterObjects);
	Iterator_type**		iterObjectPtr;
	
	// reset iteration indices recursively for iterators of iterators	
	if (iterator->iterType == Iterator_Iterator) 
		for (size_t i = 1; i <= nObjects; i++) {
			iterObjectPtr = ListGetPtrToItem(iterator->iterObjects, i);
			ResetAllIterators(*iterObjectPtr);   				
		}
	
	// reset current iteration index of this iterator (regardless of iterator type)
	iterator->currentIterIdx = 0;
}

BOOL AdvanceIterationIndex (Iterator_type* iterator, size_t currentIterIndex[])
{
	
	
}

