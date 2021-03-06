//==============================================================================
//
// Title:		DAQLab.h
// Purpose:		DAQLab entry point.
//
// Created on:	8-3-2014 at 14:07:18 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
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
#include "DAQLabErrHandling.h" 
#include "cvidef.h"
#include <toolbox.h>
#include <userint.h>
#include <utility.h>
#include "VChannel.h"
#include "TaskController.h"
#include "DAQLabModule.h"
#include "HWTriggering.h"
#include "DataTypes.h"

//==============================================================================
// Constants

	// Max number of characters for a virtual channel name
#define	DAQLAB_MAX_VCHAN_NAME	50
	// maximum DAQLab module instance name length
#define DAQLAB_MAX_MODULEINSTANCE_NAME_NCHARS		50
	// maximum Task Controller name length
#define DAQLAB_MAX_TASKCONTROLLER_NAME_NCHARS		50	
	
//==============================================================================
// Macros
	
// print error info in the worspace log box	
#define PRINT_ERR \
	if (errorInfo.error < 0) { \
		char* msgBuff = NULL; \
		if (!errorInfo.errMsg) { \
			msgBuff = FormatMsg(errorInfo.error, __FILE__, __func__, errorInfo.line, "Unknown error or out of memory."); \
			DLMsg(msgBuff, 1); \
			DLMsg("\n\n", 0); \
		} else { \
			msgBuff = FormatMsg(errorInfo.error, __FILE__, __func__, errorInfo.line, errorInfo.errMsg); \
			DLMsg(msgBuff, 1); \
			DLMsg("\n\n", 0); \
		} \
		OKfree(msgBuff);\
		OKfree(errorInfo.errMsg); \
	} \
		
		
	


//==============================================================================
// Types

typedef enum {
	
	DL_ELEMENT,
	DL_ATTRIBUTE
	
} DAQLabXMLNodeTypes;
		
	// DAQLab node type used to define an array of XML child nodes to be added to a parent node
typedef struct{
	
	char*			tag;	// null-terminated string
	BasicDataTypes 	type;
	void*			pData;
	
} DAQLabXMLNode;

//==============================================================================
// Global functions

//-------------------------------------------------------------------------------
// DAQLab Virtual Channel management
//-------------------------------------------------------------------------------

	// Registers a VChan with the DAQLab framework. If the VChan does not belong to a module, pass NULL for the module reference
BOOL				DLRegisterVChan						(DAQLabModule_type* mod, VChan_type* VChan);

	// Unregisters a VChan from the DAQLab framework
BOOL				DLUnregisterVChan					(DAQLabModule_type* mod, VChan_type* VChan);

	// Updates the Switchboard explicitely.
void				DLUpdateSwitchboard					(void);

	// Unregisters all VChans from a module from the DAQLab framework
void				DLUnregisterModuleVChans			(DAQLabModule_type* mod);

	// Checks if a given VChan object exists
BOOL				DLVChanExists						(VChan_type* VChan, size_t* idx);

	// Searches for a given VChan name
VChan_type*			DLVChanNameExists					(char VChanName[], size_t* idx);

	// Generates a VChan Name with the form: "ModuleInstanceName: TaskControllerName: VChanName idx" where ModuleInstanceName, TaskControllerName and idx are optional.
	// Set idx to 0 if indexing is not needed.
char* 				DLVChanName			 				(DAQLabModule_type* mod, TaskControl_type* taskController, char VChanName[], uInt32 idx);

	// To Validate a new VChan name, pass this function as a parameter when calling DLGetUINameInput.
BOOL				DLValidateVChanName					(char newVChanName[], void* null);

BOOL				DLRenameVChan						(VChan_type* VChan, char newName[]);

//------------------------------------------------------------------------------------------------------------------------------------------------------
// HW trigger dependencies
//------------------------------------------------------------------------------------------------------------------------------------------------------

	//--------------------
	// HW Masters
	//--------------------
	
	// Registers a Master HW-trigger with the framework
BOOL				DLRegisterHWTrigMaster				(HWTrigMaster_type* master);

	// Unregisters a Master HW-trigger from the framework
BOOL				DLUnregisterHWTrigMaster			(HWTrigMaster_type* master);

	//--------------------
	// HW Slaves
	//--------------------
	
	// Registers a Slave HW-trigger with the framework
BOOL				DLRegisterHWTrigSlave				(HWTrigSlave_type* slave);

	// Unregisters a Slave HW-trigger from the framework
BOOL				DLUnregisterHWTrigSlave				(HWTrigSlave_type* slave);

//-------------------------------------------------------------------------------
// DAQLab handy programmer's tools
//-------------------------------------------------------------------------------

	// Installs callback data and callback function in all controls directly within panel 
int					SetCtrlsInPanCBInfo 				(void* callbackData, CtrlCallbackPtr callbackFn, int panHndl);

// Gets a string from a text box control getting rid of leading and trailing white spaces.
char* 				GetStringFromControl				(int panelHandle, int stringControlID);

	// Checks if all strings in a list are unique; strings must be of char* type
BOOL				DLUniqueStrings						(ListType stringList, size_t* idx);

	// Copies a list of strings to a new list.
ListType 			StringListCpy						(ListType src); 

//-------------------------------------------------------------------------------
// DAQLab user interface management
//-------------------------------------------------------------------------------

	// displays messages in the main workspace log panel and optionally beeps
void				DLMsg								(const char* text, BOOL beep);

//-------------------------------------------------------------------------------
// DAQLab module and Task Controller management
//-------------------------------------------------------------------------------

	// Checks if module instance name is unique within the framework.
BOOL 				DLValidModuleInstanceName			(char name[]);

	// Checks if Task Controller name is unique within the framework.
BOOL 				DLValidTaskControllerName			(char name[]);

	// Returns a unique Task Controller name among existing Task Controllers within the framework
char*				DLGetUniqueTaskControllerName		(char baseTCName[]);

	// Adds a list of Task Controllers to the DAQLab framework.
	// tcList of TaskControl_type* 
BOOL				DLAddTaskControllers				(DAQLabModule_type* mod, ListType tcList);
	// Adds a single Task Controller to the DAQLab framework
BOOL				DLAddTaskController					(DAQLabModule_type* mod, TaskControl_type* taskController);

	// Removes a list of Task Controllers from the DAQLab framework.
	// tcList of TaskControl_type*   
BOOL				DLRemoveTaskControllers 			(DAQLabModule_type* mod, ListType tcList);
	// Removes a single Task Controller from the DAQLab framework.
BOOL				DLRemoveTaskController 				(DAQLabModule_type* mod, TaskControl_type* taskController);	

//-------------------------------------------------------------------------------
// DAQLab common thread pool management
//-------------------------------------------------------------------------------

CmtThreadPoolHandle	DLGetCommonThreadPoolHndl			(void);

//-------------------------------------------------------------------------------
// DAQLab XML management
//-------------------------------------------------------------------------------
	// Adds multiple XML Element children or attributes for a parent XML Element
int					DLAddToXMLElem						(CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, DAQLabXMLNode childXMLNodes[], DAQLabXMLNodeTypes nodeType, size_t nNodes, ERRORINFO* xmlErrorInfo);

	// Places the value of multiple XML Attributes of an Element into user provided pointers
int					DLGetXMLElementAttributes			(char XMLElementName[], ActiveXMLObj_IXMLDOMElement_ XMLElement, DAQLabXMLNode Attributes[], size_t nAttributes);

int					DLGetSingleXMLElementFromElement	(ActiveXMLObj_IXMLDOMElement_ parentXMLElement, char elementName[], ActiveXMLObj_IXMLDOMElement_* childXMLElement);

	// Saves task controller settings to XML and returns a "TaskControllerSettings" XML element
int 				DLSaveTaskControllerSettingsToXML 	(TaskControl_type* taskController, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ taskControllerXMLElement, ERRORINFO* xmlErrorInfo);
	// Loads task controller settings from a "TaskControllerSettings" XML element
int					DLLoadTaskControllerSettingsFromXML	(TaskControl_type* taskController, ActiveXMLObj_IXMLDOMElement_ taskControllerSettingsXMLElement, ERRORINFO* xmlErrorInfo);

int					DLSaveToXMLPopup					(CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ rootElement, ERRORINFO* xmlErrorInfo);

	// Displays a file selection popup and loads its root XML element. If rootElementName is different than NULL or "" then it loads the root element only if the name matches.
int					DLLoadFromXMLPopup					(CAObjHandle* xmlDOMPtr, ActiveXMLObj_IXMLDOMElement_* rootElementPtr, char rootElementName[], ERRORINFO* xmlErrorInfo);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DAQLab_H__ */
