//==============================================================================
//
// Title:		DisplayEngine.c
// Purpose:		A short description of the implementation.
//
// Created on:	2-2-2015 at 15:38:29 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DisplayEngine.h"

//==============================================================================
// Constants

//==============================================================================
// Types

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables

//==============================================================================
// Global functions


void init_DisplayEngine_type (	DisplayEngine_type* 					displayEngine,
								DiscardDisplayEngineFptr_type			discardFptr,
								DisplayImageFptr_type					displayImageFptr,
								GetDisplayHandleFptr_type				getDisplayHandleFptr,
								GetDisplayHandleCBDataFptr_type			getDisplayHandleCBDataFptr,
								SetRestoreImgSettingsCBsFptr_type		setRestoreImgSettingsFptr, 
								OverlayROIFptr_type						overlayROIFptr,
								ClearAllROIFptr_type					clearAllROIFptr,
							 	ROIPlaced_CBFptr_type					ROIPlacedCBFptr,
								ROIRemoved_CBFptr_type					ROIRemovedCBFptr,
							 	ErrorHandlerFptr_type					errorHandlerCBFptr		)
{
	displayEngine->discardFptr 					= discardFptr;
	displayEngine->displayImageFptr 			= displayImageFptr; 
	displayEngine->getDisplayHandleFptr 		= getDisplayHandleFptr;
	displayEngine->getDisplayHandleCBDataFptr	= getDisplayHandleCBDataFptr;
	displayEngine->setRestoreImgSettingsFptr	= setRestoreImgSettingsFptr;
	displayEngine->overlayROIFptr				= overlayROIFptr;
	displayEngine->clearAllROIFptr				= clearAllROIFptr;
	displayEngine->ROIPlacedCBFptr				= ROIPlacedCBFptr;
	displayEngine->ROIRemovedCBFptr				= ROIRemovedCBFptr;
	displayEngine->errorHandlerCBFptr			= errorHandlerCBFptr;
}

void discard_DisplayEngine_type (DisplayEngine_type** displayEnginePtr)
{
	if (!*displayEnginePtr) return;
	
	// call child class dispose method
	(*(*displayEnginePtr)->discardFptr) ((void**)displayEnginePtr);
}

void discard_DisplayEngineClass (DisplayEngine_type** displayEnginePtr)
{
	
}
