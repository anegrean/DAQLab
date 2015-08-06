//==============================================================================
//
// Title:		Whisker.h
// Purpose:		A short description of the interface.
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

		/* Tab Index */
#define TAB_DROP_IN			0
#define TAB_DROP_OUT		1
#define TAB_AIR_PUFF		2
		
//==============================================================================
// Types

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
	//WhiskerScript_t		*whisker_script;		/* WhiskerScript builder structure */
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
