//==============================================================================
//
// Title:		HWTriggering.c
// Purpose:		A short description of the implementation.
//
// Created on:	12/21/2014 at 2:40:25 PM by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include <windows.h> 
#include "HWTriggering.h"
#include "DAQLabUtility.h"
#include <formatio.h>
#include <ansi_c.h>
#include "toolbox.h"

//==============================================================================
// Constants

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}

#define WaitForHWTrigArmedSlaves_Timeout		1e4		// Timeout in [ms] for HW triggering masters to wait for all HW triggered slaves to be armed. 
														// Warning: do not set this to 0 as the HW triggering master will not wait for slaves to be armed anymore.

//==============================================================================
// Types

struct HWTrigMaster {
	char*					name; 						// HW-trigger Master name.
	ListType				slaves;						// List of HWTrigSlave_type* elements.
	size_t					nActiveSlaves;				// Number of HW-triggered slaves connected to this Master HW-trigger that expect to be triggered.
	HANDLE WINAPI*			activeSlavesArmedHndls;		// Array of active slaves armed event object handles of length nActiveSlaves.
};

struct HWTrigSlave {
	char*					name; 						// HW-triggered Slave name.
	HWTrigMaster_type*		master;						// Master HW-trigger of this Slave.
	HANDLE WINAPI			armed;						// Object handle to signal if slave is armed.
	BOOL					active;						// If True, Slave expects to be triggered, False otherwise. Default: True.
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static void				GenerateSlaveArmedHandles				(HWTrigMaster_type* master);

//==============================================================================
// Global variables

//==============================================================================
// Global functions


static void GenerateSlaveArmedHandles (HWTrigMaster_type* master)
{
	size_t					nSlaves 	= ListNumItems(master->slaves);
	HWTrigSlave_type*   	slave		= NULL;
	
	master->nActiveSlaves = 0;
	OKfree(master->activeSlavesArmedHndls);
	for (size_t i = 1; i <= nSlaves; i++) {
		slave = *(HWTrigSlave_type**)ListGetPtrToItem(master->slaves, i);
		if (slave->active) {
			master->nActiveSlaves++;
			master->activeSlavesArmedHndls = realloc(master->activeSlavesArmedHndls, master->nActiveSlaves * sizeof(HANDLE));
			master->activeSlavesArmedHndls[master->nActiveSlaves-1] = slave->armed;
		}
	}
}

HWTrigMaster_type* init_HWTrigMaster_type (char name[])
{
	HWTrigMaster_type*	master = malloc(sizeof(HWTrigMaster_type));
	if (!master) return NULL;
	
	// init
	master->name						= StrDup(name);
	master->slaves 						= 0;
	master->nActiveSlaves				= 0;
	master->activeSlavesArmedHndls		= NULL;
	
	// allocate
	if ( !(master->slaves 				= ListCreate(sizeof(HWTrigSlave_type*))) ) goto Error;
	
	return master;
	
Error:
	
	if (master->slaves) ListDispose(master->slaves);
	OKfree(master);
	
	return NULL;
}

void discard_HWTrigMaster_type (HWTrigMaster_type** masterPtr)
{
	HWTrigMaster_type*	master	= *masterPtr;
	
	if (!master) return;
	
	// name
	OKfree(master->name);
	
	// slave armed event handles
	OKfree(master->activeSlavesArmedHndls);
	
	// remove slave HW triggers
	size_t				nSlaves = ListNumItems(master->slaves);
	HWTrigSlave_type*	slave	= NULL;
	
	for (size_t i = 1; i <= nSlaves; i++) {
		slave = *(HWTrigSlave_type**)ListGetPtrToItem(master->slaves, i);
		slave->master = NULL;
	}
	
	ListDispose(master->slaves);
	
	OKfree(*masterPtr);
}

HWTrigSlave_type* init_HWTrigSlave_type (char name[])
{
	HWTrigSlave_type*	slave = malloc(sizeof(HWTrigSlave_type));
	if (!slave) return NULL;
	
	slave->name			= StrDup(name);
	
	slave->master		= NULL;
	if ( !(slave->armed	= CreateEvent(NULL, FALSE, FALSE, NULL)) ) goto Error;
	
	slave->active		= TRUE;
	
	return slave;
	
Error:
	
	OKfree(slave);
	
	return NULL;
}

void discard_HWTrigSlave_type (HWTrigSlave_type** slavePtr)
{
	HWTrigSlave_type*	slave = *slavePtr;
	if (!slave) return;
	
	OKfree(slave->name);
	
	// remove slave from master if there is one
	RemoveHWTrigSlaveFromMaster(slave);
	
	// close armed event handle
	CloseHandle(slave->armed);
	
	OKfree(*slavePtr);
}

HWTrigMaster_type* HWTrigMasterNameExists (ListType masterList, char name[], size_t* idx)
{
  	HWTrigMaster_type* 		master		= NULL;
	size_t					nMasters	= ListNumItems(masterList);
	
	for (size_t i = 1; i <= nMasters; i++) {
		master = *(HWTrigMaster_type**)ListGetPtrToItem(masterList, i);
		if (!strcmp(master->name, name)) {
			if (idx) *idx = i;
			return master;
		}
	}
	
	if (idx) *idx = 0;
	return NULL;
}

HWTrigSlave_type* HWTrigSlaveNameExists (ListType slaveList, char name[], size_t* idx)
{
  	HWTrigSlave_type* 		slave		= NULL;
	size_t					nSlaves		= ListNumItems(slaveList);
	
	for (size_t i = 1; i <= nSlaves; i++) {
		slave = *(HWTrigSlave_type**)ListGetPtrToItem(slaveList, i);
		if (!strcmp(slave->name, name)) {
			if (idx) *idx = i;
			return slave;
		}
	}
	
	if (idx) *idx = 0;
	return NULL;
}

char* GetHWTrigMasterName (HWTrigMaster_type* master)
{
	return StrDup(master->name);
}

char* GetHWTrigSlaveName (HWTrigSlave_type* slave)
{
	return StrDup(slave->name);
}

HWTrigMaster_type* GetHWTrigSlaveMaster (HWTrigSlave_type* slave)
{
	return slave->master;
}

HWTrigSlave_type* GetHWTrigSlaveFromMaster (HWTrigMaster_type* master, size_t slaveIdx)
{
	HWTrigSlave_type** slavePtr = ListGetPtrToItem(master->slaves, slaveIdx);
	
	return *slavePtr;
}

size_t GetNumHWTrigSlaves (HWTrigMaster_type* master)
{
	return ListNumItems(master->slaves);
}

void SetHWTrigSlaveActive (HWTrigSlave_type* slave, BOOL active)
{
	// set slave active/inactive if different than current state
	if (slave->active == active) return;
	
	slave->active = active;
	
	// update master active slave handles
	if (slave->master)
		GenerateSlaveArmedHandles(slave->master);
}

BOOL GetHWTrigSlaveActive (HWTrigSlave_type* slave)
{
	return slave->active;
}

int AddHWTrigSlaveToMaster (HWTrigMaster_type* master, HWTrigSlave_type* slave, char** errorMsg)
{
#define AddHWTrigSlave_Err_SlaveAlreadyHasAMaster   -1
	
INIT_ERR
	
	// check if slave already has a master
	if (slave->master)
		SET_ERR(AddHWTrigSlave_Err_SlaveAlreadyHasAMaster, "HW triggered Slave already has a HW triggering Master assigned");
		
	// add slave to master
	ListInsertItem(master->slaves, &slave, END_OF_LIST);
	slave->master = master;
	
	// if slave is active, then add its handle to the master
	if (slave->active) {
		master->nActiveSlaves++;
		nullChk( master->activeSlavesArmedHndls = realloc (master->activeSlavesArmedHndls, master->nActiveSlaves * sizeof(HANDLE)) );
		master->activeSlavesArmedHndls[master->nActiveSlaves-1] = slave->armed;
	}
	
Error:
	
RETURN_ERR
}

void RemoveHWTrigSlaveFromMaster (HWTrigSlave_type* slave)
{
	if (!slave->master) return;
	
	// remove slave from master if there is one
	ListType			slaveList   = slave->master->slaves;
	size_t				nSlaves 	= ListNumItems(slaveList);
	HWTrigSlave_type*	slaveItem	= NULL;
	
	for (size_t i = 1; i <= nSlaves; i++) {
		slaveItem = *(HWTrigSlave_type**)ListGetPtrToItem(slaveList, i);
		if (slaveItem == slave) {
			ListRemoveItem(slaveList, 0, i);
			// update master slave armed event handles
			GenerateSlaveArmedHandles(slave->master);
			break;
		}
	}
	
	slave->master = NULL;
}

void RemoveHWTrigMasterFromSlaves (HWTrigMaster_type* master)
{
	size_t nSlaves = ListNumItems(master->slaves);
	
	if (!nSlaves) return; // no slaves
	
	HWTrigSlave_type*	slave = NULL;
	for (size_t i = 1; i <= nSlaves; i++) {
		slave = *(HWTrigSlave_type**)ListGetPtrToItem(master->slaves, i);
		slave->master = NULL;
	}
	
	ListClear(master->slaves);
	master->nActiveSlaves = 0;
	OKfree(master->activeSlavesArmedHndls);
}

int WaitForHWTrigArmedSlaves (HWTrigMaster_type* master, char** errorMsg)
{
#define WaitForHWTrigArmedSlaves_Err_Failed		-1
#define WaitForHWTrigArmedSlaves_Err_Timeout	-2
	
INIT_ERR

	DWORD 	fCallResult 	= 0;
	char	msgBuff[500] 	= "";
	
	if (!master->nActiveSlaves) return 0;
	
	fCallResult = WaitForMultipleObjects(master->nActiveSlaves, master->activeSlavesArmedHndls, TRUE, (DWORD) WaitForHWTrigArmedSlaves_Timeout);
	
	// check result
	switch (fCallResult) {
			
		case WAIT_TIMEOUT:
			
			SET_ERR(WaitForHWTrigArmedSlaves_Err_Timeout, "Waiting for all HW triggered slaves timed out.");
		
		case WAIT_FAILED:
			
			// process here windows error message
			Fmt(msgBuff, "%s< WaitForMultipleObjects windows SDK function failed with error code %d", GetLastError());
			SET_ERR(WaitForHWTrigArmedSlaves_Err_Failed, msgBuff);
	}
	
Error:
	
RETURN_ERR
}

int SetHWTrigSlaveArmedStatus (HWTrigSlave_type* slave, char** errorMsg)
{
#define SetHWTrigSlaveArmedStatus_Err_Failed		-1
	
INIT_ERR
	
	char	msgBuff[500]	= "";
	
	if (!slave->active) return 0; // skip if slave is not active
	
	if (!SetEvent(slave->armed)) {
		
		// process here windows error message
		Fmt(msgBuff, "%s< SetEvent windows SDK function failed with error code %d", GetLastError());
		SET_ERR(SetHWTrigSlaveArmedStatus_Err_Failed, msgBuff);
	}
	
Error:
	
RETURN_ERR
}


