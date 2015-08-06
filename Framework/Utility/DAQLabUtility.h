//==============================================================================
//
// Title:		DAQLabUtility.h
// Purpose:		A short description of the interface.
//
// Created on:	14-8-2014 at 22:25:52 by Adrian.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

#ifndef __DAQLabUtility_H__
#define __DAQLabUtility_H__

#ifdef __cplusplus
    extern "C" {
#endif

#ifndef	_CVI_DEBUG_
#define	_CVI_DEBUG_	0
#endif
		
#ifndef	_DEBUG_LEVEL_
#define	_DEBUG_LEVEL_ 	-1
#endif
		
//==============================================================================
// Include files

#include "cvidef.h"

//==============================================================================
// Constants

//=====================================================		
// USAGE
//=====================================================
		
		
//-------------------------------------------------------------------------------
// Error checking macros
//-------------------------------------------------------------------------------

typedef struct {
	int			result;
	int			error;
	int			line;
	char*		errMsg;
} ErrorInfo_type;

#ifndef INIT_ERROR_INFO
#define INIT_ERROR_INFO	ErrorInfo_type errorInfo = {.result = 0, .error = 0, .line = 0, .errMsg = NULL};	
#endif

#ifdef errChk
#undef errChk
#endif
		
#define errChk(fCall) if (errorInfo.result = (fCall), errorInfo.line = __LINE__, errorInfo.result < 0) \
{errorInfo.error = errorInfo.result; goto Error;} else

#ifdef nullChk
#undef nullChk
#endif
		
#define nullChk(fCall) if (errorInfo.line = __LINE__, !(fCall)) \
{errorInfo.error = UIEOutOfMemory; goto Error;} else

//==============================================================================
// Macros

#define OKfree(ptr) 					if (ptr) {free(ptr); ptr = NULL;}
#define OKfreeCAHndl(objHandle)			if (objHandle) {CA_DiscardObjHandle(objHandle); objHandle = 0;}
#define OKfreePanHndl(panelHandle)  	if (panelHandle) {DiscardPanel(panelHandle); panelHandle = 0;}
#define OKfreeList(list) 				if (list) {ListDispose(list); list = 0;}  

#define NumElem(ptr) (sizeof(ptr)/sizeof(ptr[0]))	 // Note: do not use this inside a function to get the number of elements in an array passed as an argument!
												 
#define RETURN_ERROR_INFO \
	if (errorInfo.error < 0) \
		if (errorMsg) { \
			if (errorInfo.errMsg) \
				*errorMsg = FormatMsg(errorInfo.error, __FILE__, __func__, errorInfo.line, errorInfo.errMsg); \
			else \
				*errorMsg = FormatMsg(errorInfo.error, __FILE__, __func__, errorInfo.line, "Unknown or out of memory"); \
		} else \
			OKfree(errorInfo.errMsg); \
	return errorInfo.error;
	
					
/* Log Messages that is only printed when _CVI_DEBUG_ is set and it is within 
 * _DEBUG_LEVEL_
 */
#define LOG_MSG(level, f)		if (_CVI_DEBUG_ && level <= _DEBUG_LEVEL_) { 				\
									printf("(%s: %d) " f "\n", __FILE__, __LINE__); 		\
								}
													 
#define LOG_MSG1(level, f, v)	if (_CVI_DEBUG_ && level <= _DEBUG_LEVEL_) { 				\
									printf("(%s: %d) " f "\n", __FILE__, __LINE__, v); 		\
								}
													 
#define LOG_MSG2(level, f, v1, v2)	if (_CVI_DEBUG_ && level <= _DEBUG_LEVEL_) { 			\
									printf("(%s: %d) " f "\n", __FILE__, __LINE__, v1, v2); \
								}
													 
//==============================================================================
// Types

// Used to return meaningful error information
typedef struct FCallReturn {
	int						retVal;			// Value returned by function call.
	char*					errorMsg;		// In case of error, additional info.
} FCallReturn_type;

//==============================================================================
// External variables

//==============================================================================
// Global functions

// Used to return extended error info in case of error.
FCallReturn_type*	init_FCallReturn_type			(int valFCall, const char errorOrigin[], const char errorDescription[]);

void				discard_FCallReturn_type		(FCallReturn_type** fCallReturnPtr);

// Formats error and warning messages ( errors: msgID < 0, message: msgID = 0, warning: msgID > 0 )
char* 				FormatMsg 						(int messageID, char fileName[], char functionName[], int lineNumber, char message[]);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DAQLabUtility_H__ */
