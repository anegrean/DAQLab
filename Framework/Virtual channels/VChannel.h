//==============================================================================
//
// Title:		VChannel.h
// Purpose:		A short description of the interface.
//
// Created on:	18-6-2014 at 19:11:05 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __VChannel_H__
#define __VChannel_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

//==============================================================================
// Constants

//==============================================================================
// Types

//------------------------------------------------------------------------------
// Virtual channel types
//------------------------------------------------------------------------------

typedef struct VChan		VChan_type;
		
typedef struct SinkVChan 	SinkVChan_type;

typedef struct SourceVChan 	SourceVChan_type;

//------------------------------------------------------------------------------
// Virtual channel data flow direction
//------------------------------------------------------------------------------

typedef enum {
	
	VCHAN_SINK, 						 
	VCHAN_SOURCE
	
} VChanDataFlow_type;

//------------------------------------------------------------------------------
// Virtual channel data types
//------------------------------------------------------------------------------

typedef enum {
	
	VCHAN_CHAR,			// 8 bits	   			 			char
	VCHAN_UCHAR,		// 8 bits			 				unsigned char
	VCHAN_SHORT,		// 16 bits							short
	VCHAN_USHORT,   	// 16 bits							unsigned short
	VCHAN_INT,			// 32 bits							int
	VCHAN_UINT,			// 32 bits							unsigned int
	VCHAN_SSIZE_T,  	// 32 bits or 64 bits 				ssize_t
	VCHAN_SIZE_T,		// 32 bits or 64 bits				size_t			   			
	VCHAN_FLOAT,		// 16 bits							float
	VCHAN_DOUBLE,   	// 32 bits							double
	
	VCHAN_IMGPTR		// 32 or 64 bits, sizeof(void*),	pointer to an NI Vision image
	
} VChanData_type;
				
//------------------------------------------------------------------------------
// Callback functions
//------------------------------------------------------------------------------

	// Callback when a VChan connection is established between a Source and Sink
typedef void	(* Connected_CBFptr_type) 		(VChan_type* self, VChan_type* connectedVChan);

	// Callback when a VChan connection is broken by calling VChan_Disconnect. When disconnecting
	// a Source type VChan, this callback will be called for each Sink.
typedef void	(* Disconnected_CBFptr_type) 	(VChan_type* self, VChan_type* disconnectedVChan);


//==============================================================================
// Global functions

//------------------------------------------------------------------------------
// Creation / Destruction
//------------------------------------------------------------------------------

SourceVChan_type*		init_SourceVChan_type	(char 						name[], 
										  	  	 VChanData_type 			dataType,
										 	  	 void* 						vChanOwner,
												 Connected_CBFptr_type		Connected_CBFptr,
												 Disconnected_CBFptr_type	Disconnected_CBFptr);


SinkVChan_type*			init_SinkVChan_type		(char 						name[], 
										  	  	 VChanData_type 			dataType,
										 	  	 void* 						vChanOwner,
												 int 						queueSize,
												 size_t						dataSize,
												 Connected_CBFptr_type		Connected_CBFptr,
												 Disconnected_CBFptr_type	Disconnected_CBFptr);

	// Discard common to both types of VChan. Cast SourceVChan_type** and SinkVChan_type** to VChan_type**.
void 					discard_VChan_type 		(VChan_type** vchan); 

//------------------------------------------------------------------------------
// Connections
//------------------------------------------------------------------------------

	// Connects a Sink and a Source VChan .
BOOL					VChan_Connect			(SourceVChan_type* source, SinkVChan_type* sink);

	// Disconnects a VChan. 
BOOL					VChan_Disconnect		(VChan_type* vchan); 

//------------------------------------------------------------------------------
// Set/Get
//------------------------------------------------------------------------------

void 					SetVChanName 			(VChan_type* vchan, char newName[]);

	// Returns pointer to dynamically allocated VChan name (null terminated string) 
char*					GetVChanName			(VChan_type* vchan);



#ifdef __cplusplus
    }
#endif

#endif  /* ndef __VChannel_H__ */
