//==============================================================================
//
// Title:		ImageDisplay.h
// Purpose:		A short description of the interface.
//
// Created on:	16-7-2015 at 13:16:19 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __ImageDisplayCVI_H__
#define __ImageDisplayCVI_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "cvidef.h"
#include "DataTypes.h"
#include "ImageDisplay.h" 

//==============================================================================
// Constants

// pixel depth represents how many bits from the bitmapBitArray each pixel has. If pixelDepth is 24, each pixel is represented by a 24-bit RGB value of the form RRGGBB,
// where RR, GG, and BB represent the red, green, and blue components of the color value. The RR byte is at the lowest memory address of the three bytes.
// When pixelDepth is 24 or 32, valid RR, GG, and BB values range from 0 to 255.		
#define pixelDepth 32
#define BytesPerPixelDepth 4

//==============================================================================
// Types
		
typedef struct ImageDisplayCVI	ImageDisplayCVI_type; 

//=============================================================================
// External variables

//==============================================================================
// Global functions

ImageDisplayCVI_type* 		init_ImageDisplayCVI_type 		(	int    					parentPanHandl, 
												 				char   					displayTitle[],
												 				int						imgWidth,
												 				int						imgHeight,
												 				CallbackGroup_type**	callbackGroupPtr);


void						discard_ImageDisplayCVI_type	(ImageDisplayCVI_type** imageDisplayPtr);



#ifdef __cplusplus
    }
#endif

#endif  /* ndef __ImageDisplayCVI_H__ */
