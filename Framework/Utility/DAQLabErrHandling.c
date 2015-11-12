//==============================================================================
//
// Title:		DAQLabUtility.c
// Purpose:		A short description of the implementation.
//
// Created on:	14-8-2014 at 22:25:52 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DAQLabErrHandling.h"
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

char* FormatMsg (int messageID, char fileName[], char functionName[], int lineNumber, char message[])
{
#define	FormatMsg_Message  	"<File \"%s\", Function \"%s\", Line %d: %s>\n"
#define	FormatMsg_Error	 	"<File \"%s\", Function \"%s\", Line %d, Error %d, Reason:\n\t  %s>"	
#define	FormatMsg_Warning 	"<File \"%s\", Function \"%s\", Line %d, Warning %d, Reason:\n\t  %s>"
	
	size_t	nChars		= 0;
	char*   msg			= NULL;
	
	// check file name
	if (!fileName || !fileName[0])
		fileName = "unknown";
	
	// check function name
	if (!functionName || !functionName[0])
		functionName = "unknown";
	
	if (!message || !message[0])
		message = "unknown";
	
	if (!messageID) {
		
		// no error or warning, just a message
		nChars = snprintf(msg, 0, FormatMsg_Message, fileName, functionName, lineNumber, message); 
		msg = malloc((nChars+1) * sizeof(char));
		if (!msg) return NULL;
		snprintf(msg, nChars+1, FormatMsg_Message, fileName, functionName, lineNumber, message);
		
	} else
		if (messageID < 0) {
			// error message
			nChars = snprintf(msg, 0, FormatMsg_Error, fileName, functionName, lineNumber, messageID, message); 
			msg = malloc((nChars+1) * sizeof(char));
			if (!msg) return NULL;
			snprintf(msg, nChars+1, FormatMsg_Error, fileName, functionName, lineNumber, messageID, message);
		} else {
			// warning message
			nChars = snprintf(msg, 0, FormatMsg_Warning, fileName, functionName, lineNumber, messageID, message); 
			msg = malloc((nChars+1) * sizeof(char));
			if (!msg) return NULL;
			snprintf(msg, nChars+1, FormatMsg_Warning, fileName, functionName, lineNumber, messageID, message);
		}
	
	return msg;
}
