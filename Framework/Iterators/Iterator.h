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
	
	Iterator_None,					// Default for newly created iterators
	Iterator_Iterator,			  	// Iterator over other Iterator_type* iterators.
	Iterator_Waveform			  	// Iterator over Waveform_type* waveforms.
	
} IterTypes;

//==============================================================================
// External variables

//==============================================================================
// Global functions

//---------------------------------------------------------------------------------------------------------------------------
// Iterator Creation / Destruction functions
//---------------------------------------------------------------------------------------------------------------------------

	// Creates iterator
Iterator_type*			init_Iterator_type			(char name[]);

	// Discards a given iterator and all the data or iterators contained within recursively
void					discard_Iterator_type		(Iterator_type** iterator);

//---------------------------------------------------------------------------------------------------------------------------
// Get functions
//---------------------------------------------------------------------------------------------------------------------------

IterTypes				GetIteratorType				(Iterator_type* iterator);

size_t					GetIteratorSize				(Iterator_type* iterator);

size_t					GetIteratorDimensions		(Iterator_type* iterator);


//---------------------------------------------------------------------------------------------------------------------------
// Iterator composition
//---------------------------------------------------------------------------------------------------------------------------

int						IteratorAddWaveform			(Iterator_type* iterator, Waveform_type* waveform);

int						IteratorAddIterator			(Iterator_type* iterator, Iterator_type* iteratorToAdd);





#ifdef __cplusplus
    }
#endif

#endif  /* ndef __Iterator_H__ */
