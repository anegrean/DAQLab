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
	iterator->iterType				= Iterator_None;
	iterator->iterObjects   		= 0;
	iterator->parent				= NULL;
	
	if ( !(iterator->iterObjects   	= ListCreate(sizeof(void*))) )	goto Error;
	
	return iterator;
	
Error:
	if (iterator->iterObjects) ListDispose(iterator->iterObjects);
	free(iterator);
	return NULL;
}

