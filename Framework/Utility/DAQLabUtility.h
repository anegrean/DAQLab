//==============================================================================
//
// Title:		DAQLabUtility.h
// Purpose:		A short description of the interface.
//
// Created on:	11/12/2015 at 12:57:34 PM by Adrian Negrean.
// Copyright:	Vrije University Amsterdam. All Rights Reserved.
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
		
// Function pointer type to validate user input, return TRUE for valid input
typedef BOOL 		(*ValidateInputFptr_type) 			(char inputStr[], void* callbackData); 

//==============================================================================
// External variables

//==============================================================================
// Global functions

// Displays a popup box where the user can give a string after which a validate function pointer is called.
char*				DLGetUINameInput					(char popupWndName[], size_t maxInputLength, ValidateInputFptr_type validateInputFptr, void* callbackData);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DAQLabUtility_H__ */
