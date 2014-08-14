//==============================================================================
//
// Title:		DAQLabUtility.c
// Purpose:		A short description of the implementation.
//
// Created on:	14-8-2014 at 22:25:52 by Adrian.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DAQLabUtility.h"
#include <ansi_c.h>

//==============================================================================
// Constants

#define OKfree(ptr) 				if (ptr) {free(ptr); ptr = NULL;} 

//==============================================================================
// Types

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables

//==============================================================================
// Global functions

///HIFN Use this function to compose extended error information when returning from a function.
FCallReturn_type* init_FCallReturn_type (int valFCall, const char errorOrigin[], const char errorDescription[])
{
	char*				Msg		= NULL;
	size_t				nchars;
	FCallReturn_type* 	result 	= malloc(sizeof(FCallReturn_type));
	
	if (!result) return NULL;
	
	result -> retVal 	= valFCall;
	if (errorDescription) {
		
		nchars 	= snprintf(Msg, 0, "<%s Error ID %d. Reason: %s>", errorOrigin, valFCall, errorDescription);
		Msg		= malloc((nchars+1)*sizeof(char));
		if (!Msg) {free(result); return NULL;}
		snprintf(Msg, nchars+1, "<%s Error. Reason: %s>", errorOrigin, errorDescription);
		result -> errorInfo	= Msg;
	} else
		result -> errorInfo = NULL;
		
	return result;
}

///HIFN Discards an FCallReturn_type data structure.
void discard_FCallReturn_type (FCallReturn_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->errorInfo);
	OKfree(*a);
}
