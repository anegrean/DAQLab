//==============================================================================
//
// Title:		DAQLab.c
// Purpose:		A short description of the implementation.
//
// Created on:	8-3-2014 at 13:48:00 by Adrian Negrean.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include "DAQLab.h"			// include this first 
#include <formatio.h>
#include <userint.h>
#include <toolbox.h>
#include "UI_DAQLab.h"
#include "DAQLabModule.h"

 


//==============================================================================
// Constants
	
	// DAQLab main workspace UI resource
#define DAQLAB_UI_DAQLAB			"UI_DAQLab.uir"
	// name of config file
#define DAQLAB_CFG_FILE				"DAQLabCfg.xml"

#define DAQLAB_CFG_DOM_ROOT_NAME	"DAQLabConfig"
	// log panel displayed at startup and displays received messages
#define DAQLAB_LOG_PANEL			TRUE


//==============================================================================
// Types

//---------------------------------------------------------------------------------
	// Predefined DAQLab errors and warnings to be used with DAQLab_Msg function.
	//
	// DAQLabMsg is a function with variable arguments and types which depend on
	// the particular warning or error message. When calling this function with a 
	// certain error code from below, pass in the additional parameters with 
	// required data type.
	//
typedef enum _DAQLabMessageID{
	
	// errors
	DAQLAB_MSG_ERR_DOM_CREATION,
	DAQLAB_MSG_ERR_ACTIVEXML,
	
	// warnings
	DAQLAB_MSG_WARN_NO_CFG_FILE,
	
	// success
	DAQLAB_MSG_OK_DOM_CREATION
	
	
		
} DAQLabMessageID;
//----------------------------------------------------------------------------------

//==============================================================================
// Static global variables

	// DAQLab XML ActiveX server controller handle for DOM XML management
CAObjHandle							DAQLabCfg_DOMHndl		= 0;
ActiveXMLObj_IXMLDOMElement_   		DAQLabCfg_RootElement 	= 0; 

	// handle of main workspace panel
int				mainPanHndl			= 0;
	// handle of log panel
int				logPanHndl			= 0;

	// different modules that inherit from DAQLabModule_type 
	// list elements of DAQLabModule_type* size
ListType		modules;		
							

//==============================================================================
// Static functions

static int		DAQLab_Load 			(void);

static int 		DAQLab_Close 			(void);

static void 	DAQLab_Msg 				(DAQLabMessageID msgID, void* data1, void* data2, void* data3);

static int		DAQLab_NewXMLDOM	   	(const char fileName[], CAObjHandle* xmlDOM, ActiveXMLObj_IXMLDOMElement_* rootElement); 

static int		DAQLab_SaveXMLDOM		(const char fileName[], CAObjHandle* xmlDOM);

 
//==============================================================================
// Global variables

//==============================================================================
// Global functions

/// HIFN  Main entry point
/// HIRET Return indicates if function was successful.
/// HIRET 0 success
/// HIRET -1 fail
int main (int argc, char *argv[])
{
	int error = 0;
	
	// start CVI Run-Time Engine
	if (!InitCVIRTE (0, argv, 0)) return -1;
	
	// load DAQLab environment resources
	//errChk ( )
	DAQLab_Load(); 
	
	


	// run GUI
	errChk ( RunUserInterface() ); 
	
	return 0;
	
	Error:
	
	return error;
}


/// HIFN  Loads DAQLab environment
/// HIRET Return indicates if function was successful.
/// HIRET 0 if success
/// HIRET negative value if fail
static int DAQLab_Load (void)
{
	int									error 			= 0;
	
	// load main panel resources
	errChk ( mainPanHndl = LoadPanel (0, DAQLAB_UI_DAQLAB, MainPan) );
	if (DAQLAB_LOG_PANEL)
		errChk ( logPanHndl = LoadPanel (mainPanHndl, "UI_DAQLab.uir", LogPan) );
	// maximize panel by default
	SetPanelAttribute(mainPanHndl, ATTR_WINDOW_ZOOM, VAL_MAXIMIZE);
	
	DisplayPanel(mainPanHndl);
	
	if (DAQLAB_LOG_PANEL)
		DisplayPanel(logPanHndl);
	
	// init modules list
	modules = DAQLabModule_populate();
	
	// init DAQLab DOM
	errChk ( DAQLab_NewXMLDOM(DAQLAB_CFG_FILE, &DAQLabCfg_DOMHndl, &DAQLabCfg_RootElement) );
	
	// add multiple XML elements to root element
	
	int 		dog_val = -1;
	float		cat_val = 4.0/3;
	DAQLabXMLNode children[] = { {"dog", DL_INT, &dog_val},
								 {"cat", DL_FLOAT, &cat_val}
															 };  
	
	DLAddXMLElem (DAQLabCfg_DOMHndl, DAQLabCfg_RootElement, children, NumElem(children));
	

	
	
	errChk ( DAQLab_SaveXMLDOM(DAQLAB_CFG_FILE, &DAQLabCfg_DOMHndl) ); 
	
	
	// TEST: load first module
	DAQLabModule_type** daqlabmodptr = ListGetPtrToItem(modules, 1);
	(*(*daqlabmodptr)->Load)(*daqlabmodptr, mainPanHndl);

	
	
	
	
	return 0;
	
	Error:
	
	return error;
	
}

static int DAQLab_Close (void)
{   
	// save module specific info
	
	
	// dispose modules list
	DAQLabModule_empty (&modules);
	
	QuitUserInterface(0);  
	
	return 0;
}


int CVICALLBACK CB_DAQLab_MainPan (int panel, int event, void *callbackData, int eventData1, int eventData2)
{
	if (event == EVENT_CLOSE) DAQLab_Close ();
	
	return 0;
}


/// HIFN Installs callbackData and callbackFn to controls directly within panel.
/// HIPAR callbackData/ Callback data received by all controls in the panel. Set NULL if not used.
/// HIPAR callbackFn/ Callback function received by all controls in the panel. Set NULL if not used.
/// HIPAR panHndl/ Panel handle.
/// HIRET 0 if successful, negative number otherwise.
int	SetCtrlsInPanCBInfo (void* callbackData, CtrlCallbackPtr callbackFn, int panHndl)
{
	int 	error 					= 0;
	int		ctrlID;
	
	GetPanelAttribute(panHndl, ATTR_PANEL_FIRST_CTRL, &ctrlID);
	while (ctrlID) {
		// install callback data
		errChk( SetCtrlAttribute(panHndl, ctrlID, ATTR_CALLBACK_DATA, callbackData) );
		// install callback function
		errChk( SetCtrlAttribute(panHndl, ctrlID, ATTR_CALLBACK_FUNCTION_POINTER, callbackFn) );
		// get next control in the panel
		GetCtrlAttribute(panHndl, ctrlID, ATTR_NEXT_CTRL, &ctrlID);
	}
	
	Error:
	return error;
}

void DLMsg(const char* text, BOOL beep)
{
	if (!DAQLAB_LOG_PANEL) return;
	
	SetCtrlVal(logPanHndl, LogPan_LogBox, text);
	if (beep) Beep();
	
}

/// HIFN Displays predefined error and warning messages in the main workspace log panel.
/// HIPAR msgID/ If required, pass in additional data to format the message.
static void DAQLab_Msg (DAQLabMessageID msgID, void* data1, void* data2, void* data3)
{
	char buff[1000]="";
	char buffCA[1000]="";
	
	switch (msgID) {
		
		case DAQLAB_MSG_WARN_NO_CFG_FILE:
			
			DLMsg("Warning: Loading DAQLab config file failed. Default config used instead.\nReason: ", 1);   
			CA_GetAutomationErrorString(*(HRESULT*)data1, buffCA, sizeof(buffCA));
			DLMsg(buffCA, 0);
			DLMsg("\n", 0);
			// additional error info
			if (*(HRESULT*)data2 == DISP_E_EXCEPTION) {
				CA_DisplayErrorInfo(*(CAObjHandle*)data1, NULL, *(HRESULT*)data2, (const ERRORINFO*)data3); 
				DLMsg("\n\n", 0);
			} else
				DLMsg("\n", 0);
			
			break;
			
		case DAQLAB_MSG_ERR_DOM_CREATION:
			
			Fmt(buff, "Error: Could not create DAQLab config file \"%s\" XMLDOM. Quit software and resolve issue. \nReason: ", DAQLAB_CFG_FILE);
			CA_GetAutomationErrorString(*(HRESULT*)data1, buffCA, sizeof(buffCA));
			DLMsg(buff, 1);
			DLMsg(buffCA, 0);
			DLMsg("\n\n", 0);
			
			break;
			
		case DAQLAB_MSG_ERR_ACTIVEXML:
			
			CA_GetAutomationErrorString(*(HRESULT*)data2, buffCA, sizeof(buffCA));
			DLMsg(buffCA, 1);
			DLMsg("\n", 0);
			// additional error info
			if (*(HRESULT*)data2 == DISP_E_EXCEPTION) {
				CA_DisplayErrorInfo(*(CAObjHandle*)data1, NULL, *(HRESULT*)data2, (const ERRORINFO*)data3); 
				DLMsg("\n\n", 0);
			} else
				DLMsg("\n", 0);
			
			
			break;
			
		case DAQLAB_MSG_OK_DOM_CREATION:
			
			DLMsg("XML DOM created successfully.\n\n", 0);
			
			break;
			
	}
	
	
}


/// HIFN Creates an MSXML DOM object using ActiveX connection to Microsoft XML 6.0 from a specified .xml file.
/// HIFN If no .xml file is found, an empty DOM is created.
/// HIPAR fileName/ String of XML file name, including .xml file extension.
/// HIRET New DOM object handle from loaded .xml file or an empty DOM if file not found or not specified.
/// HIRET New Topmost (parent) element handle of the DOM to which other elements and attributes can be added.   
/// OUT xmlDOM 
/// OUT rootElement 
static int DAQLab_NewXMLDOM (const char fileName[], CAObjHandle* xmlDOM, ActiveXMLObj_IXMLDOMElement_* rootElement)
{
	HRESULT								xmlerror;
	ERRORINFO							xmlERRINFO;
	VBOOL								xmlLoaded;
	BOOL								rootElementNameOK		= FALSE;
	ActiveXMLObj_IXMLDOMElement_		newElement;
	BSTR								bstrFileName;
	double 								t1, t2;
	char								timeStr[10];
	char								msg[100];
	char*								rootElementName;
	
	
	// create new ActiveX DOM object
	if ( (xmlerror = ActiveXML_NewDOMDocument60IXMLDOMDocument3_ (NULL, 1, LOCALE_NEUTRAL, 0, xmlDOM)) < 0 ) {
			DAQLab_Msg(DAQLAB_MSG_ERR_DOM_CREATION, &xmlerror, 0, 0);
			return xmlerror;
		} else 
			DAQLab_Msg(DAQLAB_MSG_OK_DOM_CREATION, 0, 0, 0);
	
	// load DAQLab XML config file to DOM or create new DOM if file not found
	DLMsg("Loading XML file ", 0);
	DLMsg(fileName, 0);
	DLMsg(" ...", 0);
	
	t1 = Timer();     
	
	CA_CStringToBSTR(fileName, &bstrFileName); 
	
	// load XML file
	xmlerror = ActiveXML_IXMLDOMDocument3_load (*xmlDOM, &xmlERRINFO, CA_VariantBSTR(bstrFileName), &xmlLoaded);
	t2 = Timer();
	
	if (xmlLoaded == VTRUE)	{
		DLMsg("loaded in ", 0);
		Fmt(timeStr, "%s<%f[p2] s.\n\n", t2 - t1);
		DLMsg(timeStr, 0);
	} else
		DAQLab_Msg(DAQLAB_MSG_WARN_NO_CFG_FILE, &xmlerror, &xmlERRINFO, 0);
	
	// if file was loaded check root element name if it matches the name defined in DAQLAB_CFG_DOM_ROOT_NAME 
	if (xmlLoaded == VARIANT_TRUE) {
		// get document root element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_GetdocumentElement (*xmlDOM, &xmlERRINFO, rootElement) );
		
		// check if root element name is correct
		XMLErrChk ( ActiveXML_IXMLDOMElement_GettagName(*rootElement, &xmlERRINFO, &rootElementName) );
		
		if(strcmp(rootElementName, DAQLAB_CFG_DOM_ROOT_NAME)) {
			rootElementNameOK = FALSE;
			Fmt(msg, "Root element %s was not found. Empty DOM will be used instead.\n\n", DAQLAB_CFG_DOM_ROOT_NAME);
			DLMsg(msg, 1);
		} else
			rootElementNameOK = TRUE;
	} 
	
	if (xmlLoaded == VARIANT_FALSE || !rootElementNameOK) {
		// create root element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (*xmlDOM, &xmlERRINFO, DAQLAB_CFG_DOM_ROOT_NAME, &newElement) );
	
		// append root element to DOM and return handle to root element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_appendChild(*xmlDOM, &xmlERRINFO, newElement, rootElement) );
	} 
	
	return 0;
	
	XMLError:
	
	DAQLab_Msg(DAQLAB_MSG_ERR_ACTIVEXML, xmlDOM, &xmlerror, &xmlERRINFO);
	
	return xmlerror;
	
}

static int	DAQLab_SaveXMLDOM (const char fileName[], CAObjHandle* xmlDOM)
{
	HRESULT				xmlerror;
	ERRORINFO			xmlERRINFO;
	BSTR				bstrFileName;
	double				t1, t2;
	char				timeStr[10]; 
	
	// save XML file
	DLMsg("Saving XML file ", 0);
	DLMsg(fileName, 0);
	DLMsg("... ", 0);
	
	t1 = Timer();
	CA_CStringToBSTR(fileName, &bstrFileName); 
	// write file
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_save(*xmlDOM, &xmlERRINFO, CA_VariantBSTR(bstrFileName)) );
	
	t2 = Timer();
	
	DLMsg("saved in ", 0);
	Fmt(timeStr, "%s<%f[p2] s.\n\n", t2 - t1);
	DLMsg(timeStr, 0);
	
	
	return 0;
	
	XMLError:
	
	DAQLab_Msg(DAQLAB_MSG_ERR_ACTIVEXML, xmlDOM, &xmlerror, &xmlERRINFO);
	
	return xmlerror;
}

int	DLAddXMLElem (CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, DAQLabXMLNode childXMLElements[], size_t nElements)
{
	HRESULT							xmlerror;
	ERRORINFO						xmlERRINFO;
	ActiveXMLObj_IXMLDOMElement_	newXMLElement;
	VARIANT							xmlVal;
	BSTR							bstrVal;
	
	for (int i = 0; i < nElements; i++) {
		
		// create new element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, childXMLElements[i].tag, &newXMLElement) );
		
		// convert to variant
		switch (childXMLElements[i].type) {
			
			case DL_NULL:
				
				xmlVal = CA_VariantNULL ();
				break;
			
			case DL_BOOL:
				
				xmlVal = CA_VariantBool ((VBOOL)*(BOOL*)childXMLElements[i].pData);
				break;
				
			case DL_CHAR:
				
				xmlVal = CA_VariantChar(*(char*)childXMLElements[i].pData);
				break;
				
			case DL_UCHAR:
				
				xmlVal = CA_VariantUChar(*(unsigned char*)childXMLElements[i].pData);
				break;
				
			case DL_SHORT:
				
				xmlVal = CA_VariantShort(*(short*)childXMLElements[i].pData);
				break;
				
			case DL_USHORT:
				
				xmlVal = CA_VariantUShort(*(unsigned short*)childXMLElements[i].pData);
				break;
			
			case DL_INT:
				
				xmlVal = CA_VariantInt(*(int*)childXMLElements[i].pData);
				break;
				
			case DL_UINT:
				
				xmlVal = CA_VariantUInt(*(unsigned int*)childXMLElements[i].pData);
				break;
				
			case DL_LONG:
				
				xmlVal = CA_VariantLong(*(long*)childXMLElements[i].pData);
				break;
			
			case DL_ULONG:
				
				xmlVal = CA_VariantULong(*(unsigned long*)childXMLElements[i].pData);
				break;
				
			case DL_LONGLONG:
				
				xmlVal = CA_VariantLongLong(*(long long*)childXMLElements[i].pData);
				break;
				
			case DL_ULONGLONG:
				
				xmlVal = CA_VariantULongLong(*(unsigned long long*)childXMLElements[i].pData);
				break;
			
			case DL_FLOAT:
				
				xmlVal = CA_VariantFloat(*(float*)childXMLElements[i].pData);
				break;
			
			case DL_DOUBLE:
				
				xmlVal = CA_VariantDouble(*(double*)childXMLElements[i].pData);
				break;
				
			case DL_CSTRING:
				
				CA_CStringToBSTR((char*)childXMLElements[i].pData, &bstrVal); 
				xmlVal = CA_VariantBSTR(bstrVal);
				break;
		}
		
		// set element typed value
		XMLErrChk ( ActiveXML_IXMLDOMElement_SetnodeTypedValue(newXMLElement, &xmlERRINFO, xmlVal) );
		
		// add new element as child
		XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (parentXMLElement, &xmlERRINFO, newXMLElement, NULL) );
	}
	
	
	return 0;
	
	XMLError:
	
	DAQLab_Msg(DAQLAB_MSG_ERR_ACTIVEXML, &xmlDOM, &xmlerror, &xmlERRINFO);
	
	return xmlerror;
	
	return 0;
}
/*
	// create root element node
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (DOMHndl, &xmlERRINFO, "DAQLab", &DAQLab_root) );
	
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_createNode(DOMHndl, &xmlERRINFO, CA_VariantInt(NODE_ELEMENT), "root", "", &DAQLab_rootNode) ); 
	
	// create attribute node
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_createAttribute (DOMHndl, &xmlERRINFO, "Size", &attrNode) );
	
	// add double value to attribute node
	XMLErrChk ( ActiveXML_IXMLDOMAttribute_Setvalue (attrNode, &xmlERRINFO, CA_VariantDouble(3.141)) );
	
	
	// set element attribute
	XMLErrChk ( ActiveXML_IXMLDOMElement_setAttribute(DAQLab_rootNode, &xmlERRINFO, "Size", CA_VariantDouble(3.141)) ); 
	
		/*   this works */
	/*
	t1 = Timer();
	
	// create root element by loading XML syntacs directly to DOM
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_loadXML (DOMHndl, &xmlERRINFO, "<root></root>", &XMLrootCreated) );
	
	// get document root element
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_GetdocumentElement (DOMHndl, &xmlERRINFO, &DAQLab_rootElement) );
	
	// add attribute to root element
	XMLErrChk ( ActiveXML_IXMLDOMElement_setAttribute(DAQLab_rootElement, &xmlERRINFO, "Size", CA_VariantDouble(3.141)) ); 
	/*	*/
			/*
	t2 = Timer();
		
	diff = (t2 - t1) * 1000; // in ms
	
	/
	// create root element
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (DOMHndl, &xmlERRINFO, "DAQLab", &DAQLab_rootElement) );
	
	// append root element to DOM
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_appendChild(DOMHndl, &xmlERRINFO, DAQLab_rootElement, &DAQLab_rootNode) )
	
	*/
