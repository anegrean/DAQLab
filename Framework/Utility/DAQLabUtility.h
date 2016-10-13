//==============================================================================
//
// Title:		DAQLabUtility.h
// Purpose:		Collection of useful functions.
//
// Created on:	11/12/2015 at 12:57:34 PM by Adrian Negrean.
// Copyright:	Vrije University Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
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

// Concatenates several input strings and returns the resulting dynamically allocated string. Note, for this function to work correctly, the last string argument must
// be always set to NULL. Instead of using the function directly, it is more convenient to use the macro which automatically includes the NULL string as the last argument.
#define 			DLDynStringCat(string1, ...)		DL_DynStringCat(string1, __VA_ARGS__, NULL)
char*				DL_DynStringCat						(char string1[], ...);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DAQLabUtility_H__ */
