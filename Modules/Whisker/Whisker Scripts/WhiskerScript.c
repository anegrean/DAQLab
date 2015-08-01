//==============================================================================
//
// Title:		WhiskerScript.c
// Purpose:		A short description of the implementation.
//
// Created on:	25-7-2015 at 20:33:50 by Vinod Nigade.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include "DAQLab.h"
#include "Whisker.h"
#include "WhiskerScript.h"
#include "UI_Scripts.h"
#include "CVIXML.h"

//==============================================================================
// Constants
#define XMLErrChk(f) 		if (error = (f), FAILED (error)) goto Error; else
#define TOT_IO_CH			32
#define ERR_MSG_SIZE		128

#define	XML_ELEMENT_VALUE		9			/* 8 Number of digits in the value + '\0'*/
#define MAX_CHILD_ELEMENTS		5			/* Condition Element has maximum childrens */
#define START_ELEMENT_CHILDS	2
#define ACTION_ELEMENT_CHILDS	4
#define COND_ELEMENT_CHILDS		5
#define REPEAT_ELEMENT_CHILDS	3
#define STOP_ELEMENT_CHILDS		2
#define WAIT_ELEMENT_CHILDS		2

#define ATTR_ID				"ID"
#define XML_ROOT_ELEMENT	"Script"
#define START_ELEMENT_TAG	"Start"
#define ACTION_ELEMENT_TAG	"Action"
#define COND_ELEMENT_TAG	"Condition"
#define REPEAT_ELEMENT_TAG	"Repeat"
#define STOP_ELEMENT_TAG	"Stop"
#define WAIT_ELEMENT_TAG	"Wait"

#define SEQ_NO_TAG			"Seq_No"
#define DELAY_TAG			"Delay"
#define SUB_ACTION_TAG		"Sub_Action"
#define DURATION_TAG		"Duration"
#define IO_CH_TAG			"IO_Channel"
#define VALUE_TAG			"Value"
#define TRUE_TAG			"True"
#define FALSE_TAG			"False"
#define NTIMES_TAG			"NTimes"
#define STEP_TAG			"Step"

#define LOG_MSG_LEN			256
#define LOG_TITLE			"ID\t\tType\t\tParams\n"
#define LOG_HYPHEN			"-------\t\t-----------------\t\t------------------"

const char	*Action_String[] = {
			"AIRPUFF",
			"DROPOUT",
			"DROPIN" };

//==============================================================================
// Types

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables
WhiskerScript_t *whisker_script = NULL;
//==============================================================================
// Global functions
inline void setscript_ctrl_attribute(WhiskerScript_t *whisker_script);
static int add_XMLElement_toRoot(CVIXMLElement *par_element, char *tag[], char value[][XML_ELEMENT_VALUE], int index);
static int get_XMLElement_childvalues(CVIXMLElement *xml_element, int *value, int num_child);

void start_runner(void *element, void *whisker_script, int *index);
void action_runner(void *element, void *whisker_script, int *index);
void condition_runner(void *element, void *whisker_script, int *index);
void repeat_runner(void *element, void *whisker_script, int *index);
void stop_runner(void *element, void *whisker_script, int *index);
void wait_runner(void *element, void *whisker_script, int *index);
inline void save_and_print_LogMsg(WhiskerScript_t *whisker_script, char	*msg);

static void DisplayActiveXErrorMessageOnFailure(HRESULT error);

/// HIFN  What does your function do?
/// HIPAR x/What inputs does your function expect?
/// HIRET What does your function return?

/* Init and Display Script's UI Panel */

int
init_display_script(void *function_data)
{
	//Whisker_t	*whisker_m = (Whisker_t *)function_data;
	int		error = 0;
	char	IO_channel[7] = { 0 };
	
	if (whisker_script != NULL) {
		LOG_MSG(9, "whisker_script is not NULL");
		return -1;
	}
	
	/* Allocate Memory for the script structure */
	whisker_script = (WhiskerScript_t *) malloc(sizeof(WhiskerScript_t));
	if (whisker_script == NULL) {
		LOG_MSG(9, "NO Memory for script structure");
		return -1;
	}
	
	whisker_script->whisker_m = function_data;
	whisker_script->cur_script.script_elements = NULL;
	whisker_script->cur_script.num_elements = 0;
	whisker_script->cur_script.xml_script.VALID_FILE = FALSE;
	whisker_script->cur_script.log_file.VALID_FILE = FALSE;
	whisker_script->cur_script.run_status = STOPPED;
	CmtNewLock (NULL, 0, &(whisker_script->cur_script.lock));
	
	//whisker_m->whisker_script = whisker_script;
	
	/* Load Panel */
	errChk(whisker_script->main_panel_handle = LoadPanel(0, MOD_WhiskerScript_UI, ScriptPan));
	errChk(whisker_script->container_panel_handle = LoadPanel(whisker_script->main_panel_handle, 
														MOD_WhiskerScript_UI, ContainPan));
	/* Load Script Element Panel */
	errChk(whisker_script->startElement_panel_handle = LoadPanel(whisker_script->container_panel_handle, 
										  				MOD_WhiskerScript_UI, StartEle));
	errChk(whisker_script->actionElement_panel_handle = LoadPanel(whisker_script->container_panel_handle, 
										  				MOD_WhiskerScript_UI, ActionEle));
	errChk(whisker_script->condElement_panel_handle = LoadPanel(whisker_script->container_panel_handle, 
										  				MOD_WhiskerScript_UI, CondEle));
	errChk(whisker_script->repeatElement_panel_handle = LoadPanel(whisker_script->container_panel_handle, 
										  				MOD_WhiskerScript_UI, RepEle));
	errChk(whisker_script->stopElement_panel_handle = LoadPanel(whisker_script->container_panel_handle, 
										  				MOD_WhiskerScript_UI, StopEle));
	errChk(whisker_script->waitElement_panel_handle = LoadPanel(whisker_script->container_panel_handle, 
										  			MOD_WhiskerScript_UI, WaitEle));
	
	/* Set Ctrl attibute */
	setscript_ctrl_attribute(whisker_script);
	
	/* Set I/O Channels */
	InsertListItem(whisker_script->actionElement_panel_handle, ActionEle_EleIO_CH, 0,
					   "Default", 0);
	InsertListItem(whisker_script->condElement_panel_handle, CondEle_EleIO_CH, 0,
					   "Default", 0);
	for (int i = 0; i < TOT_IO_CH; i++) {
		sprintf(IO_channel, "I/O %d", i);
		InsertListItem(whisker_script->actionElement_panel_handle, ActionEle_EleIO_CH, i+1,
					   IO_channel, i+1);
		InsertListItem(whisker_script->condElement_panel_handle, CondEle_EleIO_CH, i+1,
					   IO_channel, i+1);
	}
	
	/* Adjust Container Panel Position 
	 * XXX : Hardcoded Value.
	 */
	SetPanelAttribute(whisker_script->container_panel_handle, ATTR_TOP, 95);
	SetPanelAttribute(whisker_script->container_panel_handle, ATTR_LEFT, 15);
	
	/* Display Panel */
	DisplayPanel(whisker_script->main_panel_handle);
	DisplayPanel(whisker_script->container_panel_handle);
	
Error:
	return error;
}

/* Discard panels, free elements and free list */
int
discard_cur_script(WScript_t *cur_script) 
{
	ScriptElement_t	*element = NULL;
	
	if (cur_script->script_elements == NULL) {
		LOG_MSG(9, "Script Elements list is NULL");
		return -1;	
	}
	
	while (ListNumItems(cur_script->script_elements)) {
		ListRemoveItem(cur_script->script_elements, &element, FRONT_OF_LIST);
		
		/* Discard Panel */
		DiscardPanel(element->panel_handle);
		
		/* Free element */
		OKfree(element);
	}
	
	/* Free list */
	OKfreeList(cur_script->script_elements);
	cur_script->num_elements = 0;
	cur_script->xml_script.VALID_FILE = FALSE;
	cur_script->log_file.VALID_FILE = FALSE;
	return 0;
}

/* Discard script module structure */
int
discard_script_module()
{
	/* Discard Panels.
	 * When discard parent panel, all its childrens are discarded.
	 */
	DiscardPanel(whisker_script->main_panel_handle);
	CmtDiscardLock(whisker_script->cur_script.lock);
	OKfree(whisker_script);
	return 0;
}

inline void 
setscript_ctrl_attribute(WhiskerScript_t *whisker_script)
{
	/* Script Main Panel level control attributes */
	SetCtrlAttribute (whisker_script->main_panel_handle, ScriptPan_ScriptNew, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->main_panel_handle, ScriptPan_ScriptAdd, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->main_panel_handle, ScriptPan_ScriptQuit, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->main_panel_handle, ScriptPan_ScriptSave, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->main_panel_handle, ScriptPan_ScriptLoad, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->main_panel_handle, ScriptPan_ScriptRun, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->main_panel_handle, ScriptPan_ScriptPause, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->main_panel_handle, ScriptPan_ScriptStop, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	
	/* Add callback data to script elements panel */
	SetCtrlAttribute (whisker_script->startElement_panel_handle, StartEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->startElement_panel_handle, StartEle_EleApply, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->actionElement_panel_handle, ActionEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->actionElement_panel_handle, ActionEle_EleApply, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->condElement_panel_handle, CondEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->condElement_panel_handle, CondEle_EleApply, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->repeatElement_panel_handle, RepEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->repeatElement_panel_handle, RepEle_EleApply, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->stopElement_panel_handle, StopEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->stopElement_panel_handle, StopEle_EleApply, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->waitElement_panel_handle, WaitEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->waitElement_panel_handle, WaitEle_EleApply, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	return;
}

/* Draw Script Elements.
 */
void
redraw_script_elements(WScript_t *cur_script)
{
	size_t			num_elements = cur_script->num_elements;
	size_t			height_element_panel = 0;
	size_t			cur_panel_position = INTER_ELEMENT_SPACING;
	char			seq_num[3];				/* Two digit sequence number */
	ScriptElement_t	*element = NULL;
	
	for (int i = 1; i <= num_elements; i++) {
		/* Get Element one by one */
		ListGetItem(cur_script->script_elements, &element, i);
		
		/* Set position of element */
		SetPanelAttribute(element->panel_handle, ATTR_TOP, cur_panel_position);
		
		/* Set Set number into panel control elenum 
		 */
		sprintf(seq_num, "%d", i);
		if (element->MAGIC_NUM == START) {
			SetCtrlVal(element->panel_handle, StartEle_EleNum, seq_num);
		} else if (element->MAGIC_NUM == ACTION) {
			SetCtrlVal(element->panel_handle, ActionEle_EleNum, seq_num);	
		} else if (element->MAGIC_NUM == CONDITION) {
			SetCtrlVal(element->panel_handle, CondEle_EleNum, seq_num);	
		} else if (element->MAGIC_NUM == REPEAT) {
			SetCtrlVal(element->panel_handle, RepEle_EleNum, seq_num);	
		} else if (element->MAGIC_NUM == STOP) {
			SetCtrlVal(element->panel_handle, StopEle_EleNum, seq_num);	
		} else if (element->MAGIC_NUM == WAIT) {
			SetCtrlVal(element->panel_handle, WaitEle_EleNum, seq_num);	
		}
						 
		GetPanelAttribute(element->panel_handle, ATTR_HEIGHT, &height_element_panel);
		cur_panel_position += height_element_panel + INTER_ELEMENT_SPACING;   /* TODO */
		DisplayPanel(element->panel_handle);
	}
	
	return;
}


/* Save current script into XML document */
void
save_script(WhiskerScript_t	*whisker_script)
{
	WScript_t		*cur_script = &(whisker_script->cur_script);
	ScriptElement_t	*element = NULL;
	int				ret = 0;
	CVIXMLDocument	script_doc;
	CVIXMLElement	root_element = NULL;
	HRESULT         error = S_OK;
	char			*tag[MAX_CHILD_ELEMENTS + 1 + 1];	/* Childs + Parent + NULL */
	char			value[MAX_CHILD_ELEMENTS + 1][XML_ELEMENT_VALUE];
	
	
	if (cur_script->num_elements < 1) {	/* No elements in the script */
		MessagePopup("Save XML", "Script Should have atleast one element!");
		return;
	}
	
	XMLErrChk(CVIXMLNewDocument(XML_ROOT_ELEMENT, &script_doc));
	XMLErrChk(CVIXMLGetRootElement(script_doc, &root_element));
	
	/* Add every Script Element to XML element */
	for (int i = 1; i <= cur_script->num_elements; i++) {
		ListGetItem(cur_script->script_elements, &element, i);
		
		switch (element->MAGIC_NUM) {
			case START:		/* Add start element */
				/* Set tag */
				tag[0] = START_ELEMENT_TAG; tag[1] = SEQ_NO_TAG;
				tag[2] = DELAY_TAG;			tag[3] = NULL;
				
				/* Set value . Tag and index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				sprintf(value[2], "%d", ((StartElement_t *)element)->delay);
			
				break;
				
			case ACTION: 	/* Add action element */
				/* Set tag */
				tag[0] = ACTION_ELEMENT_TAG;	tag[1] = SEQ_NO_TAG;
				tag[2] = SUB_ACTION_TAG;		tag[3] = DURATION_TAG;
				tag[4] = IO_CH_TAG;				tag[5] = NULL;
				
				/* Set value . Tag and index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				sprintf(value[2], "%d", ((ActionElement_t *)element)->action);
				sprintf(value[3], "%d", ((ActionElement_t *)element)->duration);
				sprintf(value[4], "%d", ((ActionElement_t *)element)->IO_Channel);
				
				break;
				
			case CONDITION: /* Add condition element */
				/* Set tag */
				tag[0] = COND_ELEMENT_TAG;	tag[1] = SEQ_NO_TAG;
				tag[2] = IO_CH_TAG;			tag[3] = VALUE_TAG;
				tag[4] = TRUE_TAG;			tag[5] = FALSE_TAG;
				tag[6] = NULL;
				
				/* Set value . Tag and index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				sprintf(value[2], "%d", ((ConditionElement_t *)element)->IO_channel);
				sprintf(value[3], "%d", ((ConditionElement_t *)element)->value);
				sprintf(value[4], "%d", ((ConditionElement_t *)element)->true_step);
				sprintf(value[5], "%d", ((ConditionElement_t *)element)->false_step);
		
				break;
				
			case REPEAT:	/* Add repeat element */
				/* Set tag */
				tag[0] = REPEAT_ELEMENT_TAG;	tag[1] = SEQ_NO_TAG;
				tag[2] = NTIMES_TAG;			tag[3] = STEP_TAG;
				tag[4] = NULL;
				
				/* Set value . Tag and index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				sprintf(value[2], "%d", ((RepeatElement_t *)element)->ntimes);
				sprintf(value[3], "%d", ((RepeatElement_t *)element)->repeat_step);

				break;
				
			case STOP:		/* Add stop element */
				/* Set tag */
				tag[0] = STOP_ELEMENT_TAG;	tag[1] = SEQ_NO_TAG;
				tag[2] = DELAY_TAG;			tag[3] = NULL;
				
				/* Set value . Tag and index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				sprintf(value[2], "%d", ((StopElement_t *)element)->delay);
			
				break;
				
			case WAIT:		/* Add Wait element */
				/* Set tag */
				tag[0] = WAIT_ELEMENT_TAG;	tag[1] = SEQ_NO_TAG;
				tag[2] = DELAY_TAG;			tag[3] = NULL;
				
				/* Set value . Tag and index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				sprintf(value[2], "%d", ((WaitElement_t *)element)->delay);
				
				break;
		}
		
		/* Add tag and value to script element */
		add_XMLElement_toRoot(&root_element, tag, value, i-1);
	}
	
	/* Get File path */
	if (!cur_script->xml_script.VALID_FILE) {	/* FilePop to get file path */
		ret = FileSelectPopup ("", "*.xml", "", "Save to XML",
                                VAL_SAVE_BUTTON, 0, 1, 1, 0, cur_script->xml_script.file_path);
        if (!ret) {
			goto Error;
		}
        /* TODO: Set file path to UI display */
		cur_script->xml_script.VALID_FILE = TRUE;
	} else {
		/* Delete file and create new file path */
		/* This is because, loaded file can be modified, like delete
		 * Existing element, add existing element, change existing element.
		 * This involves modifying XML document during above operations when
		 * script_elements are loaded from XML document.Thse XML documents
		 * has to be handled explicitely during script_element operation
		 * Thus, we avoid that by deleting existing file and writing this
		 * document to fresh file
		 */
	}
	
	/* Save XML document to file */
	XMLErrChk(CVIXMLSaveDocument(script_doc, 1, cur_script->xml_script.file_path));
	
Error:
	CVIXMLDiscardElement(root_element);
	CVIXMLDiscardDocument(script_doc);
	DisplayActiveXErrorMessageOnFailure(error);
	return;
}

/* Load Existing Scripts */
void
load_script(WhiskerScript_t	*whisker_script)
{
	WScript_t		*cur_script = &(whisker_script->cur_script);
	ScriptElement_t	*element = NULL;
	int				ret = 0;
	CVIXMLDocument	script_doc;
	CVIXMLElement	root_element = NULL;
	CVIXMLElement	xml_element = NULL;
	CVIXMLAttribute xmlelement_type = NULL;
	char			attr_value[XML_ELEMENT_VALUE];
	ElementType_t	MAGIC_NUM;
	HRESULT         error = S_OK;
	int				num_XMLelements = 0;
	int				value[MAX_CHILD_ELEMENTS];
	
	if (cur_script->script_elements != NULL) {
		ret = ConfirmPopup ("Load Script", "Discard existing script?");
		if (ret == 0) {		/* Cancelled Operation */
			return;	
		} 
		discard_cur_script(cur_script);	
	}
	
	/* Get the file path */
	ret = FileSelectPopup("", "*.xml", "", "Select a Script File",
                                   VAL_LOAD_BUTTON, 0, 1, 1, 0, cur_script->xml_script.file_path);
    if (!ret) {
		return;
	}
    /* TODO: Set file path to UI display */
	cur_script->xml_script.VALID_FILE = TRUE;
	
	/* Load XML Document and root element */
	XMLErrChk(CVIXMLLoadDocument (cur_script->xml_script.file_path, &script_doc));
    XMLErrChk(CVIXMLGetRootElement(script_doc, &root_element));
	XMLErrChk(CVIXMLGetNumChildElements(root_element, &num_XMLelements));
	
	if (num_XMLelements < 1) {
		MessagePopup("Load XML", "Script Should have atleast one element!");
		goto Error;	
	}
	
	/* Create ScriptElement list */
	nullChk(cur_script->script_elements = ListCreate(sizeof(ScriptElement_t*)));
	cur_script->num_elements = 0;
	
	for (int i = 0; i < num_XMLelements; i++) {
		XMLErrChk(CVIXMLGetChildElementByIndex(root_element, i, &xml_element));
		XMLErrChk(CVIXMLGetAttributeByName(xml_element, ATTR_ID, &xmlelement_type));
		XMLErrChk(CVIXMLGetAttributeValue(xmlelement_type, attr_value));
		RemoveSurroundingWhiteSpace(attr_value);
		MAGIC_NUM = (ElementType_t)atoi(attr_value);
		
		switch (MAGIC_NUM) {
			case START:
				XMLErrChk(get_XMLElement_childvalues(&xml_element, value, START_ELEMENT_CHILDS));
				nullChk(element = init_StartElement(whisker_script, value));
				break;
				
			case ACTION:
				XMLErrChk(get_XMLElement_childvalues(&xml_element, value, ACTION_ELEMENT_CHILDS));
				nullChk(element = init_ActionElement(whisker_script, value));
				break;
			
			case CONDITION:
				XMLErrChk(get_XMLElement_childvalues(&xml_element, value, COND_ELEMENT_CHILDS));
				nullChk(element = init_ConditionElement(whisker_script, value));
				break;
				
			case REPEAT:
				XMLErrChk(get_XMLElement_childvalues(&xml_element, value, REPEAT_ELEMENT_CHILDS));
				nullChk(element = init_RepeatElement(whisker_script, value));
				break;
				
			case STOP:
				XMLErrChk(get_XMLElement_childvalues(&xml_element, value, STOP_ELEMENT_CHILDS));
				nullChk(element = init_StopElement(whisker_script, value));
				break;
				
			case WAIT:
				XMLErrChk(get_XMLElement_childvalues(&xml_element, value, WAIT_ELEMENT_CHILDS));
				nullChk(element = init_WaitElement(whisker_script, value));
				break;
		}
		/* Set Panel Attributes */
		SetPanelAttribute(element->panel_handle, ATTR_TITLEBAR_VISIBLE, 0);
				
		/* Add this element into script_elements list */
		if (NULL == ListInsertItem(cur_script->script_elements, &element, (cur_script->num_elements + 1))) {
			DiscardPanel(element->panel_handle);
			LOG_MSG(0, "Error when inserting element into list\n");
			goto Error;
		}
		cur_script->num_elements += 1;
		
		CVIXMLDiscardElement(xml_element);
		xml_element = NULL;
	}
	
	redraw_script_elements(cur_script);
	
	/* Undim element add functionality */
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptElement, ATTR_DIMMED, 0);
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptAdd, ATTR_DIMMED, 0);
	
Error:
	CVIXMLDiscardElement(xml_element);
	CVIXMLDiscardElement(root_element);
	CVIXMLDiscardDocument(script_doc);
	DisplayActiveXErrorMessageOnFailure(error);
	return;	
}

/*
 * Ask for file path 
 */
int
get_logfile_path(WhiskerScript_t *whisker_script)
{
	WScript_t	*cur_script = &(whisker_script->cur_script);
	char 		msg[LOG_MSG_LEN];
	int			ret = 0;
	
	/* Select log file */
	if (cur_script->log_file.VALID_FILE) {
		/* Prompt user to use same log file */
		sprintf(msg, "Use existing log file : %s ", cur_script->log_file.file_path); 
		ret = ConfirmPopup ("Log File", msg);
		
		if (ret == 0) {	/* New file is asked */
			fclose(cur_script->log_file.file_handle);
			cur_script->log_file.VALID_FILE = FALSE;
		}
	}
	
	/* New log file location */
	if (ret == 0) {
		ret = FileSelectPopup("", "*.txt", "", "Log File",
                                VAL_SAVE_BUTTON, 0, 1, 1, 0, cur_script->log_file.file_path);
        if (!ret) {
			return -1;
		}
        /* TODO: Set file path to UI display */
		cur_script->log_file.VALID_FILE = TRUE;
		
		/* Open file */
		cur_script->log_file.file_handle = fopen(cur_script->log_file.file_path, "w+");
		if (cur_script->log_file.file_handle == NULL) {
			MessagePopup("Log file Error", "Error while opening log file!");
			cur_script->log_file.VALID_FILE = FALSE;
			return -1;
		}
		
		/* Set file path in text box */
		SetCtrlVal(whisker_script->main_panel_handle, ScriptPan_LogFile, 
				   									cur_script->log_file.file_path);
	}
	return;
}
 

/* Script Runner that runs current script */
int CVICALLBACK
script_runner(void *thread_data)
{
	WhiskerScript_t	*whisker_script = (WhiskerScript_t *)thread_data;
	WScript_t		*cur_script = &(whisker_script->cur_script);
	ScriptElement_t	*element = NULL;
	int				i = 1;
	int				value = 0;
	
	if (cur_script->script_elements == NULL || cur_script->num_elements <= 0) {
		MessagePopup("Script Run Error", "No Elements in script to run!");
		return -1;	
	}
	
	GetCtrlVal(whisker_script->main_panel_handle, ScriptPan_LogCheck, &value);
	if (value) {
		get_logfile_path(whisker_script);
	} else {
		/* Invalidate logging */
		cur_script->log_file.VALID_FILE = FALSE;
	}
	
	/* Dim other invalid options.
	 * Add, Load, Save, New and Run button itself
	 */ 
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptNew,
							ATTR_DIMMED, 1);
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptLoad,
							ATTR_DIMMED, 1);
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptSave,
							ATTR_DIMMED, 1);
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptAdd,
							ATTR_DIMMED, 1);
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptRun,
							ATTR_DIMMED, 1);
	
	/* UnDim Other Pause and Stop */
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptPause,
							ATTR_DIMMED, 0);
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptStop,
							ATTR_DIMMED, 0);
	
	/* Reset Log Box */
	ResetTextBox (whisker_script->main_panel_handle, ScriptPan_ScriptLog, "");
	save_and_print_LogMsg(whisker_script, LOG_TITLE);
	save_and_print_LogMsg(whisker_script, LOG_HYPHEN); 
	
	CmtGetLock(cur_script->lock);
	cur_script->run_status = STARTED;
	while (i <= cur_script->num_elements && cur_script->run_status != STOPPED) {
		if (cur_script->run_status == PAUSED) {
			CmtReleaseLock(cur_script->lock);
			Sleep(100);
			CmtGetLock(cur_script->lock);
			continue;
		}
		CmtReleaseLock(cur_script->lock);
		
		ListGetItem(cur_script->script_elements, &element, i);
		element->runner_function(element, whisker_script, &i);
		i++;
		CmtGetLock(cur_script->lock);
	}
	
	cur_script->run_status = STOPPED;
	CmtReleaseLock(cur_script->lock);
	
	/* Undim other invalid options
	 * New, Save, Load, Add
	 */
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptNew,
							ATTR_DIMMED, 0);
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptLoad,
							ATTR_DIMMED, 0);
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptSave,
							ATTR_DIMMED, 0);
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptAdd,
							ATTR_DIMMED, 0);
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptRun,
							ATTR_DIMMED, 0);
							 
	/* Dim Other Pause and Stop */
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptPause,
							ATTR_DIMMED, 1);
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptStop,
							ATTR_DIMMED, 1);
	
	return 0;

}

/* Save and Print log message. This operation should be async */
inline void
save_and_print_LogMsg(WhiskerScript_t *whisker_script, char	*str)
{
	WScript_t	*cur_script = &(whisker_script->cur_script);
	SYSTEMTIME	dh;
	char		msg[LOG_MSG_LEN];
	
	/* Get current local date and time */
	GetLocalTime(&dh);
	sprintf(msg, "%d/%d/%04d %d:%02d:%02d \t%s", dh.wDay, dh.wMonth,
											dh.wYear, dh.wHour, 
											dh.wMinute, dh.wSecond, str);
	
	/* Write it to UI */
	SetCtrlVal(whisker_script->main_panel_handle, ScriptPan_ScriptLog, msg);
	SetCtrlVal(whisker_script->main_panel_handle, ScriptPan_ScriptLog, "\n");
	
	/* If log file location is valid then write messages asynchronously */
	if (cur_script->log_file.VALID_FILE == TRUE) {
		fprintf(cur_script->log_file.file_handle, "%s \n", msg);
	}
	return;
}

/*********************************************************************
 * Element specific functions.										 *
 *********************************************************************/

void *
init_Element(WhiskerScript_t *whisker_script, size_t element_size, int panel_handle)
{
	ScriptElement_t	*element = (ScriptElement_t *)malloc(element_size);
	
	if (element == NULL) {
		return NULL;	
	}
	
	element->panel_handle = DuplicatePanel(whisker_script->container_panel_handle,
										   		panel_handle, "", 0, 0);
	
	if (panel_handle <= 0) {
		OKfree(element);
		return NULL;
	}
	
	return (void *)element;
}

ScriptElement_t *
init_StartElement(WhiskerScript_t *whisker_script, int *value)
{
	StartElement_t	*start_element = (StartElement_t *)init_Element(whisker_script, 
									 		sizeof(StartElement_t),
									 		whisker_script->startElement_panel_handle);
			  
	if (start_element == NULL) {
		return NULL;
	}
	
	start_element->base_class.MAGIC_NUM = START;
	start_element->base_class.runner_function = start_runner;
																
	if (value != NULL) { /* Init Start Element with supplied values */
		start_element->delay = value[1];	/* 0th index for delay */
		/* Update UI */
		SetCtrlVal(start_element->base_class.panel_handle, StartEle_EleDelay, value[1]);
	}
	
	return (ScriptElement_t *)start_element;
}

ScriptElement_t *
init_ActionElement(WhiskerScript_t *whisker_script, int *value)
{
	ActionElement_t	*action_element = (ActionElement_t *)init_Element(whisker_script, 
									  		sizeof(ActionElement_t),
									 		whisker_script->actionElement_panel_handle);

	if (action_element == NULL) {
		return NULL;
	}
				
	action_element->base_class.MAGIC_NUM = ACTION;
	action_element->base_class.runner_function = action_runner;
	
	if (value != NULL) {
		action_element->action = value[1];
		action_element->duration = (Action_t *) value[2];
		action_element->IO_Channel = value[3];
		
		/* Update UI */
		SetCtrlVal(action_element->base_class.panel_handle, ActionEle_EleCommand, value[1]);
		SetCtrlVal(action_element->base_class.panel_handle, ActionEle_EleDuration, value[2]);
		SetCtrlVal(action_element->base_class.panel_handle, ActionEle_EleIO_CH, value[3]);
	}						
	
	return (ScriptElement_t *)action_element;
}

ScriptElement_t *
init_ConditionElement(WhiskerScript_t *whisker_script, int *value)
{
	ConditionElement_t	*condition_element = (ConditionElement_t *)init_Element(whisker_script, 
													sizeof(ConditionElement_t),
									 				whisker_script->condElement_panel_handle);
	
	if (condition_element == NULL) {
		return NULL;
	}
	
	condition_element->base_class.MAGIC_NUM = CONDITION;
	condition_element->base_class.runner_function = condition_runner;
	
	if (value != NULL) {
		condition_element->IO_channel = value[1];
		condition_element->value = value[2];
		condition_element->true_step = value[3];
		condition_element->false_step = value[4];
		
		/* Update UI */
		SetCtrlVal(condition_element->base_class.panel_handle, CondEle_EleIO_CH, value[1]);
		SetCtrlVal(condition_element->base_class.panel_handle, CondEle_EleValue, value[2]);
		SetCtrlVal(condition_element->base_class.panel_handle, CondEle_EleTrue, value[3]);
		SetCtrlVal(condition_element->base_class.panel_handle, CondEle_EleFalse, value[4]);
		
	}
		
	return (ScriptElement_t *)condition_element;
}

ScriptElement_t *
init_RepeatElement(WhiskerScript_t *whisker_script, int *value)
{
	RepeatElement_t	*repeat_element = (RepeatElement_t *)init_Element(whisker_script, 
												sizeof(RepeatElement_t),
									 			whisker_script->repeatElement_panel_handle);
	
	if (repeat_element == NULL) {
		return NULL;
	}
				
	repeat_element->base_class.MAGIC_NUM = REPEAT;
	repeat_element->base_class.runner_function = repeat_runner;
	repeat_element->counter = 0;
	
	if (value != NULL) {
		repeat_element->ntimes = value[1];
		repeat_element->repeat_step = value[2];
		
		/* Update UI */
		SetCtrlVal(repeat_element->base_class.panel_handle, RepEle_EleNTimes, value[1]);
		SetCtrlVal(repeat_element->base_class.panel_handle, RepEle_EleStep, value[2]);
	}
		
	return (ScriptElement_t *)repeat_element;
}

ScriptElement_t *
init_StopElement(WhiskerScript_t *whisker_script, int *value)
{
	StopElement_t	*stop_element = (StopElement_t *)init_Element(whisker_script, 
												sizeof(StopElement_t),
									 			whisker_script->stopElement_panel_handle);
	
	if (stop_element == NULL) {
		return NULL;
	}
	
	stop_element->base_class.MAGIC_NUM = STOP;
	stop_element->base_class.runner_function = stop_runner;
	
	if (value != NULL) { /* Init Start Element with supplied values */
		stop_element->delay = value[1];	/* 0th index for delay */
		SetCtrlVal(stop_element->base_class.panel_handle, StopEle_EleDelay, value[1]);
	}
	
	return (ScriptElement_t *)stop_element;
}

ScriptElement_t *
init_WaitElement(WhiskerScript_t *whisker_script, int *value)
{
	WaitElement_t	*wait_element = (WaitElement_t *)init_Element(whisker_script, 
												sizeof(WaitElement_t),
									 			whisker_script->waitElement_panel_handle);

	if (wait_element == NULL) {
		return NULL;
	}
					
	wait_element->base_class.MAGIC_NUM = WAIT;
	wait_element->base_class.runner_function = wait_runner;
	
	if (value != NULL) { /* Init Start Element with supplied values */
		wait_element->delay = value[1];	/* 0th index for delay */
		SetCtrlVal(wait_element->base_class.panel_handle, WaitEle_EleDelay, value[1]);
	}
	
	return (ScriptElement_t *)wait_element;
}


/*********************************************************************
 * XML specific functions											 *
 *********************************************************************/

static void 
DisplayActiveXErrorMessageOnFailure(HRESULT error)
{
	char errBuf [ERR_MSG_SIZE];
	
    if (FAILED(error)) {
        CVIXMLGetErrorString (error, errBuf, sizeof(errBuf));
        MessagePopup ("XML Document Error", errBuf);
    }
	
	return;
}

/* Add sub element and its value to parent element */
static int
append_childElement_and_value(CVIXMLElement *par_element, char	*tag, char	*value)
{
	CVIXMLElement	child_element;
	HRESULT         error = S_OK;
	
	/* Add child element and its value */
	XMLErrChk(CVIXMLNewElement(*par_element, -1, tag, &child_element));
	XMLErrChk(CVIXMLSetElementValue(child_element, value));
	
Error:
	CVIXMLDiscardElement(child_element);
	DisplayActiveXErrorMessageOnFailure(error);
	return error;
}

/* Add tag and value to script element */
static int
add_XMLElement_toRoot(CVIXMLElement *par_element, char *tag[], char value[][XML_ELEMENT_VALUE], int index)
{
	CVIXMLElement	xml_element;
	HRESULT	error = S_OK;
	int		i = 1;
	
	/* Add child start element to script element */
	XMLErrChk(CVIXMLNewElement(*par_element, index, tag[0], &xml_element));
	XMLErrChk(CVIXMLAddAttribute(xml_element, ATTR_ID, value[0]));
	
	while (tag[i] != NULL) {
		if (append_childElement_and_value(&xml_element, tag[i], value[i])) {
			goto Error;
		}
		i++;
	}
	
Error:
	CVIXMLDiscardElement(xml_element);
	DisplayActiveXErrorMessageOnFailure(error);
	return error;
}

static int
get_XMLElement_childvalues(CVIXMLElement *xml_element, int *value, int num_child)
{
	CVIXMLElement	child_element = NULL;
	HRESULT			error = S_OK;
	char			xml_value[XML_ELEMENT_VALUE];
	
	for (int i = 0; i < num_child; i++) {
		XMLErrChk(CVIXMLGetChildElementByIndex(*xml_element, i, &child_element));
		XMLErrChk(CVIXMLGetElementValue (child_element, xml_value));
		RemoveSurroundingWhiteSpace (xml_value);		  
		value[i] = atoi(xml_value);
		CVIXMLDiscardElement(child_element);
	}
	
Error:
	//CVIXMLDiscardElement(child_element); /* TODO: */
	DisplayActiveXErrorMessageOnFailure(error);
	return error;
}

/************************************************************************
 * Runner functions for each element 									*
 ************************************************************************/
void
start_runner(void *element, void *whisker_script, int *index)
{
	Whisker_t		*whisker_m = (Whisker_t *)
								((WhiskerScript_t *)whisker_script)->whisker_m;
	StartElement_t	*start_element = (StartElement_t *)element;
	char			log_msg[LOG_MSG_LEN];
	
	/* Just delay for a while */
	if (start_element->delay <=0 ) {
		return;
	}
	
	/* Log message to file and text box */
	sprintf(log_msg, "Start :\tDuration %d ms", start_element->delay);
	save_and_print_LogMsg((WhiskerScript_t *)whisker_script, log_msg);
	
	SetCtrlVal(start_element->base_class.panel_handle, StartEle_LED, 1);
	
	Sleep(start_element->delay);

	SetCtrlVal(start_element->base_class.panel_handle, StartEle_LED, 0);
	return;
}

void
action_runner(void *element, void *whisker_script, int *index)
{
	Whisker_t		*whisker_m = (Whisker_t *)
							((WhiskerScript_t *)whisker_script)->whisker_m;
	ActionElement_t	*action_element = (ActionElement_t *)element;
	char			log_msg[LOG_MSG_LEN];
	
	/* Log message to file and text box */
	sprintf(log_msg, "Action :\t %s Duration %d ms", 
						Action_String[(int)action_element->action], 
						action_element->duration);
	save_and_print_LogMsg((WhiskerScript_t *)whisker_script, log_msg);
	
	
	SetCtrlVal(action_element->base_class.panel_handle, ActionEle_LED, 1);
	set_with_timer(&(whisker_m->de_dev), (action_element->IO_Channel - 1), ON, /* 0: default */
				   								action_element->duration);
	SetCtrlVal(action_element->base_class.panel_handle, ActionEle_LED, 0);
	return;	
}

void
stop_runner(void *element, void *whisker_script, int *index)
{
	Whisker_t		*whisker_m = (Whisker_t *)
							((WhiskerScript_t *)whisker_script)->whisker_m;
	StopElement_t	*stop_element = (StopElement_t *)element;
	char			log_msg[LOG_MSG_LEN];
	
	/* Just delay for a while */
	if (stop_element->delay <=0 ) {
		return;
	}
	
	/* Log message to file and text box */
	sprintf(log_msg, "Stop :\tDuration %d ms", stop_element->delay);
	save_and_print_LogMsg((WhiskerScript_t *)whisker_script, log_msg);
	
	SetCtrlVal(stop_element->base_class.panel_handle, StopEle_LED, 1);
	Sleep(stop_element->delay);
	SetCtrlVal(stop_element->base_class.panel_handle, StopEle_LED, 0);
	return;		
}

void
wait_runner(void *element, void *whisker_script, int *index)
{
	Whisker_t		*whisker_m = (Whisker_t *)
								((WhiskerScript_t *)whisker_script)->whisker_m;
	WaitElement_t	*wait_element = (WaitElement_t *)element;
	char			log_msg[LOG_MSG_LEN];
	
	/* Just delay for a while */
	if (wait_element->delay <=0 ) {
		return;
	}
	
	/* Log message to file and text box */
	sprintf(log_msg, "Wait : Duration %d ms", wait_element->delay);
	save_and_print_LogMsg((WhiskerScript_t *)whisker_script, log_msg);
	
	SetCtrlVal(wait_element->base_class.panel_handle, WaitEle_LED, 1);
	Sleep(wait_element->delay);
	SetCtrlVal(wait_element->base_class.panel_handle, WaitEle_LED, 0);

	return;		
}

void
condition_runner(void *element, void *whisker_script, int *index)
{
	Whisker_t			*whisker_m = (Whisker_t *)
									((WhiskerScript_t *)whisker_script)->whisker_m;
	ConditionElement_t	*cond_element = (ConditionElement_t *)element;
	char				log_msg[LOG_MSG_LEN];
	int					input_value;
	
	SetCtrlVal(cond_element->base_class.panel_handle, CondEle_LED, 1);
	
	/* Test input on cond_element->IO_channel */
	 input_value = get_input_value(&whisker_m->de_dev, (cond_element->IO_channel - 1)); /* 0: Default */
	 
	 if (cond_element->value == input_value) {
		*index = cond_element->true_step;
	 } else {
		*index = cond_element->false_step;
	 }
	 
	 *index = *index - 1;	/* Decrement by one because, script runner increments it */
	
	/* Log message to file and text box */
	sprintf(log_msg, "Condition : I/O Channel %d Value %d goto step %d", 
							cond_element->IO_channel, cond_element->value, *index + 1);
	save_and_print_LogMsg((WhiskerScript_t *)whisker_script, "Condition");
	
	SetCtrlVal(cond_element->base_class.panel_handle, CondEle_LED, 0);
	return;
}

void
repeat_runner(void *element, void *whisker_script, int *index)
{
	Whisker_t			*whisker_m = (Whisker_t *)
									((WhiskerScript_t *)whisker_script)->whisker_m;
	RepeatElement_t		*repeat_element = (RepeatElement_t *)element;
	char				log_msg[LOG_MSG_LEN];
	
	/* Log message to file and text box */
	save_and_print_LogMsg((WhiskerScript_t *)whisker_script, "Repeat");
	
	SetCtrlVal(repeat_element->base_class.panel_handle, RepEle_LED, 1);
	if (repeat_element->counter < repeat_element->ntimes) {
		*index = repeat_element->repeat_step - 1; 	/* Decrement by one because script runner
													 * increments it by one */
		repeat_element->counter++;
	}
	SetCtrlVal(repeat_element->base_class.panel_handle, RepEle_LED, 0);
	return;
}





