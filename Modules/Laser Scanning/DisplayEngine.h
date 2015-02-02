//==============================================================================
//
// Title:		DisplayEngine.h
// Purpose:		A short description of the interface.
//
// Created on:	2-2-2015 at 15:38:29 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __DisplayEngine_H__
#define __DisplayEngine_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "cvidef.h"
#include "DataTypes.h"

//==============================================================================
// Constants

//==============================================================================
// Types
		
typedef struct DisplayEngine	DisplayEngine_type;		// Generic display engine.

typedef void*					DisplayHandle_type;		// Display handle for one display window that can be used by
														// an external module to interact with the display.
//------------------
// ROIs
//------------------
	// Point
typedef struct {
	int x;
	int y;
} Point_type;

	// Rectangle
typedef struct {
	int top;
	int left;
	int height;
	int width;
} Rect_type;
							   
//==============================================================================
// Class to inherit from

struct DisplayEngine {
	//-------------------------------------
	// DATA
	//-------------------------------------
	
	
	//-------------------------------------
	// METHODS
	//-------------------------------------
	
	int							(* DisplayImage)			(DisplayEngine_type* displayEngine, DisplayHandle_type displayHandle, void* pixelArray, 
													 		 int imgHeight, int imgWidth, ImageTypes imageType);
	
	DisplayHandle_type			(* NewDisplayHandle)		(DisplayEngine_type* displayEngine, void* callbackData)
	
};

//==============================================================================
// External variables

//==============================================================================
// Global functions

//--------------------------------------------------------------------------------------------------------------------------
// Display Engine management
//--------------------------------------------------------------------------------------------------------------------------
DisplayEngine_type*				initalloc_DisplayEngine				(DisplayEngine_type* displayEngine);
void 							discard_DisplayEngine 				(DisplayEngine_type** displayEnginePtr);

//--------------------------------------------------------------------------------------------------------------------------
// ROI management
//--------------------------------------------------------------------------------------------------------------------------
	// Point
Point_type*						init_Point_type						(int x, int y);
void							discard_Point_type					(Point_type** pointPtr);

	// Rectangle
Rect_type*						init_Rect_type						(int top, int left, int height, int width);
void							discard_Rect_type					(Rect_type** rectPtr);


#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DisplayEngine_H__ */
