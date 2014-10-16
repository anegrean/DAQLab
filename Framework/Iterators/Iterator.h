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
void					SetIteratorIterations		(Iterator_type* iterator, size_t nIterations);

	// Returns the current 0-based iteration indices of the iterator set. Provide a currentIterIndex buffer with at
	// least as many elements as obtained from GetIteratorDimensions. Note:To return all indices apply this
	// function to the root iterator.
void					GetCurrentIterationIndices	(Iterator_type* iterator, size_t currentIterIndex[]);

	// Returns the dimensionality of the iterator. Note: The dimensionality is calculated by considering its children iterators.
	// To get the dimensionality of the entire iterator set, apply this function to the root iterator
size_t					GetIteratorDimensions		(Iterator_type* iterator);

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
void					ResetAllIterators			(Iterator_type* iterator);

	// Advances the iteration of a composite iterator. Valid only for root iterators. Provide currentIterIndex array buffer with at least
	// as many elements as the iterator dimension obtained by calling GetIteratorDimensions. Returns TRUE if the iterator can be advanced
	// further, and FALSE if there are no more iterations.
BOOL					AdvanceIterationIndex		(Iterator_type* iterator, size_t currentIterIndex[]);





#ifdef __cplusplus
    }
#endif

#endif  /* ndef __Iterator_H__ */
