//==============================================================================
//
// Title:		WhiskerScript.h
// Purpose:		Whisker Script Header file.
//
// Created on:	25-7-2015 at 20:33:50 by Vinod Nigade.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __WhiskerScript_H__
#define __WhiskerScript_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

//==============================================================================
// Constants
#define MOD_WhiskerScript_UI 	"./Modules/Whisker/Whisker Scripts/UI_Scripts.uir"
#define LOG_TEMP_FILE			"log_temp.txt"
		
#define	XML_ELEMENT_VALUE		128		/* Message text OR 8 Number of digits in the value + '\0'*/
#define	ELEMENT_NAME_LEN	 	32
#define INTER_ELEMENT_SPACING 	10
#define FILE_PATH_LEN			128
#define FILE_NAME_LEN			64
#define MSG_LEN					128
#define	NEW_SCRIPT_NAME			"<New Script>"
#define NO_SCRIPT_NAME			"<No Script>"
#define NO_LOG_NAME				"<No Log File>"

/* Element type of script */		
typedef enum {
	START,
	ACTION,
	CONDITION,
	REPEAT,
	STOP,
	WAIT,
	MESSAGE,
	SOUND,
	XYMOVE,
	ZMOVE,
	JUMP,
	THWAIT
} ElementType_t;

/* Type of action to take */
typedef enum {
	AIRPUFF,
	DROPOUT,
	DROPIN,
	TTLOUT
} Action_t;

/* Script Run Status */
typedef enum {
	STARTED,
	PAUSED,
	STOPPED
} RunStatus_t;

/* ZMove Direction */
typedef enum {
	DOWN,
	UP
} ZDirection_t;

//==============================================================================
// Types

typedef struct {
	char	file_path[FILE_PATH_LEN];	/* XML file path */
	char	file_name[FILE_NAME_LEN];	/* Extracted file name */
	int		VALID_FILE;					/* If XML file path is valid */
	FILE	*file_handle;				/* File handle */
} FilePath_t;

/* Element Structure */
typedef struct {
	ElementType_t	MAGIC_NUM;								/* Identifies Element type */
	int				panel_handle;							/* UI its panel ID */
	int				mode;									/* Synchronous or asynchronous */
	void		(*runner_function)(void *element, 
								  void *whisker_script, 
								   int *index);
} ScriptElement_t;

typedef struct {
	ScriptElement_t	base_class;	/* Base Class */
	size_t			delay;		/* Delay for start operation */
} StartElement_t;				/* Start Element */

typedef struct {
	ScriptElement_t base_class;	/* Base Class */
	size_t			duration;	/* Duration of operation */
	Action_t		action;		/* Sub Action */
	int				IO_Channel;	/* IO channel */
} ActionElement_t; 				/* Action Element */

typedef struct {
	ScriptElement_t	base_class;	/* Base Class */
	int				IO_channel;	/* I/O channel to check input on */
	int				value;		/* ON/OFF, bool type */
	size_t			true_step;	/* step when condition is true */
	size_t			false_step; /* step when condition is false */
	size_t			duration;	/* Interval to detect condition */
} ConditionElement_t; 			/* Condition Element */

typedef struct {
	ScriptElement_t base_class; 	/* Base Class */
	size_t			ntimes;			/* Repeat number of times */
	size_t			repeat_step; 	/* Repeat from step */
	int				counter;		/* Counter that starts from 1 to ntimes */
} RepeatElement_t; 					/* Repeat Element */

typedef struct {
	ScriptElement_t	base_class;	/* Base Class */
	size_t			delay;		/* Delay for stop operation */
} StopElement_t; 				/* Stop Element */

typedef struct {
	ScriptElement_t	base_class;	/* Base Class */
	size_t			delay;		/* Delay for wait operation */
} WaitElement_t; 				/* Wait Element */

typedef struct {
	ScriptElement_t	base_class; 	/* Base Class */
	char			text[MSG_LEN];	/* Message String */
} MessageElement_t;				/* Message Element */

typedef struct {
	ScriptElement_t	base_class;		/* Base Class */
	FilePath_t		file_path;		/* File path */
} SoundElement_t;					/* Sound Element */

typedef struct {
	ScriptElement_t	base_class;		 /* Base Class */
	ListType		saved_positions; /* Saved Positions */
} XYMoveElement_t;					 /* XY Move Element */

typedef struct {
	ScriptElement_t	base_class;		/* Base Class */
	int				IO_channel;		/* IO channel */
	ZDirection_t	dir;			/* Direction of movement */
} ZMoveElement_t;					/* Z move Element */

typedef struct {
	ScriptElement_t	base_class;		/* Base Class */
	size_t			step;			/* Step to jump */
} JumpElement_t;					/* Jump Element */

typedef struct {
	ScriptElement_t base_class;		/* Base Class */
} THWaitElement_t;				/* Thread Wait element */
					
/* Actual script which stores elements list */
typedef struct {
	ListType			script_elements;	/* Stores script elements */
	size_t				num_elements;		/* Stores number of elements */
	FilePath_t  		xml_script;			/* Stores xml specific variables */
	FilePath_t  		log_file;			/* Stores log file path */
	RunStatus_t			run_status;			/* Script run status */
	CmtThreadLockHandle	lock;				/* Lock to protect status variable */
	ListType			thread_functionIDs;	/* Stores thread ids of asynchronous element */
} WScript_t;

typedef struct {
	void		*whisker_m;			/* Whisker Module Structure */
	WScript_t	cur_script;			/* Current Script */
	
	/* UI */
	int		main_panel_handle;			/* Main Script Panel */
	int		container_panel_handle; 	/* Container Script Panel */
	/* Element Panel */
	int		startElement_panel_handle;  /* Start Element Panel Handle */
	int		actionElement_panel_handle; /* Action Element Panel Handle */
	int		condElement_panel_handle;	/* Conditional Element Panel Handle */
	int		repeatElement_panel_handle; /* Repeat Element Panel Handle */
	int 	stopElement_panel_handle;	/* Stop Element Panel Handle */
	int 	waitElement_panel_handle;	/* Wait Element Panel Handle */
	int		msgElement_panel_handle;	/* Message Element Panel Handle */
	int		soundElement_panel_handle;	/* Sound Element Panel Handle */
	int		xymoveElement_panel_handle;	/* XY Move Panel Handle */
	int		zmoveElement_panel_handle;	/* Z Move Panel Handle */
	int		jumpElement_panel_handle;	/* Jump Element Panel Handle */
	int		thwaitElement_panel_handle;  /* Thread Wait Panel Handle */
	/* Other Panels */
	
	size_t	element_panel_height;		/* Height of script element panel */
} WhiskerScript_t;
//==============================================================================
// External variables

//==============================================================================
// Global functions
int	init_display_script(void *function_data);
void discard_script_elements(WScript_t *cur_script);
int discard_cur_script(WScript_t *cur_script);
int discard_script_module(WhiskerScript_t *whisker_script);
void redraw_script_elements(WScript_t *cur_script);
void save_script(WhiskerScript_t *whisker_script);
void load_script(WhiskerScript_t	*whisker_script);
void import_settings(WhiskerScript_t	*whisker_script);
int CVICALLBACK script_runner(void *thread_data);

ScriptElement_t* init_StartElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE]);
ScriptElement_t* init_ActionElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE]);
ScriptElement_t* init_RepeatElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE]);
ScriptElement_t* init_StopElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE]);
ScriptElement_t* init_ConditionElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE]);
ScriptElement_t* init_WaitElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE]);
ScriptElement_t* init_MessageElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE]);
ScriptElement_t* init_SoundElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE]);
ScriptElement_t* init_XYMoveElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE]);
ScriptElement_t* init_ZMoveElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE]);
ScriptElement_t* init_JumpElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE]);
ScriptElement_t* init_THWaitElement(WhiskerScript_t *whisker_script, char value[][XML_ELEMENT_VALUE]);

void apply_changes(WScript_t *cur_script);
extern inline void apply_start_changes(StartElement_t *start_element);
extern inline void apply_action_changes(ActionElement_t *action_element);
extern inline void apply_condition_changes(ConditionElement_t *condition_element);
extern inline void apply_repeat_changes(RepeatElement_t *repeat_element);
extern inline void apply_stop_changes(StopElement_t *stop_element);
extern inline void apply_wait_changes(WaitElement_t *wait_element);
extern inline void apply_message_changes(MessageElement_t *message_element);
extern inline void apply_sound_changes(SoundElement_t *sound_element);
extern inline void apply_xymove_changes(XYMoveElement_t *xymove_element);
extern inline void apply_zmove_changes(ZMoveElement_t *zmove_element);
extern inline void apply_jump_changes(JumpElement_t *zmove_element);
							 
#ifdef __cplusplus
    }
#endif

#endif  /* ndef __WhiskerScript_H__ */
