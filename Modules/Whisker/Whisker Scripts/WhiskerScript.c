//==============================================================================
//
// Title:		WhiskerScript.c
// Purpose:		Whisker Script main file. It implements init, discard etc.
//				functions.
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

/* Static elements in the XML file */
#define MAX_CHILD_ELEMENTS		7		/* Condition Element has maximum childrens */
#define START_ELEMENT_CHILDS	2			
#define ACTION_ELEMENT_CHILDS	5
#define COND_ELEMENT_CHILDS		7
#define REPEAT_ELEMENT_CHILDS	3
#define STOP_ELEMENT_CHILDS		2
#define WAIT_ELEMENT_CHILDS		2
#define MSG_ELEMENT_CHILDS		2
#define SOUND_ELEMENT_CHILDS	3
#define	XYMOVE_ELEMENT_CHILDS	2		/* Others are variable length childs */
#define ZMOVE_ELEMENT_CHILDS	3
#define JUMP_ELEMENT_CHILDS		2
#define THWAIT_ELEMENT_CHILDS	1
#define XYCOND_ELEMENT_CHILDS	4
#define RESREP_ELEMENT_CHILDS	2

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
#define XYMOVE_ELEMENT_TAG	"XYMove"
#define ZMOVE_ELEMENT_TAG	"ZMove"
#define JUMP_ELEMENT_TAG	"Jump"
#define THWAIT_ELEMENT_TAG	"Threadwait"
#define XYCOND_ELEMENT_TAG	"XYCondition"
#define RESREP_ELEMENT_TAG	"ResetRepeat"

#define SEQ_NO_TAG			"Seq_No"
#define DELAY_TAG			"Delay"
#define SUB_ACTION_TAG		"Sub_Action"
#define DURATION_TAG		"Duration"
#define FULL_DURATION_TAG	"Full_Duration"
#define IO_CH_TAG			"IO_Channel"
#define VALUE_TAG			"Value"
#define TRUE_TAG			"True"
#define FALSE_TAG			"False"
#define NTIMES_TAG			"NTimes"
#define STEP_TAG			"Step"
#define TEXT_TAG			"Text"
#define FILE_PATH_TAG		"FilePath"
#define POS_TAG				"Position"
#define Z_DIR_TAG			"Direction"
#define MODE_TAG			"mode"
#define X_TAG				"X"
#define Y_TAG				"Y"

#define X_ATTR				"X"
#define Y_ATTR				"Y"
#define PERCENT_ATTR		"Percent"
#define GOSTATE_ATTR		"GO_State"

#define LOG_TITLE				"ID\t\tType\t\tParams\n"
#define LOG_HYPHEN				"-------\t\t-----------------\t\t------------------"
#define LOG_NUM_LINES			100
#define LOG_NUM_DELETE_LINES    10		/* 10% of LOG_NUM_LINES */

#define BAR_GRAPH_COUNTERS	4
#define GRAPH_PLOT_DELAY	100			/* 100 ms delay to update the graph */

const char	*Action_String[] = {
			"AIR PUFF",
			"DROP OUT",
			"DROP IN",
			"TTL OUT" 
			};

//==============================================================================
// Types
typedef struct
{
	WhiskerScript_t	*whisker_script;	/* Whisker Script */
	ScriptElement_t	*element;			/* Script element */
	int				*index;				/* index of element in the list */
} AsyncThreadData_t;
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
static int append_child_with_attributes(CVIXMLElement *par_element, char *tag[], char value[][XML_ELEMENT_VALUE], int index);
static int load_xymove_positions(CVIXMLElement *par_element, XYMoveElement_t *xymove_element);

void CVICALLBACK async_script_runner(void *thread_data);
void start_runner(void *element, void *whisker_script, int *index);
void CVICALLBACK action_runner(void *element, void *whisker_script, int *index);
void condition_runner(void *element, void *whisker_script, int *index);
void repeat_runner(void *element, void *whisker_script, int *index);
void stop_runner(void *element, void *whisker_script, int *index);
void wait_runner(void *element, void *whisker_script, int *index);
void message_runner(void *element, void *whisker_script, int *index);
void sound_runner(void *element, void *whisker_script, int *index);
void xymove_runner(void *element, void *whisker_script, int *index);
void zmove_runner(void *element, void *whisker_script, int *index);
void jump_runner(void *element, void *whisker_script, int *index);
void thwait_runner(void *element, void *whisker_script, int *index);
void xycondition_runner(void *element, void *whisker_script, int *index);
void resetrepeat_runner(void *element, void *whisker_script, int *index);

inline void save_and_print_LogMsg(WhiskerScript_t *whisker_script, char	*msg);
inline void save_stats(WhiskerScript_t *whisker_script);
inline void reset_stats_UIcounters(WhiskerScript_t	*whisker_script);

static void DisplayActiveXErrorMessageOnFailure(HRESULT error);

/// HIFN  What does your function do?
/// HIPAR x/What inputs does your function expect?
/// HIRET What does your function return?

/* Init and Display Script's UI Panel */
int
init_display_script(void *function_data)
{
INIT_ERR

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
	errChk(whisker_script->xymoveElement_panel_handle = LoadPanel(whisker_script->container_panel_handle,
														MOD_WhiskerScript_UI, XYMoveEle));
	errChk(whisker_script->zmoveElement_panel_handle = LoadPanel(whisker_script->container_panel_handle,
														MOD_WhiskerScript_UI, ZMoveEle));
	errChk(whisker_script->jumpElement_panel_handle = LoadPanel(whisker_script->container_panel_handle,
														MOD_WhiskerScript_UI, JumpEle));
	errChk(whisker_script->thwaitElement_panel_handle = LoadPanel(whisker_script->container_panel_handle,
														MOD_WhiskerScript_UI, THWaitEle));
	errChk(whisker_script->xycondElement_panel_handle = LoadPanel(whisker_script->container_panel_handle,
														MOD_WhiskerScript_UI, XYCondEle));
	errChk(whisker_script->resrepElement_panel_handle = LoadPanel(whisker_script->container_panel_handle,
														MOD_WhiskerScript_UI, ResRepEle));
	
	/* XXX: Dummy element is used in print panel case. Not found better and easy way */
	errChk(whisker_script->dummyElement_panel_handle = LoadPanel(whisker_script->container_panel_handle,
														MOD_WhiskerScript_UI, DummyEle));
	
	/* Set Ctrl attibute */
	setscript_ctrl_attribute(whisker_script);
	
	/* Set I/O Channels */
	for (int i = 0; i < TOT_IO_CH; i++) {
		sprintf(IO_channel, "CH %d", i+1);	/* Show Channels to user */
		InsertListItem(whisker_script->actionElement_panel_handle, ActionEle_EleIO_CH, i,
					   IO_channel, i);
		InsertListItem(whisker_script->condElement_panel_handle, CondEle_EleIO_CH, i,
					   IO_channel, i);
		InsertListItem(whisker_script->zmoveElement_panel_handle, ZMoveEle_EleIO_CH, i,
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
	return errorInfo.error;
}

void
discard_script_elements(ListType script_elements)
{
	ScriptElement_t	*element = NULL;
	
	while (ListNumItems(script_elements)) {
		ListRemoveItem(script_elements, &element, FRONT_OF_LIST);
		
		/* Discard Panel */
		DiscardPanel(element->panel_handle);
		
		/* Free element */
		if (element->MAGIC_NUM == XYMOVE) 
			/* Free saved Positions */
			OKfreeList(&((XYMoveElement_t *)element)->saved_positions, OKFree);
		
		OKfree(element);
	}
	
	//cur_script->num_elements = 0; /* Caller has to set it to NULL */
	return;
}

/* Discard panels, free elements and free list */
int
discard_cur_script(WScript_t *cur_script) 
{
	if (cur_script->script_elements == NULL) {
		LOG_MSG(9, "Script Elements list is NULL");
		return -1;	
	}
	
	/* Discard elements from the list */
	discard_script_elements(cur_script->script_elements);
	
	/* Free list */
	OKfreeList(&cur_script->script_elements, NULL);
	cur_script->num_elements = 0;
	cur_script->xml_script.VALID_FILE = FALSE;
	/* Close log file handle */
	if (cur_script->log_file.VALID_FILE ==  TRUE) {
		fclose(cur_script->log_file.file_handle);
		/* Delete temp file also */
		DeleteFile(LOG_TEMP_FILE);
	}
	cur_script->log_file.VALID_FILE = FALSE;
	
	/* Delete lick detection file */
	DeleteFile(LICK_DET_TEMP_FILE);
	
	/* Reset Log UI text box */
	log_lines_counter = 0;
	ResetTextBox (whisker_script->main_panel_handle, ScriptPan_ScriptLog, "");
	SetCtrlVal(whisker_script->main_panel_handle, ScriptPan_LogFile, NO_LOG_NAME);
	reset_stats_UIcounters(whisker_script);
							   										
	return 0;
}

/* Discard script module structure */
int
discard_script_module()
{
	if (whisker_script == NULL) {
		LOG_MSG(9, "Attempt to discard non-existent script module");
		return -1;
	}
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
	SetCtrlAttribute (whisker_script->main_panel_handle, ScriptPan_ScriptApply, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->main_panel_handle, ScriptPan_ScriptSaveLog, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->main_panel_handle, ScriptPan_ScriptPrint, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->main_panel_handle, ScriptPan_ScriptPrinter, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->main_panel_handle, ScriptPan_ScriptImportSetting, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	/* Add callback data to script elements panel */
	SetCtrlAttribute (whisker_script->startElement_panel_handle, StartEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	//SetCtrlAttribute (whisker_script->startElement_panel_handle, StartEle_EleApply, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->actionElement_panel_handle, ActionEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	//SetCtrlAttribute (whisker_script->actionElement_panel_handle, ActionEle_EleApply, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->condElement_panel_handle, CondEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	//SetCtrlAttribute (whisker_script->condElement_panel_handle, CondEle_EleApply, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->repeatElement_panel_handle, RepEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	//SetCtrlAttribute (whisker_script->repeatElement_panel_handle, RepEle_EleApply, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->stopElement_panel_handle, StopEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	//SetCtrlAttribute (whisker_script->stopElement_panel_handle, StopEle_EleApply, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->waitElement_panel_handle, WaitEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	//SetCtrlAttribute (whisker_script->waitElement_panel_handle, WaitEle_EleApply, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->msgElement_panel_handle, MsgEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	//SetCtrlAttribute (whisker_script->msgElement_panel_handle, MsgEle_EleApply, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->soundElement_panel_handle, SoundEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	//SetCtrlAttribute (whisker_script->soundElement_panel_handle, SoundEle_EleApply, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->soundElement_panel_handle, SoundEle_EleLoad, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->soundElement_panel_handle, SoundEle_ElePlay, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->xymoveElement_panel_handle, XYMoveEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	//SetCtrlAttribute (whisker_script->xymoveElement_panel_handle, XYMoveEle_EleApply, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->xymoveElement_panel_handle, XYMoveEle_EleAdd, ATTR_CALLBACK_DATA, (void *)whisker_script);
	SetCtrlAttribute (whisker_script->xymoveElement_panel_handle, XYMoveEle_EleShow, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->zmoveElement_panel_handle, ZMoveEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	//SetCtrlAttribute (whisker_script->zmoveElement_panel_handle, ZMoveEle_EleApply, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->jumpElement_panel_handle, JumpEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->thwaitElement_panel_handle, THWaitEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->xycondElement_panel_handle, XYCondEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
	
	SetCtrlAttribute (whisker_script->resrepElement_panel_handle, ResRepEle_EleDelete, ATTR_CALLBACK_DATA, (void *)whisker_script);
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
		} else if (element->MAGIC_NUM == XYMOVE) {
			SetCtrlVal(element->panel_handle, XYMoveEle_EleNum, seq_num);
		} else if (element->MAGIC_NUM == ZMOVE) {
			SetCtrlVal(element->panel_handle, ZMoveEle_EleNum, seq_num);
		} else if (element->MAGIC_NUM == JUMP) {
			SetCtrlVal(element->panel_handle, JumpEle_EleNum, seq_num);
		} else if (element->MAGIC_NUM == THWAIT) {
			SetCtrlVal(element->panel_handle, THWaitEle_EleNum, seq_num);	
		} else if (element->MAGIC_NUM == XYCONDITION) {
			SetCtrlVal(element->panel_handle, XYCondEle_EleNum, seq_num);	
		} else if (element->MAGIC_NUM == RESETREPEAT) {
			SetCtrlVal(element->panel_handle, ResRepEle_EleNum, seq_num);
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
	CVIXMLElement	child_element = NULL;
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
				sprintf(value[2], "%u", ((StartElement_t *)element)->delay);
			
				break;
				
			case ACTION: 	/* Add action element */
				/* Set tag */
				tag[0] = ACTION_ELEMENT_TAG;	tag[1] = SEQ_NO_TAG;
				tag[2] = SUB_ACTION_TAG;		tag[3] = DURATION_TAG;
				tag[4] = IO_CH_TAG;				tag[5] = MODE_TAG;
				tag[6] = NULL;
				
				/* Set value . Tag and index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				sprintf(value[2], "%d", ((ActionElement_t *)element)->action);
				sprintf(value[3], "%u", ((ActionElement_t *)element)->duration);
				sprintf(value[4], "%d", ((ActionElement_t *)element)->IO_Channel);
				sprintf(value[5], "%d", element->mode);
				
				break;
				
			case CONDITION: /* Add condition element */
				/* Set tag */
				tag[0] = COND_ELEMENT_TAG;	tag[1] = SEQ_NO_TAG;
				tag[2] = IO_CH_TAG;			tag[3] = VALUE_TAG;
				tag[4] = TRUE_TAG;			tag[5] = FALSE_TAG;
				tag[6] = DURATION_TAG;		tag[7] = FULL_DURATION_TAG;
				tag[8] = NULL;
				
				/* Set value . Tag and index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				sprintf(value[2], "%d", ((ConditionElement_t *)element)->IO_channel);
				sprintf(value[3], "%d", ((ConditionElement_t *)element)->value);
				sprintf(value[4], "%u", ((ConditionElement_t *)element)->true_step);
				sprintf(value[5], "%u", ((ConditionElement_t *)element)->false_step);
				sprintf(value[6], "%u", ((ConditionElement_t *)element)->duration);
				sprintf(value[7], "%u", ((ConditionElement_t *)element)->full_duration);
										
				break;
				
			case REPEAT:	/* Add repeat element */
				/* Set tag */
				tag[0] = REPEAT_ELEMENT_TAG;	tag[1] = SEQ_NO_TAG;
				tag[2] = NTIMES_TAG;			tag[3] = STEP_TAG;
				tag[4] = NULL;
				
				/* Set value . Tag and index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				sprintf(value[2], "%u", ((RepeatElement_t *)element)->ntimes);
				sprintf(value[3], "%u", ((RepeatElement_t *)element)->repeat_step);

				break;
				
			case STOP:		/* Add stop element */
				/* Set tag */
				tag[0] = STOP_ELEMENT_TAG;	tag[1] = SEQ_NO_TAG;
				tag[2] = DELAY_TAG;			tag[3] = NULL;
				
				/* Set value . Tag and index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				sprintf(value[2], "%u", ((StopElement_t *)element)->delay);
			
				break;
				
			case WAIT:		/* Add Wait element */
				/* Set tag */
				tag[0] = WAIT_ELEMENT_TAG;	tag[1] = SEQ_NO_TAG;
				tag[2] = DELAY_TAG;			tag[3] = NULL;
				
				/* Set value . Tag and index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				sprintf(value[2], "%u", ((WaitElement_t *)element)->delay);
				
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
				tag[2] = FILE_PATH_TAG;		tag[3] = MODE_TAG;
				tag[4] = NULL;
				
				/* Set Value. Tag index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				strncpy(value[2], ((SoundElement_t *)element)->file_path.file_path, 
														XML_ELEMENT_VALUE - 1);
				sprintf(value[3], "%d", element->mode);
				
				break;
				
			case XYMOVE:	/* Add XYMove Element */
				/* Set Tag */
				tag[0] = XYMOVE_ELEMENT_TAG;	tag[1] = SEQ_NO_TAG;
				tag[2] = MODE_TAG;				tag[3] = NULL;
				
				/* Set value. Tag index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				sprintf(value[2], "%d", element->mode);
				break;
				
			case ZMOVE:	/* Z move element */
				/* Set Tag */
				tag[0] = ZMOVE_ELEMENT_TAG;	tag[1] = SEQ_NO_TAG;
				tag[2] = IO_CH_TAG;			tag[3] = Z_DIR_TAG;
				tag[4] = NULL;
				
				/* Set value. Tag index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				sprintf(value[2], "%d", ((ZMoveElement_t *)element)->IO_channel);
				sprintf(value[3], "%d", ((ZMoveElement_t *)element)->dir);
				break;
				
			case JUMP:	/* Jump Element */
				/* Set Tag */
				tag[0] = JUMP_ELEMENT_TAG;	tag[1] = SEQ_NO_TAG;
				tag[2] = STEP_TAG;			tag[3] = NULL;
				
				/* Set value. Tag index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				sprintf(value[2], "%u", ((JumpElement_t *)element)->step);
				break;
				
			case THWAIT:	/* Thread wait element */
				/* Set Tag */
				tag[0] = THWAIT_ELEMENT_TAG;	tag[1] = SEQ_NO_TAG;
				tag[2] = NULL;
				
				/* Set value. Tag index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				break;
				
			case XYCONDITION:	/* XY condition element */
				/* Set Tag */
				tag[0] = XYCOND_ELEMENT_TAG;	tag[1] = SEQ_NO_TAG;
				tag[2] = TRUE_TAG;				tag[3] = FALSE_TAG;
				tag[4] = VALUE_TAG;				tag[5] = NULL;
				
				/* Set value. Tag index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				sprintf(value[2], "%u", ((XYConditionElement_t *)element)->true_step);
				sprintf(value[3], "%u", ((XYConditionElement_t *)element)->false_step);
				sprintf(value[4], "%u", ((XYConditionElement_t *)element)->value);
				break;
				
			case RESETREPEAT:	/* Reset Repeat Element */
				/* Set Tag */
				tag[0] = RESREP_ELEMENT_TAG;	tag[1] = SEQ_NO_TAG;
				tag[2] = STEP_TAG;				tag[3] = NULL;
				
				/* Set value. Tag index and value index should match */
				sprintf(value[0], "%d", element->MAGIC_NUM);
				sprintf(value[1], "%d", i);
				sprintf(value[2], "%u", ((ResetRepeatElement_t *)element)->step);
				break;
		}
		
		/* Add tag and value to script element */
		add_XMLElement_toRoot(&root_element, tag, value, i-1);
		
		/* Add dynamic lenght child elements with attribute list and value */
		if (element->MAGIC_NUM == XYMOVE) {
			Position_t	*saved_position = NULL;
			XMLErrChk(CVIXMLGetChildElementByIndex(root_element, i-1, &child_element));
			/* First tag name and others are attribute name */
			tag[0] = POS_TAG;		tag[1] = X_ATTR;
			tag[2] = Y_ATTR;		tag[3] = PERCENT_ATTR;
			tag[4] = GOSTATE_ATTR;	tag[5] = NULL;
			
			for (int j = 1; j <= ListNumItems(((XYMoveElement_t *)element)->saved_positions); 
											j++) {
				ListGetItem(((XYMoveElement_t *)element)->saved_positions, 
															&saved_position, j);
				sprintf(value[1], "%u", saved_position->X);
				sprintf(value[2], "%u", saved_position->Y);
				sprintf(value[3], "%u", saved_position->percent);
				sprintf(value[4], "%d", saved_position->go_state);
				
				append_child_with_attributes(&child_element, tag, value, 
											 				XYMOVE_ELEMENT_CHILDS + (j-1)); 	
			}
			CVIXMLDiscardElement(child_element);
		}
	}
	
	/* Get file Path */
	if (cur_script->xml_script.VALID_FILE) {	/* File is already known */
		/* Confirm user to use existing file or new file */
		ret = ConfirmPopup ("Save Script", "Use existing script file?");
	}
	
	if (ret == 0) {	/* Have to ask for new file path */
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
	}
	
	/* Save XML document to file */
	XMLErrChk(CVIXMLSaveDocument(script_doc, 1, cur_script->xml_script.file_path));
	
Error:
	CVIXMLDiscardElement(root_element);
	CVIXMLDiscardDocument(script_doc);
	DisplayActiveXErrorMessageOnFailure(error);
	return;
}

/* Generic function to convert XML to list */
ListType
convert_xml_to_list(WhiskerScript_t *whisker_script, char file_path[FILE_PATH_LEN]) 
{
INIT_ERR

	ListType		script_elements = NULL;
	ScriptElement_t	*element = NULL;
	CVIXMLDocument	script_doc;
	CVIXMLElement	root_element = NULL;
	CVIXMLElement	xml_element = NULL;
	CVIXMLAttribute xmlelement_type = NULL;
	char			attr_value[XML_ELEMENT_VALUE];
	ElementType_t	MAGIC_NUM;
	HRESULT         error = S_OK;
	int				num_XMLelements = 0;
	char			value[MAX_CHILD_ELEMENTS][XML_ELEMENT_VALUE];
	
	/* Load XML Document and root element */
	XMLErrChk(CVIXMLLoadDocument (file_path, &script_doc));
    XMLErrChk(CVIXMLGetRootElement(script_doc, &root_element));
	XMLErrChk(CVIXMLGetNumChildElements(root_element, &num_XMLelements));
	
	if (num_XMLelements < 1) {
		MessagePopup("Convert XML", "Script Should have atleast one element!");
		goto Error;	
	}
	
	/* Create ScriptElement list */
	nullChk(script_elements = ListCreate(sizeof(ScriptElement_t*)));
	
	for (int i = 0; i < num_XMLelements; i++) {
		XMLErrChk(CVIXMLGetChildElementByIndex(root_element, i, &xml_element));
		XMLErrChk(CVIXMLGetAttributeByName(xml_element, ATTR_ID, &xmlelement_type));
		XMLErrChk(CVIXMLGetAttributeValue(xmlelement_type, attr_value));
		CVIXMLDiscardAttribute(xmlelement_type);
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
				
			case XYMOVE:
				XMLErrChk(get_XMLElement_childvalues(&xml_element, value, XYMOVE_ELEMENT_CHILDS));
				nullChk(element = init_XYMoveElement(whisker_script, value));
				/* Get Dynamic elements */
				load_xymove_positions(&xml_element, (XYMoveElement_t *)element);
				break;
				
			case ZMOVE:
				XMLErrChk(get_XMLElement_childvalues(&xml_element, value, ZMOVE_ELEMENT_CHILDS));
				nullChk(element = init_ZMoveElement(whisker_script, value));
				break;
				
			case JUMP:
				XMLErrChk(get_XMLElement_childvalues(&xml_element, value, JUMP_ELEMENT_CHILDS));
				nullChk(element = init_JumpElement(whisker_script, value));
				break;
				
			case THWAIT:
				XMLErrChk(get_XMLElement_childvalues(&xml_element, value, THWAIT_ELEMENT_CHILDS));
				nullChk(element = init_THWaitElement(whisker_script, value));
				break;
				
			case XYCONDITION:
				XMLErrChk(get_XMLElement_childvalues(&xml_element, value, XYCOND_ELEMENT_CHILDS));
				nullChk(element = init_XYConditionElement(whisker_script, value));
				break;
				
			case RESETREPEAT:
				XMLErrChk(get_XMLElement_childvalues(&xml_element, value, RESREP_ELEMENT_CHILDS));
				nullChk(element = init_ResetRepeatElement(whisker_script, value));
				break;
		}
		/* Set Panel Attributes */
		SetPanelAttribute(element->panel_handle, ATTR_TITLEBAR_VISIBLE, 0);
				
		/* Add this element into script_elements list */
		if (NULL == ListInsertItem(script_elements, &element, END_OF_LIST)) {
			DiscardPanel(element->panel_handle);
			LOG_MSG(0, "Error when inserting element into list\n");
			/* TODO: Free list */
			discard_script_elements(script_elements);
			OKfreeList(&script_elements, NULL);
			goto Error;
		}
		
		CVIXMLDiscardElement(xml_element);
		xml_element = NULL;
	}		

Error:
	CVIXMLDiscardElement(xml_element);
	CVIXMLDiscardElement(root_element);
	CVIXMLDiscardDocument(script_doc);
	DisplayActiveXErrorMessageOnFailure(errorInfo.error);
	return script_elements;
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
	cur_script->script_elements = convert_xml_to_list(whisker_script, 
								  				cur_script->xml_script.file_path);
	if (cur_script->script_elements == NULL) {
		return;	
	}
	cur_script->num_elements = ListNumItems(cur_script->script_elements);
	redraw_script_elements(cur_script);
	
	/* Undim element add functionality */
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptElement, ATTR_DIMMED, 0);
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptAdd, ATTR_DIMMED, 0);
	return;	
}

/*
 * Ask for file path 
 */
int
get_logfile_path(WhiskerScript_t *whisker_script, FILE *temp_handle)
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

inline void
free_thread_functionIDs(WScript_t *cur_script)
{
	CmtThreadFunctionID	*thread_functionid = NULL;
	
	while (ListNumItems(cur_script->thread_functionIDs)) {
		ListRemoveItem(cur_script->thread_functionIDs, &thread_functionid, 
					   									FRONT_OF_LIST);
		/* Wait for thread to complete */
		CmtWaitForThreadPoolFunctionCompletion(DEFAULT_THREAD_POOL_HANDLE, 
								*thread_functionid, 0);
		/* Release function ID */
		CmtReleaseThreadPoolFunctionID(DEFAULT_THREAD_POOL_HANDLE, *thread_functionid); 
		/* Free thread_functionID */
		OKfree(thread_functionid);
	}
	return;
}

/* Async thread to execute element runner function */
void CVICALLBACK
async_script_runner(void *thread_data)
{
	WhiskerScript_t	*whisker_script = ((AsyncThreadData_t *)thread_data)->whisker_script;
	ScriptElement_t	*element = ((AsyncThreadData_t *)thread_data)->element;
	int				*index = ((AsyncThreadData_t *)thread_data)->index;
	
	/* Call element runner function */
	element->runner_function(element, whisker_script, index);	/* XXX: Dangerous to modify index by the element */
	
	/* Free thread_data */
	OKfree(thread_data);
	return;
}

/* This function creates a thread and starts executing element specific
 * runner function.
 */
void
start_async_runner_function(ScriptElement_t *element, WhiskerScript_t *whisker_script, int *index)
{
INIT_ERR

	CmtThreadFunctionID	*thread_functionid = NULL;
	AsyncThreadData_t	*thread_data = NULL;
	WScript_t			*cur_script = &(whisker_script->cur_script);
	int					error = 0;
	
	nullChk(thread_functionid = (CmtThreadFunctionID *)calloc(1, sizeof(CmtThreadFunctionID)));
	nullChk(thread_data = (AsyncThreadData_t *)calloc(1, sizeof(AsyncThreadData_t)));
	thread_data->whisker_script = whisker_script;
	thread_data->element = element;
	thread_data->index= index;
	
	if (NULL == ListInsertItem(cur_script->thread_functionIDs, &thread_functionid, 
									   	END_OF_LIST)) {
		goto Error;
	}
	
	/* Create thread from a default thread pool and get thread id function */
	CmtScheduleThreadPoolFunction (DEFAULT_THREAD_POOL_HANDLE, async_script_runner, 
													   	thread_data, thread_functionid);
	return;
	
Error:
	OKfree(thread_functionid);
	OKfree(thread_data);
	MessagePopup("Script Run Error", "Failed to start asynchronous thread.");
	*index = cur_script->num_elements;
	return;
}

/* Plot Graphs */
inline void
plot_counter_graph(WhiskerScript_t *whisker_script)
{
	double	data[BAR_GRAPH_COUNTERS];
	double	total;
	WScript_t	*cur_script = &(whisker_script->cur_script);
	
	/* Calculate total counter values */
	total =  cur_script->stats.correct_lick + cur_script->stats.incorrect_rejection +
			 cur_script->stats.false_alarm + cur_script->stats.correct_rejection;
	LOG_MSG1(9, "Total counter value : %lf", total);
	
	data[0] = (cur_script->stats.correct_lick / total) * 100.00;
	data[1] = (cur_script->stats.correct_rejection / total) * 100.00;
	data[2] = (cur_script->stats.incorrect_rejection / total) * 100.00;
	data[3] = (cur_script->stats.false_alarm / total) * 100.00;
	LOG_MSG2(9, "Percentage : Correct Lick = %lf\tCorrect Rejection = %lf", data[0], data[1]);
	LOG_MSG2(9, "Percentage : Incorrect Rejection = %lf\tFalse Alarm = %lf", 
			 											data[2], data[3]);
	DeleteGraphPlot (whisker_script->main_panel_handle, ScriptPan_ScriptGraphCtr, 
					 								-1, VAL_DELAYED_DRAW);
	PlotY(whisker_script->main_panel_handle, ScriptPan_ScriptGraphCtr, data, 4, 
		  VAL_DOUBLE, VAL_VERTICAL_BAR, VAL_EMPTY_SQUARE, VAL_SOLID, 1, VAL_GREEN); 
	
	return;
}

/* Graph plotting thread that runs simultaneosly with the script
 */
void CVICALLBACK
plot_graphs(void *thread_data)
{
	WhiskerScript_t	*whisker_script = (WhiskerScript_t *)thread_data;
	WScript_t		*cur_script = &(whisker_script->cur_script);
	
	while (cur_script->run_status != STOPPED) { 
		plot_counter_graph(whisker_script);
		WHISKER_DELAY(GRAPH_PLOT_DELAY);
	}
	
	plot_counter_graph(whisker_script);
	return;
}

/* Continous lick detector thread. That runs simultaneosly with the script 
 */
void CVICALLBACK
continous_lick_detector(void *thread_data)
{
	WhiskerScript_t	*whisker_script = (WhiskerScript_t *)thread_data;
	Whisker_t		*whisker_m = (Whisker_t *)whisker_script->whisker_m;
	WScript_t		*cur_script = &(whisker_script->cur_script);
	int				input_value;
	FILE			*file_handle = NULL;
	SYSTEMTIME		dh;
	char			msg[LOG_MSG_LEN];
	
	/* Open temp file for writing stats */
	file_handle = fopen(LICK_DET_TEMP_FILE, "w+");
	if (file_handle == NULL) {
		MessagePopup("Lick Detector", "Continous lick detector failed to start!");
		return;
	}
	
	/* Write initial line */
	fprintf(file_handle, "\n\n=========Lick Detection=========\n");
	
	/* Delete old plot from the graph */
	ClearStripChart(whisker_script->main_panel_handle, ScriptPan_ScriptLickDetGraph);
	
	/* Detect lick upto current script does not stop.
	 * No Lock needed.
	 */
	while (cur_script->run_status != STOPPED) {
		input_value = get_input_value(&whisker_m->de_dev, 
								whisker_m->de_dev.IO_Channel[LICK_DET_CH]);
		if (input_value == 0) {	/* Lick Detected */
			/* Get current local date and time */
			GetLocalTime(&dh);
			sprintf(msg, "%d:%02d:%02d:%02d \tLick Detected\n", 
							 dh.wHour, dh.wMinute, dh.wSecond, dh.wMilliseconds);
			fprintf(file_handle, "%s", msg);
		}
		/* Plot a graph.
		 * Toggle input value because on graph, lick is 1 but actual returned value is
		 * 0
		 */
		input_value ^= (1 << 0);
		PlotStripChart(whisker_script->main_panel_handle, ScriptPan_ScriptLickDetGraph, 
									&input_value, 1, 0, 0, VAL_INTEGER);
		WHISKER_DELAY(INPUT_CHECK_DELAY);
	}
	
	fclose(file_handle);
	LOG_MSG(9, "Continous lick detector finished");
	return;
}	

inline void
reset_stats_UIcounters(WhiskerScript_t *whisker_script)
{
	SetCtrlVal(whisker_script->main_panel_handle,
					   		ScriptPan_ScriptAirPuffCtr, 0);
	SetCtrlVal(whisker_script->main_panel_handle,
					   		ScriptPan_ScriptDropInCtr, 0);
	SetCtrlVal(whisker_script->main_panel_handle,
					   		ScriptPan_ScriptDropOutCtr, 0);
	SetCtrlVal(whisker_script->main_panel_handle,
					   		ScriptPan_ScriptCorrectLickCtr, 0);
	SetCtrlVal(whisker_script->main_panel_handle,
					   		ScriptPan_ScriptIncorrectRejCtr, 0);
	SetCtrlVal(whisker_script->main_panel_handle,
					   		ScriptPan_ScriptFalseAlarmCtr, 0);
	SetCtrlVal(whisker_script->main_panel_handle,
					   		ScriptPan_ScriptCorrectRejCtr, 0);
	return;
}

inline void
init_stats(WStats_t *stats)
{
	stats->air_puff = 0;
	stats->drop_in = 0;
	stats->drop_out = 0;
	stats->correct_lick = 0;
	stats->incorrect_rejection = 0;
	stats->correct_rejection = 0;
	stats->false_alarm = 0;
	return;
}

inline void
save_experiment_info(WhiskerScript_t *whisker_script)
{
	Whisker_t	*whisker_m	= (Whisker_t *)whisker_script->whisker_m;
	WScript_t	*cur_script =  &(whisker_script->cur_script);
	char		msg[LOG_MSG_LEN + LOG_MSG_LEN] = { '\0' };
	SYSTEMTIME	dh;
	char		temp[LOG_MSG_LEN] = { '\0' };
	size_t		zaber_speed = 0;
	
	GetLocalTime(&dh);
	/* If experiment info is valid then print in the log file */
	if (whisker_m->exp_info.VALID_INFO == TRUE) {
		sprintf(msg, "\nName:\t\t\t%s\n"
					"Experiment Number:\t%s\n"
					"Training Number:\t%s\n"
					"Animal Number:\t\t%s\n"
					"Animal Age:\t\t%s\n"
					"Animal Weight:\t\t%lf\n"
					"Message:\t\t%s\n"
					"Date:\t\t\t%d/%d/%04d\n", whisker_m->exp_info.user_name,
					whisker_m->exp_info.exp_num, whisker_m->exp_info.training_num,
					whisker_m->exp_info.animal_num, whisker_m->exp_info.animal_age,
					whisker_m->exp_info.animal_weight, whisker_m->exp_info.extra_msg,
			   		dh.wDay, dh.wMonth, dh.wYear);
	} else {
		sprintf(msg, "\nName:\n"
					"Experiment Number:\n"
					"Message:\n"
					"Training Number:\n"
					"Animal Number:\n"
					"Animal Age:\n"
					"Animal Weight:\n"
			   		"Date:\t\t%d/%d/%04d\n",
					dh.wDay, dh.wMonth, dh.wYear);
	}
	
	/* Get the Zaber Speed */
	if (whisker_m->z_dev.VALID_DEVICE == TRUE) {
		zaber_speed = get_device_data(whisker_m->z_dev.port, ZABER_DEV, "maxspeed");	
	}
	
	/* Add zaber Speed and Script Name */
	sprintf(temp, "Script Name: \t\t%s\n"
					"Zaber Device: \t\t%u\n",
					(cur_script->xml_script.VALID_FILE ? 
					 			cur_script->xml_script.file_name : ""),
					zaber_speed);
	strcat(msg, temp);
	
	save_and_print_LogMsg(whisker_script, msg);
	return 0;	
}

/* Script Runner that runs current script */
int CVICALLBACK
script_runner(void *thread_data)
{
	WhiskerScript_t	*whisker_script = (WhiskerScript_t *)thread_data;
	Whisker_t		*whisker_m	= (Whisker_t *)whisker_script->whisker_m;
	WScript_t		*cur_script = &(whisker_script->cur_script);
	ScriptElement_t	*element = NULL;
	int				i = 1;
	int				value = 0;
	
	if (cur_script->script_elements == NULL || cur_script->num_elements <= 0) {
		MessagePopup("Script Run Error", "No Elements in script to run!");
		return -1;	
	}

	/* Get temporary log file */
	if (cur_script->log_file.VALID_FILE == TRUE) {
		/* XXX: Hack to truncate a file */
		fclose(cur_script->log_file.file_handle);
		cur_script->log_file.VALID_FILE = FALSE;
	}
	cur_script->log_file.file_handle = fopen(LOG_TEMP_FILE, "w+");
	if (cur_script->log_file.file_handle == NULL) {
		MessagePopup("Log file Error", "Failed to create temporary log file!");
		return -1;
	}
	cur_script->log_file.VALID_FILE = TRUE;
	
	/* Create list to store thread IDs for asynchronous elements */
	cur_script->thread_functionIDs = ListCreate(sizeof(CmtThreadFunctionID *));
	if (cur_script->thread_functionIDs == NULL) {
		/* Close log file and mark it invalid */
		fclose(cur_script->log_file.file_handle);
		/* Delete temp file */
		DeleteFile(LOG_TEMP_FILE);
		cur_script->log_file.VALID_FILE = FALSE;
		MessagePopup("Script Run Error", "Failed to create a thread ID list."
					 					"It could be out of memory issue");
		return -1;
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
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptSaveLog,
							ATTR_DIMMED, 1);
	
	/* DIM test panel & related tabs */
	SetPanelAttribute(whisker_m->whisker_ui.main_panel_handle, ATTR_DIMMED, 1);
	SetPanelAttribute(whisker_m->whisker_ui.tab_air_puff, ATTR_DIMMED, 1);
	SetPanelAttribute(whisker_m->whisker_ui.tab_drop_in, ATTR_DIMMED, 1);
	SetPanelAttribute(whisker_m->whisker_ui.tab_drop_out, ATTR_DIMMED, 1);
	/* Stop Lick Detector test panel timer */
	SetCtrlAttribute(whisker_m->whisker_ui.main_panel_handle, MainPanel_LickTimer, 
					 						ATTR_ENABLED, 0);
	
	/* UnDim Other Pause and Stop */
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptPause,
							ATTR_DIMMED, 0);
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptStop,
							ATTR_DIMMED, 0);
	
	/* Reset Log Box */
	log_lines_counter = 0;
	ResetTextBox (whisker_script->main_panel_handle, ScriptPan_ScriptLog, "");
	save_experiment_info(whisker_script);
	save_and_print_LogMsg(whisker_script, LOG_TITLE);
	save_and_print_LogMsg(whisker_script, LOG_HYPHEN);
	
	/* Init Stats */
	init_stats(&(cur_script->stats));
	reset_stats_UIcounters(whisker_script);
	CmtScheduleThreadPoolFunction (DEFAULT_THREAD_POOL_HANDLE, continous_lick_detector, 
													 whisker_script, NULL);
	
	/* Start Graph plotting thread */
	CmtScheduleThreadPoolFunction (DEFAULT_THREAD_POOL_HANDLE, plot_graphs, 
													 whisker_script, NULL);
	
	/* Seed random number generator */
	srand(time(NULL));
	
	/* XY State position has to be GO position initially */
	cur_script->xy_state = GO;
	
	CmtGetLock(cur_script->lock);
	cur_script->run_status = STARTED;
	while (i <= cur_script->num_elements && cur_script->run_status != STOPPED) {
		if (cur_script->run_status == PAUSED) {
			CmtReleaseLock(cur_script->lock);
			WHISKER_DELAY(100);
			CmtGetLock(cur_script->lock);
			continue;
		}
		CmtReleaseLock(cur_script->lock);
		
		/* Before calling runner function, just 
		element appropriately.
		 * Element Panel height is retrieved in init function and maintained
		 * in whisker_script. We could get the height in local variable
		 * but there is no issue in maintaining in global structure.
		 */
		SetPanelAttribute(whisker_script->container_panel_handle, ATTR_VSCROLL_OFFSET, 
			(whisker_script->element_panel_height + INTER_ELEMENT_SPACING) * (i-1));
		
		ListGetItem(cur_script->script_elements, &element, i);
		if (element->mode == ASYNC) {	/* Start Thread for this operation */
			start_async_runner_function(element, whisker_script, &i);	
		} else {						/* Synchronous: In the same script context */
			element->runner_function(element, whisker_script, &i);
		}
		i++;
		CmtGetLock(cur_script->lock);
	}
	
	cur_script->run_status = STOPPED;
	CmtReleaseLock(cur_script->lock);
	
	/* Wait for asychronous thread to complete, if poor user has not specified
	 * explicitely.
	 */
	free_thread_functionIDs(cur_script);
	OKfreeList(&cur_script->thread_functionIDs, NULL);
	
	/* Save stats */
	save_stats(whisker_script);
	
	/* Flush temp log file.
	 */
	fflush(cur_script->log_file.file_handle);
	
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
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptSaveLog,
							ATTR_DIMMED, 0);
	
	/* UnDim test panel and related tabs */
	SetPanelAttribute(whisker_m->whisker_ui.main_panel_handle, ATTR_DIMMED, 0);
	SetPanelAttribute(whisker_m->whisker_ui.tab_air_puff, ATTR_DIMMED, 0);
	SetPanelAttribute(whisker_m->whisker_ui.tab_drop_in, ATTR_DIMMED, 0);
	SetPanelAttribute(whisker_m->whisker_ui.tab_drop_out, ATTR_DIMMED, 0);
	/* Start Lick Detector timer */
	if (whisker_m->de_dev.handle != 0) {
		SetCtrlAttribute(whisker_m->whisker_ui.main_panel_handle, MainPanel_LickTimer, 
					 						ATTR_ENABLED, 1);
	}
							 
	/* Dim Other Pause and Stop */
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptPause,
							ATTR_DIMMED, 1);
	SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptStop,
							ATTR_DIMMED, 1);
	
	return 0;

}

/* Save Stats */
inline void
save_stats(WhiskerScript_t *whisker_script)
{
	WScript_t	*cur_script = &(whisker_script->cur_script);
	WStats_t	*stats = &(cur_script->stats);
	char		msg[LOG_MSG_LEN];
	
	sprintf(msg, "\nStatistics:\n"
				"\t\tAir Puff\t\t\t%u\n"
				"\t\tDrop IN\t\t\t%u\n"
				"\t\tDrop OUT\t\t%u\n"
				"\t\tCorrect Lick\t\t%u\n"
				"\t\tIncorrect Rejection\t\t\t%u\n"
				"\t\tCorrect Rejection\t\t\t%u\n"
				"\t\tFalse Alarm\t\t\t%u\n",
				stats->air_puff, stats->drop_in, stats->drop_out, 
				stats->correct_lick, stats->incorrect_rejection, stats->correct_rejection,
		   		stats->false_alarm);
				
	save_and_print_LogMsg(whisker_script, msg);
	return;
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
	sprintf(msg, "%d:%02d:%02d:%02d \t%s\n", dh.wHour, dh.wMinute, dh.wSecond, 
													dh.wMilliseconds, str);
	
	/* Write it to UI */
	SetCtrlVal(whisker_script->main_panel_handle, ScriptPan_ScriptLog, msg);
	
	/* If log file location is valid then write messages asynchronously */
	if (cur_script->log_file.VALID_FILE == TRUE) {
		fprintf(cur_script->log_file.file_handle, "%s", msg);
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
	int				table_index = 0;
	Position_t		*saved_position = NULL;
	int				ch;
	size_t			duration;
	
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
		
		if (element->MAGIC_NUM == ACTION) {	/* Action Element */
			action_element = (ActionElement_t *)element;
			/* Update the action from UI */
			GetCtrlVal(element->panel_handle, ActionEle_EleCommand, 
					   							(int *)&(action_element->action));
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
			
			if (action_element->action != TTLOUT) {
				action_element->IO_Channel = ch;
				action_element->duration = duration;
			
				/* Update UI */
				SetCtrlVal(element->panel_handle, ActionEle_EleIO_CH, ch);
				SetCtrlVal(element->panel_handle, ActionEle_EleDuration, duration);
			}
		} else if (element->MAGIC_NUM == SOUND) {		/* Sound Element */
			if (whisker_m->sound.VALID_FILE == FALSE) {	/* No valid file */
				MessagePopup("Import Settings", "No valid sound file found");
			} else {
				GetCtrlVal(whisker_m->whisker_ui.main_panel_handle, MainPanel_SoundFilePath,
						   		((SoundElement_t *)element)->file_path.file_path);
				((SoundElement_t *)element)->file_path.VALID_FILE = TRUE;
				SetCtrlVal(element->panel_handle, SoundEle_ElePath,
						   		((SoundElement_t *)element)->file_path.file_path);
			}
		} else if (element->MAGIC_NUM == XYMOVE) {	/* XY Movement element */
			/* TODO: Might not required. As we have given import functionality on the
			 * Element itself */
			
		} else if (element->MAGIC_NUM == ZMOVE) {	/* Z Movement Element */
			((ZMoveElement_t *)element)->IO_channel = 
										whisker_m->de_dev.IO_Channel[ZAXIS_MOVE_CH];
			/* Update UI */
			SetCtrlVal(element->panel_handle, ZMoveEle_EleIO_CH, 
					   	((ZMoveElement_t *)element)->IO_channel);
		}
	}
	
	return;
	
}

/* Get current values from UI and update Internal Structure.
 */
void
apply_changes(WScript_t	*cur_script) 
{
	size_t			num_elements = cur_script->num_elements;
	ScriptElement_t	*element = NULL;
	
	for (int i = 1; i <= num_elements; i++) {
		/* Get Element one by one */
		ListGetItem(cur_script->script_elements, &element, i);
		
		switch (element->MAGIC_NUM) {
			case START:
				apply_start_changes((StartElement_t *)element);
				break;
				
			case ACTION:
				apply_action_changes((ActionElement_t *)element);
				break;
				
			case CONDITION:
				apply_condition_changes((ConditionElement_t *)element);
				break;
				
			case REPEAT:
				apply_repeat_changes((RepeatElement_t *)element);
				break;
				
			case STOP:
				apply_stop_changes((StopElement_t *)element);
				break;
				
			case WAIT:
				apply_wait_changes((WaitElement_t *)element);
				break;
				
			case MESSAGE:
				apply_message_changes((MessageElement_t *)element);
				break;
				
			case SOUND:
				apply_sound_changes((SoundElement_t *)element);
				break;
				
			case XYMOVE:
				apply_xymove_changes((XYMoveElement_t *)element);
				break;
				
			case ZMOVE:
				apply_zmove_changes((ZMoveElement_t *)element);
				break;
				
			case JUMP:
				apply_jump_changes((JumpElement_t *)element);
				break;
				
			case XYCONDITION:
				apply_xycondition_changes((XYConditionElement_t *)element);
				break;
				
			case RESETREPEAT:
				apply_resetrepeat_changes((ResetRepeatElement_t *)element);
				break;
		}
		
	}
	return;
}

inline void
apply_start_changes(StartElement_t *start_element) 
{
	GetCtrlVal(start_element->base_class.panel_handle, StartEle_EleDelay, 
			   						&(start_element->delay));
	return;
}

inline void
apply_action_changes(ActionElement_t *action_element)
{
	GetCtrlVal(action_element->base_class.panel_handle, ActionEle_EleDuration, 
			   							&(action_element->duration));
	GetCtrlVal(action_element->base_class.panel_handle, ActionEle_EleCommand, 
			   							(int *)&(action_element->action));
	GetCtrlVal(action_element->base_class.panel_handle, ActionEle_EleIO_CH, 
			   							&(action_element->IO_Channel));
	GetCtrlVal(action_element->base_class.panel_handle, ActionEle_EleAsync, 
			   							&(action_element->base_class.mode));
	return;
}

inline void
apply_condition_changes(ConditionElement_t *cond_element)
{
	GetCtrlVal(cond_element->base_class.panel_handle, CondEle_EleIO_CH, 
			   							&(cond_element->IO_channel));
	GetCtrlVal(cond_element->base_class.panel_handle, CondEle_EleValue, 
			   							&(cond_element->value));
	GetCtrlVal(cond_element->base_class.panel_handle, CondEle_EleTrue, 
			   							&(cond_element->true_step));
	GetCtrlVal(cond_element->base_class.panel_handle, CondEle_EleFalse, 
			   							&(cond_element->false_step));
	GetCtrlVal(cond_element->base_class.panel_handle, CondEle_EleDuration, 
			   							&(cond_element->duration));
	GetCtrlVal(cond_element->base_class.panel_handle, CondEle_EleFullDuration, 
			   							&(cond_element->full_duration));
	return;
}

inline void
apply_repeat_changes(RepeatElement_t *repeat_element)
{
	GetCtrlVal(repeat_element->base_class.panel_handle, RepEle_EleNTimes, 
			   							&(repeat_element->ntimes));
	GetCtrlVal(repeat_element->base_class.panel_handle, RepEle_EleStep, 
			   							&(repeat_element->repeat_step));
	return;
}

inline void
apply_stop_changes(StopElement_t *stop_element)
{
	GetCtrlVal(stop_element->base_class.panel_handle, StopEle_EleDelay, 
			   							&(stop_element->delay));
	return;
}

inline void
apply_wait_changes(WaitElement_t *wait_element)
{
	GetCtrlVal(wait_element->base_class.panel_handle, WaitEle_EleDelay, 
			   							&(wait_element->delay));
	return;
}

inline void
apply_message_changes(MessageElement_t *message_element)
{
	GetCtrlVal(message_element->base_class.panel_handle, MsgEle_EleText, 
			   							message_element->text);
	return;
}

inline void
apply_sound_changes(SoundElement_t *sound_element)
{
	GetCtrlVal(sound_element->base_class.panel_handle, SoundEle_ElePath, 
			   							sound_element->file_path.file_path);
	GetCtrlVal(sound_element->base_class.panel_handle, SoundEle_EleAsync, 
			   							&(sound_element->base_class.mode));
	sound_element->file_path.VALID_FILE = TRUE;
	return;
}

inline void
apply_xymove_changes(XYMoveElement_t *xymove_element) 
{
	GetCtrlVal(xymove_element->base_class.panel_handle, XYMoveEle_EleAsync,
			   					&(xymove_element->base_class.mode));
	return;	
}

inline void
apply_zmove_changes(ZMoveElement_t *zmove_element)
{
	GetCtrlVal(zmove_element->base_class.panel_handle, ZMoveEle_EleIO_CH, 
			   							&(zmove_element->IO_channel));
	GetCtrlVal(zmove_element->base_class.panel_handle, ZMoveEle_EleCommand, 
			   							(int *)&(zmove_element->dir));
	return;
}

inline void
apply_jump_changes(JumpElement_t *jump_element)
{
	GetCtrlVal(jump_element->base_class.panel_handle, JumpEle_EleStep,
			   							(size_t *)&(jump_element->step));
	return;
}

inline void
apply_xycondition_changes(XYConditionElement_t *xycond_element)
{
	GetCtrlVal(xycond_element->base_class.panel_handle, XYCondEle_EleTrue,
			   							(size_t *)&(xycond_element->true_step));
	GetCtrlVal(xycond_element->base_class.panel_handle, XYCondEle_EleFalse,
			   							(size_t *)&(xycond_element->false_step));
	GetCtrlVal(xycond_element->base_class.panel_handle, XYCondEle_EleValue,
			   							&(xycond_element->value));
	return;	
}

inline void
apply_resetrepeat_changes(ResetRepeatElement_t *resrep_element)
{
	GetCtrlVal(resrep_element->base_class.panel_handle, ResRepEle_EleStep,
			   							(size_t *)&(resrep_element->step));
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
		start_element->delay = (size_t)atoi(value[1]);	/* 0th index for delay */
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
		action_element->action = (Action_t)atoi(value[1]);
		action_element->duration = (size_t)atoi(value[2]);
		action_element->IO_Channel = atoi(value[3]);
		action_element->base_class.mode = atoi(value[4]);
		
		/* Update UI */
		SetCtrlVal(action_element->base_class.panel_handle, ActionEle_EleCommand, 
				   								action_element->action);
		SetCtrlVal(action_element->base_class.panel_handle, ActionEle_EleDuration,
				  								action_element->duration);
		SetCtrlVal(action_element->base_class.panel_handle, ActionEle_EleIO_CH,
				  								action_element->IO_Channel);
		SetCtrlVal(action_element->base_class.panel_handle, ActionEle_EleAsync,
				  								action_element->base_class.mode);
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
		condition_element->true_step = (size_t)atoi(value[3]);
		condition_element->false_step = (size_t)atoi(value[4]);
		condition_element->duration = (size_t)atoi(value[5]);
		condition_element->full_duration = atoi(value[6]);
		
		/* Update UI */
		SetCtrlVal(condition_element->base_class.panel_handle, CondEle_EleIO_CH, 
				   								condition_element->IO_channel);
		SetCtrlVal(condition_element->base_class.panel_handle, CondEle_EleValue,
				  								condition_element->value);
		SetCtrlVal(condition_element->base_class.panel_handle, CondEle_EleTrue,
				  								condition_element->true_step);
		SetCtrlVal(condition_element->base_class.panel_handle, CondEle_EleFalse,
				  								condition_element->false_step);
		SetCtrlVal(condition_element->base_class.panel_handle, CondEle_EleDuration,
				   								condition_element->duration);
		SetCtrlVal(condition_element->base_class.panel_handle, CondEle_EleFullDuration,
				   								condition_element->full_duration);
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
		repeat_element->ntimes = (size_t)atoi(value[1]);
		repeat_element->repeat_step = (size_t)atoi(value[2]);
		
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
		stop_element->delay = (size_t)atoi(value[1]);	/* 0th index for delay */
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
		wait_element->delay = (size_t)atoi(value[1]);	/* 0th index for delay */
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
		sound_element->base_class.mode = atoi(value[2]);
		
		/* Update UI */
		SetCtrlVal(sound_element->base_class.panel_handle, SoundEle_ElePath,
				   							UNCHECKED(sound_element->file_path.file_path));
		SetCtrlVal(sound_element->base_class.panel_handle, SoundEle_EleAsync,
				   							sound_element->base_class.mode);
		sound_element->file_path.VALID_FILE = TRUE;
	}
	
	return (ScriptElement_t *)sound_element;
}

ScriptElement_t *
init_XYMoveElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE])
{
	XYMoveElement_t *xymove_element = (XYMoveElement_t *)init_Element(whisker_script, 
												sizeof(XYMoveElement_t),
									 			whisker_script->xymoveElement_panel_handle);
	
	if (xymove_element == NULL) {
		return NULL;
	}
	
	xymove_element->base_class.MAGIC_NUM = XYMOVE;
	xymove_element->base_class.runner_function = xymove_runner;
	
	/* Init Saved position List */
	xymove_element->saved_positions = ListCreate(sizeof(Position_t *));
	
	if (value != NULL) {
		xymove_element->base_class.mode = atoi(value[1]);
		
		/* Update UI */
		SetCtrlVal(xymove_element->base_class.panel_handle, XYMoveEle_EleAsync,
				   							xymove_element->base_class.mode);
	}
	return (ScriptElement_t *)xymove_element;
}

ScriptElement_t *
init_ZMoveElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE])
{
	ZMoveElement_t *zmove_element = (ZMoveElement_t *)init_Element(whisker_script, 
												sizeof(ZMoveElement_t),
									 			whisker_script->zmoveElement_panel_handle);
	
	if (zmove_element == NULL) {
		return NULL;
	}
	
	zmove_element->base_class.MAGIC_NUM = ZMOVE;
	zmove_element->base_class.runner_function = zmove_runner;
	
	if (value != NULL) {
		 zmove_element->IO_channel = atoi(value[1]);
		 zmove_element->dir = (ZDirection_t)atoi(value[2]);
		 
		 /* Update UI */
		 SetCtrlVal(zmove_element->base_class.panel_handle, ZMoveEle_EleIO_CH,
														zmove_element->IO_channel);
		 SetCtrlVal(zmove_element->base_class.panel_handle, ZMoveEle_EleCommand,
														zmove_element->dir);
	}
	
	return (ScriptElement_t *)zmove_element;
}

ScriptElement_t *
init_JumpElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE])
{
	JumpElement_t *jump_element = (JumpElement_t *)init_Element(whisker_script, 
												sizeof(JumpElement_t),
									 			whisker_script->jumpElement_panel_handle);
	
	if (jump_element == NULL) {
		return NULL;
	}
	
	jump_element->base_class.MAGIC_NUM = JUMP;
	jump_element->base_class.runner_function = jump_runner;
	
	if (value != NULL) {
		 jump_element->step = (size_t)atoi(value[1]);
		 
		 /* Update UI */
		 SetCtrlVal(jump_element->base_class.panel_handle, JumpEle_EleStep,
														jump_element->step);
	}
	
	return (ScriptElement_t *)jump_element;
}

ScriptElement_t *
init_THWaitElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE])
{
	THWaitElement_t *thwait_element = (THWaitElement_t *)init_Element(whisker_script, 
												sizeof(THWaitElement_t),
									 			whisker_script->thwaitElement_panel_handle);
	
	if (thwait_element == NULL) {
		return NULL;
	}
	
	thwait_element->base_class.MAGIC_NUM = THWAIT;
	thwait_element->base_class.runner_function = thwait_runner;
	
	/* This element do have value */
	
	return (ScriptElement_t *)thwait_element;
}

ScriptElement_t *
init_XYConditionElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE])
{
	XYConditionElement_t *xycond_element = (XYConditionElement_t *)init_Element(whisker_script, 
												sizeof(XYConditionElement_t),
									 			whisker_script->xycondElement_panel_handle);
	
	if (xycond_element == NULL) {
		return NULL;
	}
	
	xycond_element->base_class.MAGIC_NUM = XYCONDITION;
	xycond_element->base_class.runner_function = xycondition_runner;
	
	if (value != NULL) {
		 xycond_element->true_step = (size_t)atoi(value[1]);
		 xycond_element->false_step = (size_t)atoi(value[2]);
		 xycond_element->value = (size_t)atoi(value[3]);
		 
		 /* Update UI */
		 SetCtrlVal(xycond_element->base_class.panel_handle, XYCondEle_EleTrue,
														xycond_element->true_step);
		 SetCtrlVal(xycond_element->base_class.panel_handle, XYCondEle_EleFalse,
														xycond_element->false_step);
		 SetCtrlVal(xycond_element->base_class.panel_handle, XYCondEle_EleValue,
														xycond_element->value);
	}
	
	return (ScriptElement_t *)xycond_element;
}

ScriptElement_t *
init_ResetRepeatElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE])
{
	ResetRepeatElement_t *resrep_element = (ResetRepeatElement_t *)init_Element(whisker_script, 
												sizeof(ResetRepeatElement_t),
									 			whisker_script->resrepElement_panel_handle);
	
	if (resrep_element == NULL) {
		return NULL;
	}
	
	resrep_element->base_class.MAGIC_NUM = RESETREPEAT;
	resrep_element->base_class.runner_function = resetrepeat_runner;
	
	if (value != NULL) {
		 resrep_element->step = (size_t)atoi(value[1]);
		 
		 /* Update UI */
		 SetCtrlVal(resrep_element->base_class.panel_handle, ResRepEle_EleStep,
														resrep_element->step);
	}
	
	return (ScriptElement_t *)resrep_element;
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

/* Update XY positions from variable list of elements
 */
static int
load_xymove_positions(CVIXMLElement *par_element, XYMoveElement_t *xymove_element)
{
	HRESULT			error = S_OK;
	int				num_XMLelements = 0;
	CVIXMLElement	position_element = NULL;	
	CVIXMLAttribute	x_attribute = NULL, y_attribute = NULL, 
					percent_attribute = NULL, gostate_attribute = NULL;
	char			attr_value[XML_ELEMENT_VALUE];
	Position_t		*saved_position = NULL;
	
	XMLErrChk(CVIXMLGetNumChildElements(*par_element, &num_XMLelements));
	if (num_XMLelements > XYMOVE_ELEMENT_CHILDS) {	/* Not Required */
		for (int i = XYMOVE_ELEMENT_CHILDS; i < num_XMLelements; i++) { 
			XMLErrChk(CVIXMLGetChildElementByIndex(*par_element, i, &position_element));
			XMLErrChk(CVIXMLGetAttributeByName(position_element, X_ATTR, &x_attribute));
			XMLErrChk(CVIXMLGetAttributeByName(position_element, Y_ATTR, &y_attribute));
			XMLErrChk(CVIXMLGetAttributeByName(position_element, PERCENT_ATTR, &percent_attribute));
			XMLErrChk(CVIXMLGetAttributeByName(position_element, GOSTATE_ATTR, &gostate_attribute));
		
			/* Allocate memory */
			saved_position = (Position_t *)malloc(sizeof(Position_t));
			if (saved_position == NULL) {
				CVIXMLDiscardElement(position_element);
				CVIXMLDiscardAttribute(x_attribute);
				CVIXMLDiscardAttribute(y_attribute);
				CVIXMLDiscardAttribute(percent_attribute);
				CVIXMLDiscardAttribute(gostate_attribute);
				/* TODO: Check whether to discard attributes or not */
				goto Error;	
			}
			
			/* Get X value */
			XMLErrChk(CVIXMLGetAttributeValue(x_attribute, attr_value));
			RemoveSurroundingWhiteSpace(attr_value);
			saved_position->X = (size_t)atoi(attr_value);
			/* Get Y value */
			XMLErrChk(CVIXMLGetAttributeValue(y_attribute, attr_value));
			RemoveSurroundingWhiteSpace(attr_value);
			saved_position->Y = (size_t)atoi(attr_value);
			/* Get Percent Value */
			XMLErrChk(CVIXMLGetAttributeValue(percent_attribute, attr_value));
			RemoveSurroundingWhiteSpace(attr_value);
			saved_position->percent = (size_t)atoi(attr_value);
			/* Get Go_State Value */
			XMLErrChk(CVIXMLGetAttributeValue(gostate_attribute, attr_value));
			RemoveSurroundingWhiteSpace(attr_value);
			saved_position->go_state = atoi(attr_value);
			
			CVIXMLDiscardElement(position_element);
			CVIXMLDiscardAttribute(x_attribute);
			CVIXMLDiscardAttribute(y_attribute);
			CVIXMLDiscardAttribute(percent_attribute);
			CVIXMLDiscardAttribute(gostate_attribute);
			
			/* Add saved position into list */
			if (NULL == ListInsertItem(xymove_element->saved_positions, &saved_position, 
											   	END_OF_LIST)) {
				OKfree(saved_position);
				goto Error;
			}
		}
	}

Error:
	DisplayActiveXErrorMessageOnFailure(error);
	return error;
}


/* Create element and append it to existing element but with attribute
 * list.
 */
static int
append_child_with_attributes(CVIXMLElement *par_element, char *tag[], char value[][XML_ELEMENT_VALUE], int index)
{
	CVIXMLElement	xml_element;
	HRESULT	error = S_OK;
	int		i = 1;
	
	XMLErrChk(CVIXMLNewElement(*par_element, index, tag[0], &xml_element));
	
	while (tag[i] != NULL) {
		XMLErrChk(CVIXMLAddAttribute(xml_element, tag[i], value[i]));
		i++;
	}
	
Error:
	CVIXMLDiscardElement(xml_element);
	DisplayActiveXErrorMessageOnFailure(error);
	return error;	
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
		XMLErrChk(CVIXMLGetElementValue(child_element, xml_value));
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
	
	/* Log message to file and text box */
	sprintf(log_msg, "Start :\tDuration %u ms", start_element->delay);
	save_and_print_LogMsg((WhiskerScript_t *)whisker_script, log_msg);
	
	SetCtrlVal(start_element->base_class.panel_handle, StartEle_LED, 1);
	
	WHISKER_DELAY(start_element->delay);

	SetCtrlVal(start_element->base_class.panel_handle, StartEle_LED, 0);
	return;
}

void CVICALLBACK
action_runner(void *element, void *whisker_script, int *index)
{
	Whisker_t		*whisker_m = (Whisker_t *)
							((WhiskerScript_t *)whisker_script)->whisker_m;
	WScript_t		*cur_script = &(((WhiskerScript_t *)whisker_script)->cur_script);
	ActionElement_t	*action_element = (ActionElement_t *)element;
	char			log_msg[LOG_MSG_LEN];
	
	/* Log message to file and text box */
	sprintf(log_msg, "Action :\t %s Duration %u ms", 
						Action_String[(int)action_element->action], 
						action_element->duration);
	save_and_print_LogMsg((WhiskerScript_t *)whisker_script, log_msg);
	
	/* Stat collection */
	switch (action_element->action) {
		case AIRPUFF:
			cur_script->stats.air_puff++;
			/* UPdate UI counters */
			SetCtrlVal(((WhiskerScript_t *)whisker_script)->main_panel_handle,
					   		ScriptPan_ScriptAirPuffCtr, cur_script->stats.air_puff);
			break;
			
		case DROPOUT:
			cur_script->stats.drop_out++;
			/* Update UI counter */
			SetCtrlVal(((WhiskerScript_t *)whisker_script)->main_panel_handle,
					   		ScriptPan_ScriptDropOutCtr, cur_script->stats.drop_out);
			break;
					
		case DROPIN:
			cur_script->stats.drop_in++;
			/* Update UI counter */
			SetCtrlVal(((WhiskerScript_t *)whisker_script)->main_panel_handle,
					   		ScriptPan_ScriptDropInCtr, cur_script->stats.drop_in);
			break;
	}
	
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
	
	/* Log message to file and text box */
	sprintf(log_msg, "Stop :\tDuration %u ms", stop_element->delay);
	save_and_print_LogMsg((WhiskerScript_t *)whisker_script, log_msg);
	
	SetCtrlVal(stop_element->base_class.panel_handle, StopEle_LED, 1);
	WHISKER_DELAY(stop_element->delay);
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
	
	/* Log message to file and text box */
	sprintf(log_msg, "Wait :\tDuration %u ms", wait_element->delay);
	save_and_print_LogMsg((WhiskerScript_t *)whisker_script, log_msg);
	
	SetCtrlVal(wait_element->base_class.panel_handle, WaitEle_LED, 1);
	WHISKER_DELAY(wait_element->delay);
	SetCtrlVal(wait_element->base_class.panel_handle, WaitEle_LED, 0);
	return;		
}

void
condition_runner(void *element, void *whisker_script, int *index)
{
	Whisker_t			*whisker_m = (Whisker_t *)
									((WhiskerScript_t *)whisker_script)->whisker_m;
	WScript_t			*cur_script = &(((WhiskerScript_t *)whisker_script)->cur_script);
	ConditionElement_t	*cond_element = (ConditionElement_t *)element;
	char				log_msg[LOG_MSG_LEN];
	int					input_value;
	double				start_time = 0, elapsed_time = 0;
	int					condition_true = 0;
	
	SetCtrlVal(cond_element->base_class.panel_handle, CondEle_LED, 1);
	
	/* Test input on cond_element->IO_channel */
	start_time = Timer();
	while (elapsed_time < (double)cond_element->duration) {
	 	input_value = get_input_value(&whisker_m->de_dev, cond_element->IO_channel);
	 	if (cond_element->value == input_value) {
			*index = cond_element->true_step;
			/* Lick detection channel */
			if (!condition_true && whisker_m->de_dev.IO_Channel[LICK_DET_CH] == 
										cond_element->IO_channel &&
			   					cond_element->value == 0) {
				if (cur_script->xy_state == GO) {	/* Correct Lick */
					cur_script->stats.correct_lick++;
					/* Update UI counter */
					SetCtrlVal(((WhiskerScript_t *)whisker_script)->main_panel_handle,
					   		ScriptPan_ScriptCorrectLickCtr, cur_script->stats.correct_lick);
				} else {							/* False Alarm */
					cur_script->stats.false_alarm++;
					/* Update UI counter */
					SetCtrlVal(((WhiskerScript_t *)whisker_script)->main_panel_handle,
					   	ScriptPan_ScriptFalseAlarmCtr, cur_script->stats.false_alarm);
				}
			}
			condition_true = 1;
			if (!cond_element->full_duration) {	/* User has asked to run the condition for full duration */
				break;
			}
	 	}
	 	WHISKER_DELAY(INPUT_CHECK_DELAY);	/* 1 ms delay */
	 	elapsed_time = (Timer() - start_time) * 1000.00;
	}
	LOG_MSG1(9, "Elapsed time in condition runner %lf", elapsed_time);
	 
	if (condition_true == 0) {
		*index = cond_element->false_step;
		/* Lick detection channel */
		if (whisker_m->de_dev.IO_Channel[LICK_DET_CH] == 
										cond_element->IO_channel &&
		   			cond_element->value == 0) {
			if (cur_script->xy_state == GO) {	/* No Lick */
				cur_script->stats.incorrect_rejection++;
				/* Update UI counter */
				SetCtrlVal(((WhiskerScript_t *)whisker_script)->main_panel_handle,
					ScriptPan_ScriptIncorrectRejCtr, cur_script->stats.incorrect_rejection);
			} else {							/* Correct Rejection */
				cur_script->stats.correct_rejection++;
				/* Update UI */
				SetCtrlVal(((WhiskerScript_t *)whisker_script)->main_panel_handle,
					ScriptPan_ScriptCorrectRejCtr, cur_script->stats.correct_rejection);
			}
		}
	}
	 *index = *index - 1;	/* Decrement by one because, script runner increments it */
	
	/* Log message to file and text box */
	sprintf(log_msg, "Condition :\tI/O Channel %d Value %d goto step %d", 
						(cond_element->IO_channel + 1), cond_element->value, *index + 1);
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
	
	SetCtrlVal(repeat_element->base_class.panel_handle, RepEle_LED, 1);
	if (repeat_element->counter < repeat_element->ntimes) {
		*index = repeat_element->repeat_step - 1; 	/* Decrement by one because script runner
													 * increments it by one */
		repeat_element->counter++;
		
		/* Log message to file and text box */
		sprintf(log_msg, "Repeat (%u/%u) : \tStep %u\t", repeat_element->counter, 
									repeat_element->ntimes, repeat_element->repeat_step); 
		save_and_print_LogMsg((WhiskerScript_t *)whisker_script, log_msg);
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
		sndPlaySound(sound_element->file_path.file_path, SYNC);
	}
	SetCtrlVal(sound_element->base_class.panel_handle, SoundEle_LED, 0);
	return;
}

/* Return Random index within a list of saved positions
 * based on percentage associated with them.
 */
int
get_random_position(XYMoveElement_t	*xymove_element)
{
	int			random_number = 0;
	Position_t	*saved_position = NULL;
	int			base_percent = 0;
	
	random_number = (rand() % 100) + 1;
	for (int i = 1; i <= ListNumItems(xymove_element->saved_positions); i++) {
		ListGetItem(xymove_element->saved_positions, &saved_position, i);
		if (random_number > base_percent && 
				random_number <= (base_percent + saved_position->percent)) {
			/* Within a Range */
			return i;
		}
		base_percent += saved_position->percent;
		if (base_percent > 100) {	/* Poor User */
			MessagePopup("Percentage Error", "Cumulative Percentage should not"
						 	"be greater than 100%");
			return 0;
		}
	}
	
	return 0;
}

void
xymove_runner(void *element, void *whisker_script, int *index)
{
	Whisker_t		*whisker_m = (Whisker_t *)
							((WhiskerScript_t *)whisker_script)->whisker_m;
	XYMoveElement_t	*xymove_element = (XYMoveElement_t *)element;
	Position_t		*saved_position = NULL;
	char			log_msg[LOG_MSG_LEN];
	int				pos = 0;

	pos = get_random_position(xymove_element);
	
	SetCtrlVal(xymove_element->base_class.panel_handle, XYMoveEle_LED, 1);
	if (pos == 0) {		/* No Position is selected */
		sprintf(log_msg, "XY Movement :\t NO Position Selected");
	} else {
		ListGetItem(xymove_element->saved_positions, &saved_position, pos);
		/* Do element specific operation */
		send_MoveABS_cmd(whisker_m->z_dev.port, ZABER_X_DEV, saved_position->X, 
						 								SYNC);
		send_MoveABS_cmd(whisker_m->z_dev.port, ZABER_Y_DEV, saved_position->Y,
														SYNC);
		sprintf(log_msg, "XY Movement :\tX = %u, \tY = %u", 
										saved_position->X, saved_position->Y);
	}
	save_and_print_LogMsg((WhiskerScript_t *)whisker_script, log_msg);
	SetCtrlVal(xymove_element->base_class.panel_handle, XYMoveEle_LED, 0);
	return;	
}

void
zmove_runner(void *element, void *whisker_script, int *index)
{
	Whisker_t		*whisker_m = (Whisker_t *)
							((WhiskerScript_t *)whisker_script)->whisker_m;
	ZMoveElement_t	*zmove_element = (ZMoveElement_t *)element;
	char			log_msg[LOG_MSG_LEN];
	
	sprintf(log_msg, "Z Movement :\tDirection = %s",
							(zmove_element->dir ? "UP" : "DOWN"));
	save_and_print_LogMsg((WhiskerScript_t *)whisker_script, log_msg);
	
	SetCtrlVal(zmove_element->base_class.panel_handle, ZMoveEle_LED, 1);
	/* Perform element specific operation */
	set_without_timer(&(whisker_m->de_dev), zmove_element->IO_channel,
					 						zmove_element->dir);
	SetCtrlVal(zmove_element->base_class.panel_handle, ZMoveEle_LED, 0);
	return;	
}

void
jump_runner(void *element, void *whisker_script, int *index) 
{
	Whisker_t		*whisker_m = (Whisker_t *)
							((WhiskerScript_t *)whisker_script)->whisker_m;
	JumpElement_t	*jump_element = (JumpElement_t *)element;
	char			log_msg[LOG_MSG_LEN];
	
	SetCtrlVal(jump_element->base_class.panel_handle, JumpEle_LED, 1); 
	sprintf(log_msg, "Jumping to STEP %u", jump_element->step);
	save_and_print_LogMsg((WhiskerScript_t *)whisker_script, log_msg);
	
	/* No need to validate step, as it is properly handled in script_runner */
	*index = jump_element->step - 1;	/* Decrement, because script runner increments it */
	SetCtrlVal(jump_element->base_class.panel_handle, JumpEle_LED, 0);
	return;
}

void
thwait_runner(void *element, void *whisker_script, int *index)
{
	THWaitElement_t	*thwait_element = (THWaitElement_t *)element;
	
	SetCtrlVal(thwait_element->base_class.panel_handle, THWaitEle_LED, 1);
	free_thread_functionIDs(&(((WhiskerScript_t *)whisker_script)->cur_script));
	SetCtrlVal(thwait_element->base_class.panel_handle, THWaitEle_LED, 0);
	return;
}

void
xycondition_runner(void *element, void *whisker_script, int *index)
{
	Whisker_t				*whisker_m = (Whisker_t *)
							((WhiskerScript_t *)whisker_script)->whisker_m;
	WScript_t				*cur_script = &(((WhiskerScript_t *)whisker_script)->cur_script);
	size_t					X, Y;
	ScriptElement_t			*temp_element = NULL;
	XYConditionElement_t	*xycond_element = (XYConditionElement_t *)element;
	XYMoveElement_t			*xymove_element = NULL;
	Position_t				*saved_position = NULL;
	int						is_true = FALSE;
	char					log_msg[LOG_MSG_LEN];
	
	SetCtrlVal(xycond_element->base_class.panel_handle, XYCondEle_LED, 1);
	X = get_device_data(whisker_m->z_dev.port, ZABER_X_DEV, "pos");	/* Get current X position */
	Y = get_device_data(whisker_m->z_dev.port, ZABER_Y_DEV, "pos");	/* Get current Y position */
	
	LOG_MSG2(9, "XY Condition, X = %u and Y = %u\n", X, Y);
	
	/* 
	 * Iterate over all the script elements and if XY Move element is present then
	 * Iterate over all the saved_positions.
	 * TODO: It can be optimized
	 */
	for (int i = 1; i <= cur_script->num_elements; i++) {
		/* Get Element one by one */
		ListGetItem(cur_script->script_elements, &temp_element, i);
		
		if (temp_element->MAGIC_NUM == XYMOVE) {
			/* Iterate over all saved positions */
			xymove_element = (XYMoveElement_t *)temp_element;
			for (int j = 1; j <= ListNumItems(xymove_element->saved_positions); 
									j++) {
				ListGetItem(xymove_element->saved_positions, &saved_position, j);
				if (saved_position->go_state == xycond_element->value &&
						saved_position->X == X && saved_position->Y == Y) { /* True condition */
					is_true = TRUE;
					break;			
				}
			}
			if (is_true) {	/* True Condition */
				break;
			}
		}
	}
	
	if (is_true) {
		cur_script->xy_state = xycond_element->value;
		*index = xycond_element->true_step;	
	} else {
		cur_script->xy_state = xycond_element->value ^ (1 << 0);
		*index = xycond_element->false_step;
	}
	*index = *index - 1;	/* Decrement by one because, script runner increments it */
	
	sprintf(log_msg, "XYCondition Position:\t %s goto step %u\t", 
								(cur_script->xy_state ? "GO" : "NO_GO"), *index + 1);
	save_and_print_LogMsg((WhiskerScript_t *)whisker_script, log_msg);
	SetCtrlVal(xycond_element->base_class.panel_handle, XYCondEle_LED, 0);
	return;
}

void
resetrepeat_runner(void *element, void *whisker_script, int *index) 
{
	Whisker_t				*whisker_m = (Whisker_t *)
							((WhiskerScript_t *)whisker_script)->whisker_m;
	WScript_t				*cur_script = &(((WhiskerScript_t *)whisker_script)->cur_script);
	ScriptElement_t			*repeat_element = NULL;
	ResetRepeatElement_t	*resrep_element = (ResetRepeatElement_t *)element;
	char					log_msg[LOG_MSG_LEN];
	
	SetCtrlVal(resrep_element->base_class.panel_handle, ResRepEle_LED, 1); 
	sprintf(log_msg, "Reset Repeat Element : %u", resrep_element->step);
	save_and_print_LogMsg((WhiskerScript_t *)whisker_script, log_msg);
	
	/* Get the repeat element from the list and reset its counter */
	ListGetItem(cur_script->script_elements, &repeat_element, resrep_element->step);
	if (repeat_element && repeat_element->MAGIC_NUM == REPEAT) {
		((RepeatElement_t *)repeat_element)->counter = 0; 
	}
	SetCtrlVal(resrep_element->base_class.panel_handle, ResRepEle_LED, 0);
	return;
}
	
