//==============================================================================
//
// Title:		DAQLab.h
// Purpose:		A short description of the interface.
//
// Created on:	8-3-2014 at 14:07:18 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __DAQLab_H__
#define __DAQLab_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files
		
#include "ActiveXML.h"        // must be first to be included 
#include "cvidef.h"
#include <toolbox.h>
#include <userint.h>
#include <utility.h>
#include "VChannel.h"

//==============================================================================
// Constants

	// Max number of characters for a virtual channel name
#define	DAQLAB_MAX_VCHAN_NAME	50
	// maximum DAQLab module instance name length
#define DAQLAB_MAX_MODULEINSTANCE_NAME_NCHARS		50
		
//==============================================================================
// Macros

#define OKfree(ptr) 				if (ptr) {free(ptr); ptr = NULL;}

#define NumElem(ptr) (sizeof(ptr)/sizeof(ptr[0]))	 // Note: do not use this inside a function to 
													 // get the number of elements in an array passed as an argument!

#define XMLErrChk(fCall) if (xmlerror = (fCall), xmlerror < 0) \
{goto XMLError;} else
//==============================================================================
// Types
	
	// DAQLab XML data types
typedef enum {
	
	DL_NULL,
	DL_BOOL,
	DL_CHAR,
	DL_UCHAR,
	DL_SHORT,
	DL_USHORT,
	DL_INT,
	DL_UINT,
	DL_LONG,
	DL_ULONG,
	DL_LONGLONG,
	DL_ULONGLONG,
	DL_FLOAT,
	DL_DOUBLE,
	DL_CSTRING		// null-terminated string
	
} DAQLabXMLTypes;

typedef enum {
	
	DL_ELEMENT,
	DL_ATTRIBUTE
	
} DAQLabXMLNodeTypes;
		
	// DAQLab node type used to define an array of XML child nodes to be added to a parent node
typedef struct{
	
	char*			tag;	// null-terminated string
	DAQLabXMLTypes 	type;
	void*			pData;
	
} DAQLabXMLNode;

	// function pointer type to validate user input, return TRUE for valid input
typedef BOOL (*ValidateInputFptr_type) (char inputStr[], void* dataPtr); 
	

//==============================================================================
// Global functions

//-------------------------------------------------------------------------------
// DAQLab Virtual Channel management
//-------------------------------------------------------------------------------

	// Registers a VChan with the DAQLab framework
BOOL				DLRegisterVChan					(VChan_type* vchan);

	// Deregisters a VChan from the DAQLab framework
BOOL				DLUnregisterVChan				(VChan_type* vchan);

	// Searches for a given VChan object and if found, return pointer to it. 
VChan_type**		DLVChanExists					(VChan_type* vchan, size_t* idx);

	// Searches for a given VChan name and if found, return pointer to it. 
VChan_type**		DLVChanNameExists				(char name[], size_t* idx);

	// To Validate a new VChan name, pass this function as a parameter when calling DLGetUINameInput.
BOOL				DLValidateVChanName				(char newVChanName[], void* null);

//-------------------------------------------------------------------------------
// DAQLab handy programmer's tools
//-------------------------------------------------------------------------------

	// Installs callback data and callback function in all controls directly within panel 
int		SetCtrlsInPanCBInfo 			(void* callbackData, CtrlCallbackPtr callbackFn, int panHndl);
	// Checks if all strings in a list are unique; strings must be of char* type
BOOL	DLUniqueStrings					(ListType stringList, size_t* idx);

//-------------------------------------------------------------------------------
// DAQLab user interface management
//-------------------------------------------------------------------------------

	// displays messages in the main workspace log panel and optionally beeps
void	DLMsg							(const char* text, BOOL beep);
	// displays a popup box where the user can give a string after which a validate function pointer is called
char*	DLGetUINameInput				(char popupWndName[], size_t maxInputLength, ValidateInputFptr_type validateInputFptr, void* dataPtr);

//-------------------------------------------------------------------------------
// DAQLab module management
//-------------------------------------------------------------------------------

BOOL DAQLab_ValidModuleName (char name[], void* listPtr);

//-------------------------------------------------------------------------------
// DAQLab XML management
//-------------------------------------------------------------------------------
	// adds multiple XML Element children or attributes for a parent XML Element
int		DLAddToXMLElem					(CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, DAQLabXMLNode childXMLNodes[], DAQLabXMLNodeTypes nodeType, size_t nNodes);
	// places the value of multiple XML Attributes of an Element into user provided pointers
int		DLGetXMLElementAttributes		(ActiveXMLObj_IXMLDOMElement_ XMLElement, DAQLabXMLNode Attributes[], size_t nAttributes);
	// places the value of multiple XML Attributes of a Nodet into user provided pointers
int 	DLGetXMLNodeAttributes 			(ActiveXMLObj_IXMLDOMNode_ XMLNode, DAQLabXMLNode Attributes[], size_t nAttributes);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DAQLab_H__ */
