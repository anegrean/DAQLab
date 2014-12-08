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

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DAQLabUtility_H__ */
