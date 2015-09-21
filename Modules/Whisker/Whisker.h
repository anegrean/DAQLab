//==============================================================================
//
// Title:		Whisker.h
// Purpose:		Whisker Module specific header file.
//
// Created on:	17-7-2015 at 14:17:06 by Vinod Nigade.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __Whisker_H__
#define __Whisker_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "DAQLabModule.h"
#include "stdint.h"
#include "Zaber.h"
#include "Deditec.h"
#include <mmsystem.h>

//==============================================================================
// Constants
#define MOD_Whisker_UI		"./Modules/Whisker/UI_Whisker.uir"
#define MOD_Whisker_Name 	"Rat Whisker"
#define FILE_PATH_LEN		256
#define NAME_LEN			64
#define MSG_LEN				128
#define INPUT_CHECK_DELAY	1		/* 1 ms between each input check detection */ 

		/* Tab Index */
#define TAB_DROP_IN			0
#define TAB_DROP_OUT		1
#define TAB_AIR_PUFF		2
		
/* XY Position TABLE */
#define	XYTABLE_X_COL			1		/* X column */ 
#define XYTABLE_Y_COL			2		/* Y column */
#define XYTABLE_PER_COL			3		/* Percentage Column */
#define XYTABLE_GO_COL			3		/* Go to position column */
#define XYTABLE_DEL_COL			4		/* Delete position column */
#define XYTABLE_GOSTATE_COL 	4		/* Go State column */
#define XYTABLE_SCRIPTDEL_COL 	5		/* Delete column in script element */
#define GO_LABEL				"-->"   /* Go button label */
#define DEL_LABEL				"X"		/* Delete button label */

/* Experiment Info Table */
#define EXPTABLE_NAME_COL		1		/* First column has info name */
#define EXPTABLE_VALUE_COL		2		/* Second column has info value */

#define WHISKER_DELAY(x)	Sleep(x)	/* Wait time */

//==============================================================================
// Types

/**
 * Experiment Information
 */
typedef struct {
	char	user_name[NAME_LEN];	/* User Name */
	char	exp_num[NAME_LEN];		/* Experiment Number */
	char	animal_num[NAME_LEN];	/* Animal Number */
	char	animal_age[NAME_LEN];	/* Animal Age */
	double	animal_weight;			/* Animal Weight */
	char	training_num[NAME_LEN];	/* Training Number */
	char	extra_msg[MSG_LEN];		/* Message to store */
	int		VALID_INFO;				/* Validity of this informantion */
} ExperimentInfo_t;

/**
 * Sound structure
 */
typedef struct {
	char	file_path[FILE_PATH_LEN];	/* File path */
	int		isSYNC;						/* SYNC or ASYNC mode */ 
	int		VALID_FILE;					/* Flag to check if file is selected */
} WhiskerSound_t;

/**
 * UI componants
 */
typedef struct {
	int	main_panel_handle;		/* UI: Main Panel */
	int	tab_air_puff;			/* Air Puff Tab page */
	int	tab_drop_in;			/* Drop IN Tab page */
	int	tab_drop_out;			/* Drop Out Tab Page */
	int	XYSetting_panel_handle; /* XY setting Panel handle */
	int experiment_panel_handle	/* Experiment Info Panel */
} WhiskerUI_t;

/**
 * Parent Whisker Module.
 */
typedef struct {
	DAQLabModule_type 	baseClass;  			/* Super Class */
	zaber_device_t		z_dev;					/* Zaber Device */
	delib_device_t		de_dev;					/* Deditec Device */
	WhiskerSound_t		sound;					/* Sound structure */
	WhiskerUI_t			whisker_ui;				/* UI related componants */
	ExperimentInfo_t	exp_info;				/* Experiment Information */
	//WhiskerScript_t		*whisker_script;	/* WhiskerScript builder structure */
} Whisker_t;

//==============================================================================
// External variables

//==============================================================================
// Global functions
DAQLabModule_type*	initalloc_WhiskerModule(DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl);
void 				discard_WhiskerModule(DAQLabModule_type** mod); 

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __Whisker_H__ */
