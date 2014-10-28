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
		
//==============================================================================
// Constants

//==============================================================================
// Types
		
	// Base Iterator type for indexing data
typedef struct Iterator		Iterator_type;


typedef enum {
	
	Iterator_None,												// Default for newly created iterators
	Iterator_Iterator,			  								// Iterator over other Iterator_type* iterators.
	Iterator_Waveform			  								// Iterator over Waveform_type* waveforms.
	
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
void					discard_Iterator_type		(Iterator_type** iterator);

//---------------------------------------------------------------------------------------------------------------------------
// Set/Get functions
//---------------------------------------------------------------------------------------------------------------------------

	// Returns the type of an iterator
IterTypes				GetIteratorType				(Iterator_type* iterator);

	// Returns the number of iteratable objects 
size_t					GetIteratorSize				(Iterator_type* iterator);

	// Set total number of iterations for a given iterator
void					SetTotalIterations			(Iterator_type* iterator, size_t nIterations);

	// Returns the current iterator set as a list of Iterator_type* elements. The number of iterators in the list determines the dimensionality of the set.
	// To return all current iterators apply this function to the root iterator. When not needed anymore, dispose of the list using ListDispose().
ListType				GetCurrentIteratorSet		(Iterator_type* iterator);

	// Returns the root iterator of an iterator set. If this is already a root iterator, it returns the same pointer.
Iterator_type*			GetRootIterator				(Iterator_type* iterator);

	// Returns TRUE if the provided iterator is a root iterator, i.e. it does not have another parent iterator, and FALSE otherwise
BOOL					IsRootIterator				(Iterator_type* iterator);

//---------------------------------------------------------------------------------------------------------------------------
// Iterator composition
//---------------------------------------------------------------------------------------------------------------------------

	// Adds a waveform to an iterator
int						IteratorAddWaveform			(Iterator_type* iterator, Waveform_type* waveform);

	// Adds an iterator to another iterator
int						IteratorAddIterator			(Iterator_type* iterator, Iterator_type* iteratorToAdd);

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



				

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __Iterator_H__ */