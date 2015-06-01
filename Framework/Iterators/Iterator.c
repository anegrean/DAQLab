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

#include <formatio.h>
#include "Iterator.h"
#include "toolbox.h"

//==============================================================================
// Constants

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;} 
#define MAXGROUPNAME			MAX_PATHNAME_LEN
#define MAXDATARANK				255	

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
	BOOL					stackdata;				// Combine generated data into a one-dimension higher stack of datasets 
	TC_DS_Data_type*		ds_data;				// pointer to DataStorage data
};

//Task controller Data Storage data, 
//Is send inside a datapacket to the datastorage module
//to save the data in the proper location 
struct TC_DS_Data{
	char*			groupname;  	// to create a HDF5 group for the dataset
	BOOL			stackeddata;	// true if data is stacked
	unsigned int 	datasetrank;	// rank of the dataset 
	unsigned int 	datarank;		// rank of the data (1 for waveform, 2 for Images)
	unsigned int*	iter_indices;   // array of indices of the dataset
};

   
//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables

//==============================================================================
// Global functions

TC_DS_Data_type* init_TC_DS_Data_type(void);


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
	iterator->ds_data				= init_TC_DS_Data_type();  
	
	if ( !(iterator->iterObjects   	= ListCreate(sizeof(void*))) )	goto Error;
	
	return iterator;
	
Error:
	if (iterator->iterObjects) ListDispose(iterator->iterObjects);
	free(iterator);
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



   
void discard_Iterator_type (Iterator_type** iteratorptr)
{
	Iterator_type* iterator=*iteratorptr;
	size_t 				nObjects;
	Iterator_type**		iterObjectPtr;
	size_t 				i;
	
	if (!iterator) return;
	if (iterator->name==NULL) return;
	OKfree(iterator->name);
	
	
	
	nObjects 		= ListNumItems(iterator->iterObjects);   
	for (i = 1; i <= nObjects; i++) {
		iterObjectPtr = ListGetPtrToItem(iterator->iterObjects, i);
		if (iterator->iterType == Iterator_Iterator)  {
			discard_Iterator_type(iterObjectPtr);   							// discard recursively if object is another iterator
			
		}
		else
			(*(*iterObjectPtr)->discardDataFptr)	((void**)iterObjectPtr);	// discard data if data type iterator
	}   
		
	ListDispose(iterator->iterObjects); 
	if (iterator->ds_data) discard_TC_DS_Data_type(&iterator->ds_data);

	OKfree(*iteratorptr);
}   


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

void SetCurrentIterName(Iterator_type* iterator,char* name)
{
	iterator->name=name;	
}

void  SetCurrentIterIndex	(Iterator_type* iterator,size_t index)
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


void FillDSData(Iterator_type* iterator,TC_DS_Data_type* ds_data,unsigned int datarank)	    //
{
	int 			rank=0;
	Iterator_type* 	parentiter;
	Iterator_type* 	childiter; 
	char*			tcname;
	char*			name; 
	size_t			iteridx;
	char* 			fullgroupname;
	BOOL			determining_rank=TRUE;
	int 			indices[MAXDATARANK];   
	size_t			totaliter;
	BOOL			stackdata;

	if (iterator==NULL) {
		ds_data=NULL;
		return;
	}
	
	parentiter=GetIteratorParent(iterator);
	if (parentiter==NULL) {  //no parent
		ds_data=NULL;
		return;
	}
	iteridx=GetCurrentIterIndex(iterator);  
	totaliter=GetTotalIterations(iterator);
	//send indices of sending tc  as well
	indices[rank]=iteridx;	
	rank++;  
	
	tcname=GetCurrentIterName(parentiter);
	iteridx=GetCurrentIterIndex(parentiter); 
	stackdata=GetIteratorStackData(parentiter);
	fullgroupname=malloc(MAXGROUPNAME*sizeof(char));  
	if (!stackdata) Fmt(fullgroupname, "%s<%s[w3]#%i",tcname,iteridx); 
	else Fmt(fullgroupname, "%s<%s[w3]",tcname); 
	OKfree(tcname);
	while (parentiter!=NULL){
		if (determining_rank&&stackdata) {
			indices[rank]=GetCurrentIterIndex(parentiter);
			rank++;
		}
		else {
			determining_rank=FALSE;	
		}
		childiter=parentiter;
		parentiter=GetIteratorParent(childiter);
		if (parentiter==NULL) {
			break;  //no more parent
		}
		tcname=GetCurrentIterName(parentiter);
		iteridx=GetCurrentIterIndex(parentiter);
		stackdata=GetIteratorStackData(parentiter); 
		name=malloc(MAXGROUPNAME*sizeof(char));
		if (!stackdata) Fmt(name, "%s<%s[w3]#%i",tcname,iteridx); 
		else Fmt(name, "%s<%s[w3]",tcname); 		
		AddStringPrefix (&fullgroupname,"/",-1);    
		AddStringPrefix (&fullgroupname,name,-1);
		OKfree(tcname);
		OKfree(name);
	}
	
	if (rank!=0){
		OKfree(ds_data->iter_indices);
		ds_data->iter_indices=malloc(rank*sizeof(unsigned int));
		//rearrange rank; Most significant rank first
		for (int i=0;i<rank; i++) ds_data->iter_indices[i]=indices[rank-i-1];
	}
	else ds_data->iter_indices=NULL;
	
	ds_data->groupname=StrDup(fullgroupname);
	ds_data->datasetrank=rank;
	ds_data->stackeddata=iterator->stackdata; 
	ds_data->datarank=datarank;
	
	free(fullgroupname);
	
}


	//get DataStorage data from the iterator
void					SetIteratorDSdata			(Iterator_type* iterator,TC_DS_Data_type* dsdata)
{
	 iterator->ds_data=dsdata;
}

//set DS data 
TC_DS_Data_type*		GetIteratorDSdata			(Iterator_type* iterator,unsigned int datarank)
{
	 TC_DS_Data_type* ds_data=init_TC_DS_Data_type();
	 
	 FillDSData(iterator,ds_data,datarank);  
	 
	 return ds_data;
}


TC_DS_Data_type* init_TC_DS_Data_type(void)
{
	TC_DS_Data_type* ds_data	= malloc(sizeof(TC_DS_Data_type));
	
	ds_data->groupname			= NULL;
	ds_data->stackeddata		= FALSE;
	ds_data->datasetrank		= 0;
	ds_data->iter_indices		= NULL;
	ds_data->datarank			= 0;
	
	return ds_data;
}

void discard_TC_DS_Data_type (TC_DS_Data_type** dsDataPtr)
{ 
	TC_DS_Data_type*	dsData = *dsDataPtr;
	
	if (!dsData) {
		OKfree(*dsDataPtr);
		return;
	}
	OKfree(dsData->groupname);
	//OKfree(dsData->iter_indices); 
	
	OKfree(*dsDataPtr);
}   


void SetDSdataGroupname	(TC_DS_Data_type* dsdata,char* groupname)
{
	dsdata->groupname=groupname;
}

char* GetDSdataGroupname (TC_DS_Data_type* dsdata)
{
	char* groupname =dsdata->groupname;
	
	return groupname;
}


void SetDSdataIterIndices	(TC_DS_Data_type* dsdata,unsigned int* iter_indices)
{
	dsdata->iter_indices=iter_indices;
}

unsigned int* GetDSdataIterIndices (TC_DS_Data_type* dsdata)
{
	unsigned int* iter_indices =dsdata->iter_indices;
	
	return iter_indices;
}

void SetDSDataSetRank	(TC_DS_Data_type* dsdata,unsigned int rank)
{
	dsdata->datasetrank=rank;
}

unsigned int GetDSDataSetRank (TC_DS_Data_type* dsdata)
{
	BOOL datasetrank =dsdata->datasetrank;
	
	return datasetrank;
}

void SetDSdataStackData	(TC_DS_Data_type* dsdata,BOOL stackdata)
{
	dsdata->stackeddata = stackdata;
}

BOOL GetDSdataStackData (TC_DS_Data_type* dsdata)
{
	BOOL stackdata = dsdata->stackeddata;
	
	return stackdata;
}

unsigned int GetDSDataRank (TC_DS_Data_type* dsdata)
{
	BOOL datarank =dsdata->datarank;
	
	return datarank;
}



