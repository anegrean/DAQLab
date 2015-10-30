/*GenericImageDisplay_type  <- clasa parinte = interfata ca sa cadem de acord

ImageDisplayCVI_type*/

// look in displayEngine .h and .c 


#include "ImageDisplay.h"

//==============================================================================
// Constants

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}

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

void init_GenericImageDisplay_type (GenericImageDisplay_type* 		ImageDisplay,
									DiscardFptr_type				imageDiscardFptr,
									DisplayImageFptr_type			displayImageFptr,
								 	CallbackGroup_type**			callbackGroupPtr)
{
	
	// DATA
	
	// METHODS
	
	ImageDisplay->imageDiscardFptr 			= imageDiscardFptr;
	ImageDisplay->displayImageFptr			= displayImageFptr;
	
	// CALLBACKS
	
	ImageDisplay->callbackGroup				= *callbackGroupPtr;
	*callbackGroupPtr						= NULL;
	
}
