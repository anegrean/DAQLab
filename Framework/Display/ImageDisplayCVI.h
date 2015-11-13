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

ImageDisplayCVI_type* init_ImageDisplayCVI_type (int    		parentPanHandl, 
												 char   		displayTitle[],
												 ImageTypes 	imageType,
												 int			imgWidth,
												 int			imgHeight,
												 void*			callbackData
);
						
void						discard_ImageDisplayCVI_type	(ImageDisplayCVI_type** imageDisplayPtr);

// Adds an image to display. Multiple images can be added to the same display and the user can make use of a scrollbar to view each image.
int							DisplayImage					(ImageDisplayCVI_type* imageDisp, Image_type* images[]);
unsigned char* 				initBitArray					(int nBits);



struct ImageDisplayCVI {
ImageDisplay_type baseClass; 
	//---------------------------------------------------------------------------------------------------------------
	// DATA
	//---------------------------------------------------------------------------------------------------------------
	
	ListType				images;				// List of Image_type* elements. Data in this image is used to update the display whenever needed. Note: do not modify this data directly!
	BOOL					update;				// If True, the plot will display each time the latest waveform added to it. If False, the user can use the scrollbar to return to a previous plot. Default: True.
	int						imgBitmapID;		// this is the handle to the image provided to the display canvas, it is updated from the "images" array, when needed.
	unsigned char* 			bitmapBitArray;		// Array of bits used to create the bitmap image
	int						nBytes;				// number of bits in the bitmapBitArray;
	int						imgHeight;			// image height
	int 					imgWidth;			// image width
	
	// UI
	int						displayPanHndl;
	int						canvasPanHndl;
	int						menuBarHndl;
	int						menuID_Close;
	int						menuID_Save;
	//int						scrollbarCtrl;
	
	// CALLBACK
	//WaveformDisplayCB_type	callbackFptr;
	void*					callbackData;
};


#ifdef __cplusplus
    }
#endif

#endif  /* ndef __ImageDisplayCVI_H__ */
