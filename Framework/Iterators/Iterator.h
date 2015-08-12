//==============================================================================
//
// Title:		Iterator.h
// Purpose:		A short description of the interface.
//
// Created on:	10/8/2014 at 3:07:21 PM by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __Iterator_H__
#define __Iterator_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "DataTypes.h"
#include "toolbox.h"
//#include "DataStorage.h"
		
//==============================================================================
// Constants
# define WAVERANK	1 
# define IMAGERANK	2
# define MAXRANK	2 //max rank of data


//==============================================================================
// Types
		
	// Base Iterator type for indexing data
typedef struct Iterator		Iterator_type;
typedef struct DSInfo		DSInfo_type; 


typedef enum {
	Iterator_None,												// Default for newly created iterators
	Iterator_Iterator,			  								// Iterator over other Iterator_type* iterators.
	Iterator_Waveform,			  								// Iterator over Waveform_type* waveforms.
	Iterator_Image			  								    // Iterator over Images  
} IterTypes;		   

typedef void (*DiscardDataFptr_type) (void** PtrToData);		// Function pointer for discarding data contained in data type iterators.

//==============================================================================
// External variables

//==============================================================================
// Global functions

//---------------------------------------------------------------------------------------------------------------------------
// Iterator Creation / Destruction functions
//---------------------------------------------------------------------------------------------------------------------------

	// Creates iterator.
Iterator_type*			init_Iterator_type			(char name[]);

	// Discards a given iterator and all the data or iterators contained within recursively
void					discard_Iterator_type		(Iterator_type** iteratorPtr);


void 					RemoveFromParentIterator	(Iterator_type* iterator);

//---------------------------------------------------------------------------------------------------------------------------
// Set/Get functions
//---------------------------------------------------------------------------------------------------------------------------

	// Returns the type of an iterator
IterTypes				GetIteratorType				(Iterator_type* iterator);
	// Sets the type of an iterator   
void 					SetIteratorType 			(Iterator_type* iterator,IterTypes itertype);
	// Returns the iterator parent  
Iterator_type* 			GetIteratorParent 			(Iterator_type* iterator);

	// Returns the number of iteratable objects 
size_t					GetIteratorSize				(Iterator_type* iterator);

	// 	Returns the current Iteration Index
size_t 					GetCurrentIterIndex			(Iterator_type* iterator);

	// 	Sets the current Iteration Index
void 					SetCurrentIterIndex			(Iterator_type* iterator, size_t index);

	// Set total number of iterations for a given iterator
void					SetTotalIterations			(Iterator_type* iterator, size_t nIterations);

size_t 					GetTotalIterations 			(Iterator_type* iterator);

	// Returns the current iterator set as a list of Iterator_type* elements. The number of iterators in the list determines the dimensionality of the set.
	// To return all current iterators apply this function to the root iterator. When not needed anymore, dispose of the list using ListDispose().
ListType				GetCurrentIteratorSet		(Iterator_type* iterator);

	// Returns the root iterator of an iterator set. If this is already a root iterator, it returns the same pointer.
Iterator_type*			GetRootIterator				(Iterator_type* iterator);

	// Returns TRUE if the provided iterator is a root iterator, i.e. it does not have another parent iterator, and FALSE otherwise
BOOL					IsRootIterator				(Iterator_type* iterator);

	// Returns the Iterators name
char* 					GetCurrentIterName		    (Iterator_type* iterator);

void 					SetCurrentIterName			(Iterator_type* iterator,char name[]);

//---------------------------------------------------------------------------------------------------------------------------
// Iterator composition
//---------------------------------------------------------------------------------------------------------------------------

	// Adds a waveform to an iterator
int						IteratorAddWaveform			(Iterator_type* iterator, Waveform_type* waveform);

	// Adds an iterator to another iterator
int						IteratorAddIterator			(Iterator_type* iterator, Iterator_type* iteratorToAdd, char** errorMsg);

void					SetIteratorStackData 		(Iterator_type* iterator, BOOL stackdata);

BOOL 					GetIteratorStackData		(Iterator_type* iterator);

//---------------------------------------------------------------------------------------------------------------------------
// Iterator iteration control
//---------------------------------------------------------------------------------------------------------------------------

	// Resets the current 0-based iteration index for the given iterator and all its child iterators recursively to 0.
	// Note: To reset iterations for all the iterators in the interation set, apply this function to the root iterator
void					ResetIterators				(Iterator_type* iterator);

	// Advances the iteration of an iterator set. Returns the next set of iterators as a list of Iterator_type* elements 
	// if the iteration was advanced, otherwise the function returns 0. When not needed anymore, dispose of the list using ListDispose().
	// Note: The iteration indices for each iterator in the set cannot be larger than the number of iterator objects in the set.
ListType				IterateOverIterators		(Iterator_type* iterator);



//---------------------------------------------------------------------------------------------------------------------------
// Data Storage data
//---------------------------------------------------------------------------------------------------------------------------


	// get DataStorage data from the iterator
DSInfo_type*			GetIteratorDSData			(Iterator_type* iterator, unsigned int datarank);
void 					discard_DSInfo_type 		(DSInfo_type** dsInfoPtr);

void					SetDSInfoGroupName			(DSInfo_type* dsInfo, char groupName[]);

char*					GetDSInfoGroupName			(DSInfo_type* dsInfo); 

void 					SetDSInfoIterIndices		(DSInfo_type* dsInfo, unsigned int** iterIndicesPtr);

unsigned int* 			GetDSInfoIterIndices 		(DSInfo_type* dsInfo);

void 					SetDSInfoDatasetRank		(DSInfo_type* dsInfo,unsigned int rank);

unsigned int 			GetDSInfoDatasetRank 		(DSInfo_type* dsInfo);

unsigned int 			GetDSDataRank 				(DSInfo_type* dsInfo);

void 					SetDSInfoStackData			(DSInfo_type* dsInfo,BOOL stackdata);

BOOL 					GetDSInfoStackData 			(DSInfo_type* dsInfo);

				

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __Iterator_H__ */
