//==============================================================================
//
// Title:		DAQLabUtility.c
// Purpose:		Collection of useful functions.
//
// Created on:	11/12/2015 at 12:57:34 PM by Adrian Negrean.
// Copyright:	Vrije University Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
//
//==============================================================================

//==============================================================================
// Include files

#include "DAQLabUtility.h"
#include "DAQLabErrHandling.h"
#include <ansi_c.h>
#include <userint.h>
#include "toolbox.h"


//==============================================================================
// Constants

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

/// HIFN Displays a popup box where the user can give a string after which a validate function pointer is called. 
/// HIFN If the string is not validated for example because the name already exists, then the popup box appears again until a valid name is given or until it is cancelled.
/// HIPAR validateInputFptr/ if function returns TRUE, input is valid, otherwise not and popup is displayed again.
/// HIRET Dynamically allocated string if input was succesful or NULL if operation was cancelled.
char* DLGetUINameInput (char popupWndName[], size_t maxInputLength, ValidateInputFptr_type validateInputFptr, void* callbackData)
{
	char* 	newName 		= malloc ((maxInputLength+1)*sizeof(char));
	char	msg1[]			= "Enter name.";
	char	msg2[]			= "Invalid name. Enter new name.";
	char	msg3[]			= "Names must be different. Enter new name.";
	char*	popupMsg		= NULL;								 
	int		selectedBTTN	= 0;
	
	if (!validateInputFptr) {OKfree(newName); return NULL;}  // do nothing 
	popupMsg = msg1; 
	do {
		NewInput:
		// obtain new name if one is given, else nothing happens
		selectedBTTN = GenericMessagePopup (popupWndName, popupMsg, "Ok", "Cancel", "", newName, maxInputLength * sizeof(char),
											 0, VAL_GENERIC_POPUP_INPUT_STRING,VAL_GENERIC_POPUP_BTN1,VAL_GENERIC_POPUP_BTN2);
		// remove surrounding white spaces 
		RemoveSurroundingWhiteSpace(newName);
		
		// if OK and name is not valid
		if ( (selectedBTTN == VAL_GENERIC_POPUP_BTN1) && !*newName ) {
			popupMsg = msg2;
			goto NewInput;  // display again popup to enter new name
		} else
			// if cancelled
			if (selectedBTTN == VAL_GENERIC_POPUP_BTN2) {OKfree(newName); return NULL;}  // do nothing
			
		// name is valid, provided function pointer is called for additional check 
			
		// if another Task Controller with the same name exists display popup again
		popupMsg = msg3;
		
	} while (!(*validateInputFptr)(newName, callbackData)); // repeat while input is not valid
	
	return 	newName;
	
}

char* DL_DynStringCat (char string1[], ...)
{
INIT_ERR

	va_list 	arguments;
	char*		catStrings	= StrDup(string1);
	char*		strArg		= NULL;
	
	if (!catStrings) return NULL;
	
	// initializing arguments to store all values after string1
    va_start(arguments, string1);           
	
    // concatenate strings
	while ((strArg = va_arg(arguments, char* ))) {
		nullChk( AppendString(&catStrings, strArg, -1) );
	}
	
	// clean up the list
    va_end ( arguments );

    return catStrings;
	
Error:
	
	// clean up the list
	va_end ( arguments );
	
	OKfree(catStrings);
	return NULL;
}
