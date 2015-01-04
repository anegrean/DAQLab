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
	if (errorDescription[0]) {
		
		nchars 	= snprintf(Msg, 0, "<%s Error ID %d. Reason: %s>", errorOrigin, valFCall, errorDescription);
		Msg		= malloc((nchars+1)*sizeof(char));
		if (!Msg) {free(result); return NULL;}
		snprintf(Msg, nchars+1, "<%s Error. Reason: %s>", errorOrigin, errorDescription);
		result -> errorInfo	= Msg;
	} else
		result -> errorInfo = NULL;
	
	return result;
}

/// HIFN Discards an FCallReturn_type data structure.
void discard_FCallReturn_type (FCallReturn_type** fCallReturnPtr)
{
	if (!*fCallReturnPtr) return;
	
	OKfree((*fCallReturnPtr)->errorInfo);
	OKfree(*fCallReturnPtr);
}


char* FormatMsg (int messageID, const char messageOrigin[], const char message[])
{
	size_t	nChars;
	char*   msg			= NULL;
	char*	msgText		= message;
	char*	msgOrigin	= messageOrigin;
	
	// check message origin
	if (!messageOrigin)
		msgOrigin = "Unknown";
	else
		if (!messageOrigin[0])
			msgOrigin = "Unknown";
	
	// check message text
	if (!message)
		msgText = "Unknown";
	else
		if (!message[0])
			msgText = "Unknown";
	
	if (!messageID) {
		// no error or warning, just a message
		nChars 		= snprintf(msg, 0, "<%s: %s>", msgOrigin, msgText); 
		msg		 	= malloc((nChars+1) * sizeof(char));
		if (!msg) return NULL;
		snprintf(msg, nChars+1, "<%s: %s>", msgOrigin, msgText);
	} else
		if (messageID < 0) {
			// error message
			nChars 		= snprintf(msg, 0, "<%s Error ID: %d, Reason:\n\t %s>", msgOrigin, messageID, msgText); 
			msg		 	= malloc((nChars+1) * sizeof(char));
			if (!msg) return NULL;
			snprintf(msg, nChars+1, "<%s Error ID: %d, Reason:\n\t %s>", msgOrigin, messageID, msgText); 
		} else {
			// warning message
			nChars 		= snprintf(msg, 0, "<%s Warning ID: %d, Reason:\n\t %s>", msgOrigin, messageID, msgText); 
			msg		 	= malloc((nChars+1) * sizeof(char));
			if (!msg) return NULL;
			snprintf(msg, nChars+1, "<%s Warning ID: %d, Reason:\n\t %s>", msgOrigin, messageID, msgText); 
		}
	
	return msg;
}
