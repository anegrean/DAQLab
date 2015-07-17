//==============================================================================
//
// Title:		DAQLabUtility.h
// Purpose:		A short description of the interface.
//
// Created on:	14-8-2014 at 22:25:52 by Adrian.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

#ifndef __DAQLabUtility_H__
#define __DAQLabUtility_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "cvidef.h"

//==============================================================================
// Constants
		
		
//==============================================================================
// Macros

#define OKfree(ptr) 					if (ptr) {free(ptr); ptr = NULL;}
#define OKfreeCAHndl(objHandle)			if (objHandle) {CA_DiscardObjHandle(objHandle); objHandle = 0;}
#define OKfreePanHndl(panelHandle)  	if (panelHandle) {DiscardPanel(panelHandle); panelHandle = 0;}
#define OKfreeList(list) 				if (list) {ListDispose(list); list = 0;}  

#define NumElem(ptr) (sizeof(ptr)/sizeof(ptr[0]))	 // Note: do not use this inside a function to 
													 // get the number of elements in an array passed as an argument!
													 
// Error handling for functions that can return an error message as a char** errorInfo parameter
//
// define these variables:
//		int		error;
//		char*	errMsg;
// 
// and make sure the function returns an int error code as well as has a char** errorInfo parameter

#define ReturnErrMsg(FunctionName) if (!errMsg) errMsg = FormatMsg(error, FunctionName, "Unknown or out of memory");	\
	if (errorInfo)																										\
		*errorInfo = errMsg;																							\
	else																												\
		OKfree(errMsg);

//==============================================================================
// Types

// Used to return meaningful error information
typedef struct FCallReturn {
	int						retVal;			// Value returned by function call.
	char*					errorInfo;		// In case of error, additional info.
} FCallReturn_type;

//==============================================================================
// External variables

//==============================================================================
// Global functions

// Used to return extended error info in case of error.
FCallReturn_type*	init_FCallReturn_type			(int valFCall, const char errorOrigin[], const char errorDescription[]);

void				discard_FCallReturn_type		(FCallReturn_type** fCallReturnPtr);

// Formats error and warning messages ( errors: msgID < 0, message: msgID = 0, warning: msgID > 0 )
char* 				FormatMsg 						(int messageID, char messageOrigin[], char message[]);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DAQLabUtility_H__ */
