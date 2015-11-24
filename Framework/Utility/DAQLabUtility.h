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
#define	_DEBUG_LEVEL_ -1
#endif
		
//==============================================================================
// Include files

#include "cvidef.h"

//==============================================================================
// Constants
		
//-------------------------------------------------------------------------------
// Error checking macros
//-------------------------------------------------------------------------------
typedef struct {
	int			result;
	int			error;
	int			line;
	char*		errMsg;
} ErrorInfo_type;

#ifndef INIT_ERR
#define INIT_ERR	ErrorInfo_type errorInfo = {.result = 0, .error = 0, .line = 0, .errMsg = NULL};	
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

#define OKfree(ptr) 					if (ptr) {free(ptr); ptr = NULL;} else
#define OKfreeCAHndl(objHandle)			if (objHandle) {CA_DiscardObjHandle(objHandle); objHandle = 0;} else
#define OKfreePanHndl(panelHandle)  	if (panelHandle) {DiscardPanel(panelHandle); panelHandle = 0;} else
#define OKfreeList(list) 				if (list) {ListDispose(list); list = 0;} else  

#define NumElem(ptr) (sizeof(ptr)/sizeof(ptr[0]))	 // Note: do not use this inside a function to get the number of elements in an array passed as an argument!

#ifdef RETURN_ERR
#undef RETURN_ERR
#endif

#define RETURN_ERR \
	if (errorInfo.error < 0) { \
		if (errorMsg) { \
			if (errorInfo.errMsg) \
				*errorMsg = FormatMsg(errorInfo.error, __FILE__, __func__, errorInfo.line, errorInfo.errMsg); \
			else \
				*errorMsg = FormatMsg(errorInfo.error, __FILE__, __func__, errorInfo.line, "Unknown or out of memory"); \
		} else \
			OKfree(errorInfo.errMsg);} \
	return errorInfo.error;

#ifdef SET_ERR
#undef SET_ERR
#endif

#define SET_ERR(errorID, errorMsg) \
{ \
	errorInfo.error = errorID; \
	errorInfo.errMsg = FormatMsg(errorID, __FILE__, __func__, __LINE__, errorMsg); \
	goto Error; \
}

//-----------------------------------------------------------------------------------------------------
// CVI Cmt multithreading library error handling
//-----------------------------------------------------------------------------------------------------

// Cmt library error macro
#ifdef CmtErrChk
#undef CmtErrChk
#endif

#define CmtErrChk(fCall) if (errorInfo.error = (fCall), errorInfo.line = __LINE__, errorInfo.error < 0) \ 
{goto CmtError;} else

// obtain Cmt error description and jumps to Error
#ifdef Cmt_ERR
#undef Cmt_ERR
#endif

#define Cmt_ERR { \
	if (errorInfo.error < 0) { \
		char CmtErrMsgBuffer[CMT_MAX_MESSAGE_BUF_SIZE] = ""; \
		errChk( CmtGetErrorMessage(errorInfo.error, CmtErrMsgBuffer) ); \
		nullChk( errorInfo.errMsg = StrDup(CmtErrMsgBuffer) ); \
	} \
	goto Error; \
}


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
						   
/* XXX: NI BUG Report # 423491
 * Occurs only in DEBUG build.
 * When a structure is allocated and if any field in the structure is accessed 
 * through its address without initialization, it gives run time fatal error.
 * Workaround : guard adrress using UNCHECKED(address).
 * WARN: Make sure you use it only when you are completely sure of memory
 * access.
 */
#define UNCHECKED(x)	((void*)(uintptr_t)(x))
													 
//==============================================================================
// Types

// Used to return meaningful error information
typedef struct FCallReturn {
	int						retVal;			// Value returned by function call.
	char*					errorInfo;		// In case of error, additional info.
} FCallReturn_type;

//==============================================================================
// External variables

//==============================================================================
// Global functions

// Used to return extended error info in case of error.
FCallReturn_type*	init_FCallReturn_type			(int valFCall, const char errorOrigin[], const char errorDescription[]);

void				discard_FCallReturn_type		(FCallReturn_type** fCallReturnPtr);

// Formats error and warning messages ( errors: msgID < 0, message: msgID = 0, warning: msgID > 0 )
char* 				FormatMsg						(int messageID, char fileName[], char functionName[], int lineNumber, char message[]);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DAQLabUtility_H__ */
