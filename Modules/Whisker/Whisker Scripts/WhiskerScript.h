//==============================================================================
//
// Title:		WhiskerScript.h
// Purpose:		A short description of the interface.
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
#define	ELEMENT_NAME_LEN	 	32
#define INTER_ELEMENT_SPACING 	10
#define FILE_PATH_LEN			256

/* Element type of script */		
typedef enum {
	START,
	ACTION,
	CONDITION,
	REPEAT,
	STOP,
	WAIT
} ElementType_t;

/* Type of action to take */
typedef enum {
	AIRPUFF,
	DROPOUT,
	DROPIN
} Action_t;

/* Script Run Status */
typedef enum {
	STARTED,
	PAUSED,
	STOPPED
} RunStatus_t;

//==============================================================================
// Types

typedef struct {
	char	file_path[FILE_PATH_LEN];	/* XML file path */
	int		VALID_FILE;					/* If XML file path is valid */
	FILE	*file_handle;				/* File handle */
} FilePath_t;

/* Element Structure */
typedef struct {
	ElementType_t	MAGIC_NUM;										/* Identifies Element type */
	int				panel_handle;									/* UI its panel ID */
	void		(*runner_function)(void *element, 
								  void *whisker_script, 
								   int *index);
} ScriptElement_t;

typedef struct {
	ScriptElement_t	base_class;	/* Base Class */
	int				delay;		/* Delay for start operation */
} StartElement_t;				/* Start Element */

typedef struct {
	ScriptElement_t base_class;	/* Base Class */
	int				duration;	/* Duration of operation */
	Action_t		action;		/* Sub Action */
	int				IO_Channel;	/* IO channel */
} ActionElement_t; 				/* Action Element */

typedef struct {
	ScriptElement_t	base_class;	/* Base Class */
	int				IO_channel;	/* I/O channel to check input on */
	int				value;		/* ON/OFF, bool type */
	int				true_step;	/* step when condition is true */
	int				false_step; /* step when condition is false */
} ConditionElement_t; 			/* Condition Element */

typedef struct {
	ScriptElement_t base_class; 	/* Base Class */
	int				ntimes;			/* Repeat number of times */
	int				repeat_step; 	/* Repeat from step */
	int				counter;		/* Counter that starts from 1 to ntimes */
} RepeatElement_t; 					/* Repeat Element */

typedef struct {
	ScriptElement_t	base_class;	/* Base Class */
	int				delay;		/* Delay for stop operation */
} StopElement_t; 				/* Stop Element */

typedef struct {
	ScriptElement_t	base_class;	/* Base Class */
	int				delay;		/* Delay for wait operation */
} WaitElement_t; 				/* Wait Element */

/* Actual script which stores elements list */
typedef struct {
	ListType			script_elements;	/* Stores script elements */
	size_t				num_elements;		/* Stores number of elements */
	FilePath_t  		xml_script;			/* Stores xml specific variables */
	FilePath_t  		log_file;			/* Stores log file path */
	RunStatus_t			run_status;			/* Script run status */
	CmtThreadLockHandle	lock;				/* Lock to protect status variable */
} WScript_t;

typedef struct {
	void		*whisker_m;			/* Whisker Module Structure */
	WScript_t	cur_script;			/* Current Script */
	
	/* UI */
	int	main_panel_handle;			/* Main Script Panel */
	int	container_panel_handle; 	/* Container Script Panel */ 
	int	startElement_panel_handle;  /* Start Element Panel Handle */
	int	actionElement_panel_handle; /* Action Element Panel Handle */
	int	condElement_panel_handle;	/* Conditional Element Panel Handle */
	int	repeatElement_panel_handle; /* Repeat Element Panel Handle */
	int stopElement_panel_handle;	/* Stop Element Panel Handle */
	int waitElement_panel_handle;	/* Wait Element Panel Handle */
} WhiskerScript_t;
//==============================================================================
// External variables

//==============================================================================
// Global functions
int	init_display_script(void *function_data);
int discard_cur_script(WScript_t *cur_script);
int discard_script_module(WhiskerScript_t *whisker_script);
void redraw_script_elements(WScript_t *cur_script);
void save_script(WhiskerScript_t *whisker_script);
void load_script(WhiskerScript_t	*whisker_script);
int CVICALLBACK script_runner(void *thread_data);

ScriptElement_t* init_StartElement(WhiskerScript_t *whisker_script, int *value);
ScriptElement_t* init_ActionElement(WhiskerScript_t *whisker_script, int *value);
ScriptElement_t* init_RepeatElement(WhiskerScript_t *whisker_script, int *value);
ScriptElement_t* init_StopElement(WhiskerScript_t *whisker_script, int *value);
ScriptElement_t* init_ConditionElement(WhiskerScript_t *whisker_script, int *value);
ScriptElement_t* init_WaitElement(WhiskerScript_t *whisker_script, int *value);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __WhiskerScript_H__ */
