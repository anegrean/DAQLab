//==============================================================================
//
// Title:		DAQLab.h
// Purpose:		A short description of the interface.
//
// Created on:	8-3-2014 at 14:07:18 by Adrian Negrean.
// Copyright:	. All Rights Reserved.
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
#include <userint.h>
#include <utility.h>

//==============================================================================
// Constants
		
//==============================================================================
// Macros

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}

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

	

//==============================================================================
// External variables

//==============================================================================
// Global functions
	
	// installs callback data and callback function in all controls directly within panel 
int		SetCtrlsInPanCBInfo 	(void* callbackData, CtrlCallbackPtr callbackFn, int panHndl);

	// displays messages in the main workspace log panel and optionally beeps
void	DLMsg					(const char* text, BOOL beep);
	// adds multiple XML Element children or attributes for a parent XML Element
int		DLAddToXMLElem			(CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, DAQLabXMLNode childXMLNodes[], DAQLabXMLNodeTypes nodeType, size_t nNodes);
	// places the value of multiple XML Attributes of an Element into user provided pointers
int		DLGetXMLAttributes		(ActiveXMLObj_IXMLDOMElement_ parentXMLElement, DAQLabXMLNode Attributes[], size_t nAttributes);  


#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DAQLab_H__ */
