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
	size_t					currentIterIdx; 		// 0-based iteration index used to iterate over iterObjects
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

Iterator_type* DupIterator (Iterator_type* iterator)
{
	Iterator_type* dup = malloc (sizeof(Iterator_type));
	if (!dup) return NULL;
	if (!iterator) return NULL;
	
	dup->name 					= StrDup(iterator->name);
	dup->currentIterIdx			= iterator->currentIterIdx;
	dup->totalIter				= iterator->totalIter;
	dup->iterType				= iterator->iterType;
	dup->iterObjects   			= 0;						 //no need to keep track of child iter objects, so they are not copied
	dup->discardDataFptr		= iterator->discardDataFptr;
	dup->parent					= DupIterator(iterator->parent);
	if ( !(dup->iterObjects   	= ListCreate(sizeof(void*))) )	goto Error; //but list must be created for discarding the iterator
	
	return dup;
Error:
	if (dup->iterObjects) ListDispose(dup->iterObjects);
	free(dup);
	return NULL;
	
}


void RemoveFromParentIterator(Iterator_type* iterator)
{
	size_t 				nObjects 		= ListNumItems(iterator->iterObjects);   
	Iterator_type**		iterObjectPtr; 
	
	// remove from parent iterator if any
	if (iterator->parent) {
		nObjects = ListNumItems(iterator->parent->iterObjects);
		for (size_t i = 1; i <= nObjects; i++) {
			iterObjectPtr = ListGetPtrToItem(iterator->parent->iterObjects, i);
			if (*iterObjectPtr == iterator) {
				ListRemoveItem(iterator->parent->iterObjects, 0, i);
				break;
			}
			
		}
	}	
}





void discard_Iterator (Iterator_type** iterator)
{
	if (!*iterator) return;
	
	OKfree((*iterator)->name);
	
	size_t 				nObjects;
	Iterator_type**		iterObjectPtr;
	
	size_t 				i;
	
	
	//free entire iterator tree structure from the root
	
	nObjects 		= ListNumItems((*iterator)->iterObjects);   
	for (i = 1; i <= nObjects; i++) {
		iterObjectPtr = ListGetPtrToItem((*iterator)->iterObjects, i);
		if ((*iterator)->iterType == Iterator_Iterator)  {
			(*iterObjectPtr)->parent=NULL;     //child will be losing its parent
			discard_Iterator_type(iterObjectPtr);   							// discard recursively if object is another iterator
			
		}
		else
			(*(*iterObjectPtr)->discardDataFptr)	((void**)iterObjectPtr);	// discard data if data type iterator
	}
	
	ListDispose((*iterator)->iterObjects);
	
	discard_Iterator_type(&(*iterator)->parent);
	(*iterator)->parent=NULL;

	OKfree(*iterator);
}


void discard_Iterator_type (Iterator_type** iterator) 
{
	//free entire iterator tree structure starting from the root 
	Iterator_type* 		root;
	if (!*iterator) return;
	
	root=GetRootIterator(*iterator);      
	discard_Iterator(&root);
}


   /*
void OLD_discard_Iterator_type (Iterator_type** iterator)
{
	if (!*iterator) return;
	
	OKfree((*iterator)->name);
	
	size_t 				nObjects;
	Iterator_type**		iterObjectPtr;
	Iterator_type* 		parent;
	size_t 				i;
	
	//free entire iterator tree structure from the root
	parent=(*iterator)->parent;
	if (parent) {
		nObjects 		= ListNumItems(parent->iterObjects);  
		for (i = 1; i <= nObjects; i++) {  
			iterObjectPtr = ListGetPtrToItem(parent->iterObjects, i);
			//check if equal
			if ((*iterator)==iterObjectPtr) {
				//remove item
				ListRemoveItem(parent->iterObjects,0,i);
			}
		}
	}
	nObjects 		= ListNumItems((*iterator)->iterObjects);   
	for (i = 1; i <= nObjects; i++) {
		iterObjectPtr = ListGetPtrToItem((*iterator)->iterObjects, i);
		if ((*iterator)->iterType == Iterator_Iterator)  {
			discard_Iterator_type(iterObjectPtr);   							// discard recursively if object is another iterator
			
		}
		else
			(*(*iterObjectPtr)->discardDataFptr)	((void**)iterObjectPtr);	// discard data if data type iterator
	}
	
	ListDispose((*iterator)->iterObjects);
	
	
	discard_Iterator_type(&(*iterator)->parent);	
	RemoveFromParentIterator(*iterator);

	
	OKfree(*iterator);
}   */

IterTypes GetIteratorType (Iterator_type* iterator)
{
	return iterator->iterType;
}

Iterator_type* GetIteratorParent (Iterator_type* iterator)
{
	return iterator->parent;
}

void SetIteratorType (Iterator_type* iterator,IterTypes itertype)
{
	iterator->iterType=itertype;
}

size_t GetIteratorSize (Iterator_type* iterator)
{
	return ListNumItems(iterator->iterObjects);
}

void SetTotalIterations (Iterator_type* iterator, size_t nIterations)
{
	iterator->totalIter = nIterations;
}

size_t GetCurrentIterationIndex(Iterator_type* iterator)
{
	return iterator->currentIterIdx;	
}

char* GetCurrentIterationName(Iterator_type* iterator)
{
	return StrDup(iterator->name);	
}

void  SetCurrentIterationIndex	(Iterator_type* iterator,size_t index)
{
	iterator->currentIterIdx=index;
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

