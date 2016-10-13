//==============================================================================
//
// Title:		HWTriggering.h
// Purpose:		Allows parallel execution of hardware triggered devices.
//
// Created on:	12/21/2014 at 2:40:25 PM by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
//
//==============================================================================

#ifndef __HWTriggering_H__
#define __HWTriggering_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "cvidef.h"
#include "toolbox.h"

//==============================================================================
// Constants
		

//==============================================================================
// Types

typedef struct HWTrigMaster		HWTrigMaster_type;
typedef struct HWTrigSlave		HWTrigSlave_type;


//==============================================================================
// External variables

//==============================================================================
// Global functions

//--------------------------------------------------------------------
// Create/Discard HW Triggered Master and Slaves
//--------------------------------------------------------------------

	// Creates a HW Master Trigger to which other HW Slave Triggers can connect
HWTrigMaster_type*			init_HWTrigMaster_type			(char name[]);
	// Discards a HW Master Trigger
void						discard_HWTrigMaster_type		(HWTrigMaster_type** masterPtr);

	// Creates a HW Slave Trigger that can be attached to a Master HW Trigger
HWTrigSlave_type* 			init_HWTrigSlave_type 			(char name[]);
	// Discards a HW Slave Trigger
void						discard_HWTrigSlave_type 		(HWTrigSlave_type** slavePtr);

	// Given a masterList of HWTrigMaster_type* elements, it searches this list for a given master name
	// and returns the Master HW Trig and its 1-based index in this list. If not found the function returns NULL
	// and idx is set to 0. If idx is not needed it can be set to 0.
HWTrigMaster_type*			HWTrigMasterNameExists			(ListType masterList, char name[], size_t* idx);

	// Given a slaveList of HWTrigSlave_type* elements, it searches this list for a given slave name
	// and returns the Slave HW Trig and its 1-based index in this list. If not found the function returns NULL
	// and idx is set to 0. If idx is not needed it can be set to 0.
HWTrigSlave_type* 			HWTrigSlaveNameExists 			(ListType slaveList, char name[], size_t* idx);

//--------------------------------------------------------------------
// Set/Get HW Triggered Master and Slave functions
//--------------------------------------------------------------------

	// Return HW Trig Slave name
char*						GetHWTrigMasterName				(HWTrigMaster_type* master);

	// Return HW Trig Master name
char*						GetHWTrigSlaveName				(HWTrigSlave_type* slave);

	// Returns the HW Trig Master of a given HW Trig Slave
HWTrigMaster_type*			GetHWTrigSlaveMaster			(HWTrigSlave_type* slave);

	// Returns HW Trig Slave from a HW Trig Master given its 1-based index
HWTrigSlave_type*			GetHWTrigSlaveFromMaster		(HWTrigMaster_type* master, size_t slaveIdx);

	// Returns the number of HW Trig Slaves of a given HW Trig Master
size_t						GetNumHWTrigSlaves				(HWTrigMaster_type* master);

	// Activates a HW-Triggered Slave so it can receive triggers from a HW-Trigger Master to which it may be connected.
void						SetHWTrigSlaveActive			(HWTrigSlave_type* slave, BOOL active);
	// Checks if the HW-Triggered Slave is active and it expects triggers from a HW-Trigger Master.
BOOL						GetHWTrigSlaveActive			(HWTrigSlave_type* slave);

//--------------------------------------------------------------------
// Connect / Disconnect HW triggered Slave to HW triggering Master 
//--------------------------------------------------------------------

int							AddHWTrigSlaveToMaster			(HWTrigMaster_type* master, HWTrigSlave_type* slave, char** errorMsg);

void 						RemoveHWTrigSlaveFromMaster 	(HWTrigSlave_type* slave);

void						RemoveHWTrigMasterFromSlaves	(HWTrigMaster_type* master);

//--------------------------------------------------------------------
// HW triggered Slave and HW triggering Master coordination
//--------------------------------------------------------------------

int 						WaitForHWTrigArmedSlaves 		(HWTrigMaster_type* master, char** errorMsg);

int 						SetHWTrigSlaveArmedStatus 		(HWTrigSlave_type* slave, char** errorMsg);


#ifdef __cplusplus
    }
#endif

#endif  /* ndef __HWTriggering_H__ */
