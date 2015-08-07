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
#include "UI_Whisker.h"
#include "UI_Scripts.h"
#include "CVIXML.h"

//==============================================================================
// Constants
#define XMLErrChk(f) 		if (error = (f), FAILED (error)) goto Error; else
#define TOT_IO_CH			32
#define ERR_MSG_SIZE		128

#define MAX_CHILD_ELEMENTS		5		/* Condition Element has maximum childrens */
#define START_ELEMENT_CHILDS	2			
#define ACTION_ELEMENT_CHILDS	4
#define COND_ELEMENT_CHILDS		5
#define REPEAT_ELEMENT_CHILDS	3
#define STOP_ELEMENT_CHILDS		2
#define WAIT_ELEMENT_CHILDS		2
#define MSG_ELEMENT_CHILDS		2
#define SOUND_ELEMENT_CHILDS	2

#define ATTR_ID				"ID"
#define XML_ROOT_ELEMENT	"Script"
#define START_ELEMENT_TAG	"Start"
#define ACTION_ELEMENT_TAG	"Action"
#define COND_ELEMENT_TAG	"Condition"
#define REPEAT_ELEMENT_TAG	"Repeat"
#define STOP_ELEMENT_TAG	"Stop"
#define WAIT_ELEMENT_TAG	"Wait"
#define MSG_ELEMENT_TAG		"Message"
#define SOUND_ELEMENT_TAG	"Sound"

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
#define TEXT_TAG			"Text"
#define FILE_PATH_TAG		"FilePath"

#define LOG_MSG_LEN				256
#define LOG_TITLE				"ID\t\tType\t\tParams\n"
#define LOG_HYPHEN				"-------\t\t-----------------\t\t------------------"
#define LOG_NUM_LINES			100
#define LOG_NUM_DELETE_LINES    10		/* 10% of LOG_NUM_LINES */

const char	*Action_String[] = {
			"AIRPUFF",
			"DROPOUT",
			"DROPIN" };

//==============================================================================
// Types

//==============================================================================
// Static global variables
static size_t	log_lines_counter = 0;		/* counter: number of log lines that text box can hold */
//==============================================================================
// Static functions

//==============================================================================
// Global variables
WhiskerScript_t *whisker_script = NULL;
//==============================================================================
// Global functions
inline void setscript_ctrl_attribute(WhiskerScript_t *whisker_script);
static int add_XMLElement_toRoot(CVIXMLElement *par_element, char *tag[], char value[][XML_ELEMENT_VALUE], int index);
static int get_XMLElement_childvalues(CVIXMLElement *xml_element, char value[][XML_ELEMENT_VALUE], int num_child);

void start_runner(void *element, void *whisker_script, int *index);
void action_runner(void *element, void *whisker_script, int *index);
void condition_runner(void *element, void *whisker_script, int *index);
void repeat_runner(void *element, void *whisker_script, int *index);
void stop_runner(void *element, void *whisker_script, int *index);
void wait_runner(void *element, void *whisker_script, int *index);
void message_runner(void *element, void *whisker_script, int *index);
void sound_runner(void *element, void *whisker_script, int *index);
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
	int			error = 0;
	char		IO_channel[7] = { 0 };
	
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
	errChk(whisker_script->msgElement_panel_handle = LoadPanel(whisker_script->container_panel_handle,
														MOD_WhiskerScript_UI, MsgEle));
	errChk(whisker_script->soundElement_panel_handle = LoadPanel(whisker_script->container_panel_handle,
														MOD_WhiskerScript_UI, SoundEle));
																				   
	/* Set Ctrl attibute */
	setscript_ctrl_attribute(whisker_script);
	
	/* Set I/O Channels */
	for (int i = 0; i < TOT_IO_CH; i++) {
		sprintf(IO_channel, "CH %d", i+1);	/* Show Channels to user */
		InsertListItem(whisker_script->actionElement_panel_handle, ActionEle_EleIO_CH, i,
					   IO_channel, i);
		InsertListItem(whisker_script->condElement_panel_handle, CondEle_EleIO_CH, i,
					   IO_channel, i);
	}
	
	/* Adjust Container Panel Position 
	 * XXX : Hardcoded Value.
	 */
	SetPanelAttribute(whisker_script->container_panel_handle, ATTR_TOP, 95);
	SetPanelAttribute(whisker_script->container_panel_handle, ATTR_LEFT, 15);
	
	/* Get only element panel height which is required to scroll the script
	 * when it is running.
	 */
	GetPanelAttribute(whisker_script->condElement_panel_handle, 
					  				ATTR_HEIGHT, &(whisker_script->element_panel_height));
	
	/* Display Panel */
	DisplayPanel(whisker_script->main_panel_handle);
	DisplayPanel(whisker_script->container_panel_handle);
	
Error:
	return error;
}

void
discard_script_elements(WScript_t *cur_script)
{
	ScriptElement_t	*element = NULL;
	
	while (ListNumItems(cur_script->script_elements)) {
		ListRemoveItem(cur_script->script_elements, &element, FRONT_OF_LIST);
		
		/* Discard Panel */
		DiscardPanel(element->panel_handle);
		
		/* Free element */
		OKfree(element);
	}
	
	cur_script->num_elements = 0;
	return;
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
	
	/* Discard elements from the list */
	discard_script_elements(cur_script);
	
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
	discard_cur_script(&(whisker_script->cur_script));	/* Discard if current script is active */
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
	SetCtrlAttribute (whisker_script->main_panel_handle, ScriptPan_ScriptDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->main_panel_handle, ScriptPan_ScriptImportSetting, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
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
	
	SetCtrlAttribute (whisker_script->msgElement_panel_handle, MsgEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->msgElement_panel_handle, MsgEle_EleApply, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->soundElement_panel_handle, SoundEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->soundElement_panel_handle, SoundEle_EleApply, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->soundElement_panel_handle, SoundEle_EleLoad, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->soundElement_panel_handle, SoundEle_ElePlay, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
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
		} else if (element->MAGIC_NUM == MESSAGE) {
			SetCtrlVal(element->panel_handle, MsgEle_EleNum, seq_num);
		} else if (element->MAGIC_NUM == SOUND) {
			SetCtrlVal(element->panel_handle, SoundEle_EleNum, seq_num);
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
	char			value[MAX_CHILD_ELEMENTS + 1][XML_ELEMENT_VALUE];	/* TODO: Need dynamic allocation */
	
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
				
			case MESSAGE:	/* Add Message Element */
				/* Set tag */
				tag[0] = MSG_ELEMENT_TAG;	tag[1] = SEQ_NO_TAG;
				tag[2] = TEXT_TAG;			tag[3] = NULL;
				
				/* Set Value. Tag index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				strncpy(value[2], ((MessageElement_t *)element)->text, 
														XML_ELEMENT_VALUE - 1);
											
				break;
				
			case SOUND:		/* Add sound Element */
				/* Set Tag */
				tag[0] = SOUND_ELEMENT_TAG;	tag[1] = SEQ_NO_TAG;
				tag[2] = FILE_PATH_TAG;		tag[3] = NULL;
				
				/* Set Value. Tag index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				strncpy(value[2], ((SoundElement_t *)element)->file_path.file_path, 
														XML_ELEMENT_VALUE - 1);
											
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
        /* Set file path to UI display */
		cur_script->xml_script.VALID_FILE = TRUE;
		SplitPath(cur_script->xml_script.file_path, NULL, NULL, cur_script->xml_script.file_name);
		SetCtrlVal(whisker_script->main_panel_handle, ScriptPan_ScriptName, 
			   									cur_script->xml_script.file_name);
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
	char			value[MAX_CHILD_ELEMENTS][XML_ELEMENT_VALUE];	/* TODO: Need dynamic allocation */
	
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
		/* As we have already discarded a script we should change UI */
		SetCtrlVal(whisker_script->main_panel_handle, ScriptPan_ScriptName, NO_SCRIPT_NAME);
		SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptAdd, ATTR_DIMMED, 1);
		return;
	}
    /* Set file path to UI display */
	cur_script->xml_script.VALID_FILE = TRUE;
	SplitPath(cur_script->xml_script.file_path, NULL, NULL, cur_script->xml_script.file_name);
	SetCtrlVal(whisker_script->main_panel_handle, ScriptPan_ScriptName, 
			   									cur_script->xml_script.file_name);
	
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
				
			case MESSAGE:
				XMLErrChk(get_XMLElement_childvalues(&xml_element, value, MSG_ELEMENT_CHILDS));
				nullChk(element = init_MessageElement(whisker_script, value));
				break;
				
			case SOUND:
				XMLErrChk(get_XMLElement_childvalues(&xml_element, value, SOUND_ELEMENT_CHILDS));
				nullChk(element = init_SoundElement(whisker_script, value));
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
	return 0;
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
		if (-1 == get_logfile_path(whisker_script)) {
			return -1;
		}
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
	log_lines_counter = 0;
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
		
		/* Before calling runner function, just scroll element appropriately.
		 * Element Panel height is retrieved in init function and maintained
		 * in whisker_script. We could get the height in local variable
		 * but there is no issue in maintaining in global structure.
		 */
		SetPanelAttribute(whisker_script->container_panel_handle, ATTR_VSCROLL_OFFSET, 
			(whisker_script->element_panel_height + INTER_ELEMENT_SPACING) * (i-1));
		
		ListGetItem(cur_script->script_elements, &element, i);
		element->runner_function(element, whisker_script, &i);
		i++;
		CmtGetLock(cur_script->lock);
	}
	
	cur_script->run_status = STOPPED;
	CmtReleaseLock(cur_script->lock);
	
	/* Repeat Elements counter value reached ntimes value. So reset it. 
	 * TODO: Coudn't find better way
	 */
	for (i = 0; i <= cur_script->num_elements; i++) {
		ListGetItem(cur_script->script_elements, &element, i);
		if (element->MAGIC_NUM == REPEAT) {
			((RepeatElement_t *)element)->counter = 0; 
		}
	}
	
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
	
	/* Adjust lines in log text box */
	log_lines_counter++;
	if (log_lines_counter > LOG_NUM_LINES) {
		DeleteTextBoxLines(whisker_script->main_panel_handle, ScriptPan_ScriptLog, 
						   2 /* Number of header lines */, LOG_NUM_DELETE_LINES);
		log_lines_counter -= LOG_NUM_DELETE_LINES;
	}
	
	return;
}

/* Import Script Elements from Whisker UI */
void
import_settings(WhiskerScript_t	*whisker_script)
{
	WScript_t		*cur_script = (WScript_t *)&(whisker_script->cur_script);
	Whisker_t		*whisker_m = (Whisker_t *)whisker_script->whisker_m;
	ScriptElement_t	*element = NULL;
	ActionElement_t	*action_element = NULL;
	int				ch, duration;
	
	/* Empty Script check */
	if (cur_script->num_elements == 0 || cur_script->script_elements == NULL) {
		MessagePopup("Import Setting", "Empty Script!");
		return;
	}
	
	/* Iterate through each script elements and if it is action element then
	 * based on action import setting from whisker UI or module */
	for (int i = 1; i <= cur_script->num_elements; i++) {
		/* Get Element one by one */
		ListGetItem(cur_script->script_elements, &element, i);
		
		/* Action Element */
		if (element->MAGIC_NUM == ACTION) {
			action_element = (ActionElement_t *)element;
			
			switch (action_element->action) {
				case AIRPUFF:
					ch = whisker_m->de_dev.IO_Channel[AIR_PUFF_CH];
					GetCtrlVal(whisker_m->whisker_ui.tab_air_puff, TabAirPuff_Num_AirPuff, 
							   									&duration);
					break;
					
				case DROPOUT:
					ch = whisker_m->de_dev.IO_Channel[DROP_OUT_CH];
					GetCtrlVal(whisker_m->whisker_ui.tab_drop_out, TabDropOut_Num_DropOut, 
							   									&duration);
					break;
					
				case DROPIN:
					ch = whisker_m->de_dev.IO_Channel[DROP_IN_CH];
					GetCtrlVal(whisker_m->whisker_ui.tab_drop_in, TabDropIn_Num_DropIN, 
							   									&duration);
					break;
			}
			action_element->IO_Channel = ch;
			action_element->duration = duration;
			
			/* Update UI */
			SetCtrlVal(element->panel_handle, ActionEle_EleIO_CH, ch);
			SetCtrlVal(element->panel_handle, ActionEle_EleDuration, duration);
		}
			
	}
	
	return;
	
}

/*********************************************************************
 * Element specific functions.										 *
 *********************************************************************/

void *
init_Element(WhiskerScript_t *whisker_script, size_t element_size, int panel_handle)
{
	ScriptElement_t	*element = (ScriptElement_t *)calloc(1, element_size);
	
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
init_StartElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE])
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
		start_element->delay = atoi(value[1]);	/* 0th index for delay */
		/* Update UI */
		SetCtrlVal(start_element->base_class.panel_handle, StartEle_EleDelay, 
				   										start_element->delay);
	}
	
	return (ScriptElement_t *)start_element;
}

ScriptElement_t *
init_ActionElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE])
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
		action_element->action = atoi(value[1]);
		action_element->duration = (Action_t *) atoi(value[2]);
		action_element->IO_Channel = atoi(value[3]);
		
		/* Update UI */
		SetCtrlVal(action_element->base_class.panel_handle, ActionEle_EleCommand, 
				   										action_element->action);
		SetCtrlVal(action_element->base_class.panel_handle, ActionEle_EleDuration,
				  										action_element->duration);
		SetCtrlVal(action_element->base_class.panel_handle, ActionEle_EleIO_CH,
				  										action_element->IO_Channel);
	}						
	
	return (ScriptElement_t *)action_element;
}

ScriptElement_t *
init_ConditionElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE])
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
		condition_element->IO_channel = atoi(value[1]);
		condition_element->value = atoi(value[2]);
		condition_element->true_step = atoi(value[3]);
		condition_element->false_step = atoi(value[4]);
		
		/* Update UI */
		SetCtrlVal(condition_element->base_class.panel_handle, CondEle_EleIO_CH, 
				   								condition_element->IO_channel);
		SetCtrlVal(condition_element->base_class.panel_handle, CondEle_EleValue,
				  								condition_element->value);
		SetCtrlVal(condition_element->base_class.panel_handle, CondEle_EleTrue,
				  								condition_element->true_step);
		SetCtrlVal(condition_element->base_class.panel_handle, CondEle_EleFalse,
				  								condition_element->false_step);  
	}
		
	return (ScriptElement_t *)condition_element;
}

ScriptElement_t *
init_RepeatElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE])
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
		repeat_element->ntimes = atoi(value[1]);
		repeat_element->repeat_step = atoi(value[2]);
		
		/* Update UI */
		SetCtrlVal(repeat_element->base_class.panel_handle, RepEle_EleNTimes, 
				   								repeat_element->ntimes);
		SetCtrlVal(repeat_element->base_class.panel_handle, RepEle_EleStep, 
				   								repeat_element->repeat_step);
	}
		
	return (ScriptElement_t *)repeat_element;
}

ScriptElement_t *
init_StopElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE])
{
	StopElement_t	*stop_element = (StopElement_t *)init_Element(whisker_script, 
												sizeof(StopElement_t),
									 			whisker_script->stopElement_panel_handle);
	
	if (stop_element == NULL) {
		return NULL;
	}
	
	stop_element->base_class.MAGIC_NUM = STOP;
	stop_element->base_class.runner_function = stop_runner;
	
	if (value != NULL) { /* Init Stop Element with supplied values */
		stop_element->delay = atoi(value[1]);	/* 0th index for delay */
		SetCtrlVal(stop_element->base_class.panel_handle, StopEle_EleDelay, 
				   									stop_element->delay);
	}
	
	return (ScriptElement_t *)stop_element;
}

ScriptElement_t *
init_WaitElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE])
{
	WaitElement_t	*wait_element = (WaitElement_t *)init_Element(whisker_script, 
												sizeof(WaitElement_t),
									 			whisker_script->waitElement_panel_handle);

	if (wait_element == NULL) {
		return NULL;
	}
					
	wait_element->base_class.MAGIC_NUM = WAIT;
	wait_element->base_class.runner_function = wait_runner;
	
	if (value != NULL) { /* Init Wait Element with supplied values */
		wait_element->delay = atoi(value[1]);	/* 0th index for delay */
		SetCtrlVal(wait_element->base_class.panel_handle, WaitEle_EleDelay,
				  								wait_element->delay);
	}
	
	return (ScriptElement_t *)wait_element;
}

ScriptElement_t *
init_MessageElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE])
{
	MessageElement_t *message_element = (MessageElement_t *)init_Element(whisker_script, 
												sizeof(MessageElement_t),
									 			whisker_script->msgElement_panel_handle);
	
	if (message_element == NULL) {
		return NULL;
	}
	
	message_element->base_class.MAGIC_NUM = MESSAGE;
	message_element->base_class.runner_function = message_runner;
	
	/* XXX: NI BUG Report # 423491
	 * Occurs only in DEBUG build.
	 * When a structure is dynamically allocated and if any field in the structure 
	 * is accessed through its address, it gives run time fatal error.
	 * Workaround : UNCHECKED(address)
	 * WARN: Make sure you use it only when you are completely sure of memory
 	 * access.
	 */
	if (value != NULL) { /* Init Message Element with supplied values */
		strncpy(UNCHECKED(message_element->text), value[1], XML_ELEMENT_VALUE - 1);
		SetCtrlVal(message_element->base_class.panel_handle, MsgEle_EleText,
				   							UNCHECKED(message_element->text));
	}
	
	return (ScriptElement_t *)message_element;
}

ScriptElement_t *
init_SoundElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE])
{
	SoundElement_t *sound_element = (SoundElement_t *)init_Element(whisker_script, 
												sizeof(SoundElement_t),
									 			whisker_script->soundElement_panel_handle);
	
	if (sound_element == NULL) {
		return NULL;
	}
	
	sound_element->base_class.MAGIC_NUM = SOUND;
	sound_element->base_class.runner_function = sound_runner;
	
	/* XXX: NI BUG Report # 423491
	 * Occurs only in DEBUG build.
	 * When a structure is allocated and if any field in the structure is
	 * accessed through its address, it gives run time fatal error.
	 * Workaround : UNCHECKED(address)
	 * WARN: Make sure you use it only when you are completely sure of memory
 	 * access.
	 */
	if (value != NULL) { /* Init Message Element with supplied values */
		strncpy(UNCHECKED(sound_element->file_path.file_path), value[1], XML_ELEMENT_VALUE - 1);
		SetCtrlVal(sound_element->base_class.panel_handle, SoundEle_ElePath,
				   							UNCHECKED(sound_element->file_path.file_path));
		sound_element->file_path.VALID_FILE = TRUE;
	}
	
	return (ScriptElement_t *)sound_element;
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
get_XMLElement_childvalues(CVIXMLElement *xml_element, char value[][XML_ELEMENT_VALUE], int num_child)
{
	CVIXMLElement	child_element = NULL;
	HRESULT			error = S_OK;
	char			xml_value[XML_ELEMENT_VALUE];
	
	for (int i = 0; i < num_child; i++) {
		XMLErrChk(CVIXMLGetChildElementByIndex(*xml_element, i, &child_element));
		XMLErrChk(CVIXMLGetElementValue (child_element, xml_value));
		RemoveSurroundingWhiteSpace (xml_value);		  
		strncpy(value[i], xml_value, XML_ELEMENT_VALUE - 1);
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
	set_with_timer(&(whisker_m->de_dev), action_element->IO_Channel, ON,
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
	 input_value = get_input_value(&whisker_m->de_dev, cond_element->IO_channel);
	 
	 if (cond_element->value == input_value) {
		*index = cond_element->true_step;
	 } else {
		*index = cond_element->false_step;
	 }
	 
	 *index = *index - 1;	/* Decrement by one because, script runner increments it */
	
	/* Log message to file and text box */
	sprintf(log_msg, "Condition : I/O Channel %d Value %d goto step %d", 
							cond_element->IO_channel, cond_element->value, *index + 1);
	save_and_print_LogMsg((WhiskerScript_t *)whisker_script, log_msg);
	
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

void
message_runner(void *element, void *whisker_script, int *index)
{
	Whisker_t			*whisker_m = (Whisker_t *)
							((WhiskerScript_t *)whisker_script)->whisker_m;	
	MessageElement_t	*message_element = (MessageElement_t *)element;

	SetCtrlVal(message_element->base_class.panel_handle, MsgEle_LED, 1);
	save_and_print_LogMsg((WhiskerScript_t *)whisker_script, message_element->text);
	SetCtrlVal(message_element->base_class.panel_handle, MsgEle_LED, 0);
	return;
}

void
sound_runner(void *element, void *whisker_script, int *index)
{
	Whisker_t			*whisker_m = (Whisker_t *)
							((WhiskerScript_t *)whisker_script)->whisker_m;
	SoundElement_t		*sound_element = (SoundElement_t *)element;
	
	/* Log message to file and text box */
	save_and_print_LogMsg((WhiskerScript_t *)whisker_script, "Playing Sound..");
	
	SetCtrlVal(sound_element->base_class.panel_handle, SoundEle_LED, 1);
	/* Play sound */
	if (sound_element->file_path.VALID_FILE == TRUE) {
		sndPlaySound(sound_element->file_path.file_path, SND_SYNC);
	}
	SetCtrlVal(sound_element->base_class.panel_handle, SoundEle_LED, 0);
	return;
}
	





