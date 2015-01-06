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
	char*					name; 
	ListType				slaves;					// List of HWTrigSlave_type* elements
	HANDLE WINAPI*			slaveArmedHndls;		// Array of slave armed event objects 
};

struct HWTrigSlave {
	char*					name; 
	HWTrigMaster_type*		master;
	HANDLE WINAPI			armed;
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables

//==============================================================================
// Global functions


HWTrigMaster_type* init_HWTrigMaster_type (char name[])
{
	HWTrigMaster_type*	master = malloc(sizeof(HWTrigMaster_type));
	if (!master) return NULL;
	
	// init
	master->slaves 				= 0;
	master->slaveArmedHndls		= NULL;
	
	master->name	= StrDup(name);
	
	if ( !(master->slaves = ListCreate(sizeof(HWTrigSlave_type*))) ) goto Error;
	
	return master;
	
Error:
	
	if (master->slaves) ListDispose(master->slaves);
	OKfree(master);
	
	return NULL;
}

void discard_HWTrigMaster_type (HWTrigMaster_type** masterPtr)
{
	if (!*masterPtr) return;
	
	// name
	OKfree((*masterPtr)->name);
	
	// slave armed event handles
	OKfree((*masterPtr)->slaveArmedHndls);
	
	// remove slave HW triggers
	size_t				nSlaves = ListNumItems((*masterPtr)->slaves);
	HWTrigSlave_type**	slavePtr;
	
	for (size_t i = 1; i <= nSlaves; i++) {
		slavePtr = ListGetPtrToItem((*masterPtr)->slaves, i);
		(*slavePtr)->master = NULL;
	}
	
	ListDispose((*masterPtr)->slaves);
	
	OKfree(*masterPtr);
}

HWTrigSlave_type* init_HWTrigSlave_type (char name[])
{
	HWTrigSlave_type*	slave = malloc(sizeof(HWTrigSlave_type));
	if (!slave) return NULL;
	
	slave->name			= StrDup(name);
	
	slave->master		= NULL;
	if ( !(slave->armed	= CreateEvent(NULL, FALSE, FALSE, NULL)) ) goto Error;
	

	return slave;
	
Error:
	
	OKfree(slave);
	
	return NULL;
}

void discard_HWTrigSlave_type (HWTrigSlave_type** slavePtr)
{
	if (!*slavePtr) return;
	
	OKfree((*slavePtr)->name);
	
	// remove slave from master if there is one
	RemoveHWTrigSlaveFromMaster(*slavePtr);
	
	// close armed event handle
	CloseHandle((*slavePtr)->armed);
	
	OKfree(*slavePtr);
}

HWTrigMaster_type* HWTrigMasterNameExists (ListType masterList, char name[], size_t* idx)
{
  	HWTrigMaster_type** 	masterPtr;
	size_t					nMasters	= ListNumItems(masterList);
	
	for (size_t i = 1; i <= nMasters; i++) {
		masterPtr = ListGetPtrToItem(masterList, i);
		if (!strcmp((*masterPtr)->name, name)) {
			if (idx) *idx = i;
			return *masterPtr;
		}
	}
	
	if (idx) *idx = 0;
	return NULL;
}

HWTrigSlave_type* HWTrigSlaveNameExists (ListType slaveList, char name[], size_t* idx)
{
  	HWTrigSlave_type** 		slavePtr;
	size_t					nSlaves		= ListNumItems(slaveList);
	
	for (size_t i = 1; i <= nSlaves; i++) {
		slavePtr = ListGetPtrToItem(slaveList, i);
		if (!strcmp((*slavePtr)->name, name)) {
			if (idx) *idx = i;
			return *slavePtr;
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

int AddHWTrigSlaveToMaster (HWTrigMaster_type* master, HWTrigSlave_type* slave, char** errorInfo)
{
#define AddHWTrigSlave_Err_SlaveAlreadyHasAMaster   -1
	
	// check if slave already has a master
	if (slave->master) {
		if (errorInfo)
			*errorInfo = FormatMsg(AddHWTrigSlave_Err_SlaveAlreadyHasAMaster, "AddHWTrigSlave", "HW triggered Slave already has a HW triggering Master assigned");
		return AddHWTrigSlave_Err_SlaveAlreadyHasAMaster; 
	}
	
	// add slave to master
	ListInsertItem(master->slaves, &slave, END_OF_LIST);
	slave->master = master;
	
	// generate slave armed event handles
	size_t					nSlaves 	= ListNumItems(master->slaves);
	HWTrigSlave_type**   	slavePtr;
	master->slaveArmedHndls = realloc(master->slaveArmedHndls, nSlaves * sizeof(HANDLE));
	for (size_t i = 0; i < nSlaves; i++) {
		slavePtr = ListGetPtrToItem(master->slaves, i+1);
		master->slaveArmedHndls[i] = (*slavePtr)->armed;
	}
	
	return 0;
}

void RemoveHWTrigSlaveFromMaster (HWTrigSlave_type* slave)
{
	if (!slave->master) return;
	
	// remove slave from master if there is one
	ListType			slaveList   = slave->master->slaves;
	size_t				nSlaves 	= ListNumItems(slaveList);
	HWTrigSlave_type**	slavePtr;
	
	for (size_t i = 1; i <= nSlaves; i++) {
		slavePtr = ListGetPtrToItem(slaveList, i);
		if (*slavePtr == slave) {
			ListRemoveItem(slaveList, 0, i);
			// update slave armed event handles from master
			slave->master->slaveArmedHndls = realloc(slave->master->slaveArmedHndls, (nSlaves-1) * sizeof(HANDLE));
			for (size_t j = 0; j < nSlaves-1; j++) {
				slavePtr = ListGetPtrToItem(slaveList, j+1);
				slave->master->slaveArmedHndls[j] = (*slavePtr)->armed;
			}
			break;
		}
	}
	
	slave->master = NULL;
}

void RemoveHWTrigMasterFromSlaves (HWTrigMaster_type* master)
{
	size_t nSlaves = ListNumItems(master->slaves);
	
	if (!nSlaves) return; // no slaves
	
	HWTrigSlave_type**	slavePtr;
	for (size_t i = 1; i <= nSlaves; i++) {
		slavePtr = ListGetPtrToItem(master->slaves, i);
		(*slavePtr)->master = NULL;
	}
	ListClear(master->slaves);
	OKfree(master->slaveArmedHndls);
}

int WaitForHWTrigArmedSlaves (HWTrigMaster_type* master, char** errorInfo)
{
#define WaitForHWTrigArmedSlaves_Err_Failed		-1
#define WaitForHWTrigArmedSlaves_Err_Timeout	-2
	
	DWORD 	fCallResult = 0;
	size_t	nSlaves		= ListNumItems(master->slaves);
	
	if (!nSlaves) return 0;
	
	fCallResult = WaitForMultipleObjects(nSlaves, master->slaveArmedHndls, TRUE, WaitForHWTrigArmedSlaves_Timeout);
	
	// check result
	switch (fCallResult) {
			
		case WAIT_TIMEOUT:
			
			if (errorInfo)
				*errorInfo = FormatMsg(WaitForHWTrigArmedSlaves_Err_Timeout, "WaitForHWTrigArmedSlaves", "Waiting for all HW triggered slaves timed out");
				
			return WaitForHWTrigArmedSlaves_Err_Timeout; 
			
		case WAIT_FAILED:
			
			DWORD error = GetLastError();
			// process here windows error message
			char	errMsg[500];
			Fmt(errMsg, "%s< WaitForMultipleObjects windows SDK function failed with error code %d", error);
			if (errorInfo)
				*errorInfo = FormatMsg(WaitForHWTrigArmedSlaves_Err_Failed, "WaitForHWTrigArmedSlaves", errMsg);
			
			return WaitForHWTrigArmedSlaves_Err_Failed;
	}
	
	return 0; // no error
}

int SetHWTrigSlaveArmedStatus (HWTrigSlave_type* slave, char** errorInfo)
{
#define SetHWTrigSlaveArmedStatus_Err_Failed		-1
	
	if (!SetEvent(slave->armed)) {
		
		DWORD error = GetLastError();
		// process here windows error message
		char	errMsg[500];
		Fmt(errMsg, "%s< SetEvent windows SDK function failed with error code %d", error);
		if (errorInfo)
			*errorInfo = FormatMsg(SetHWTrigSlaveArmedStatus_Err_Failed, "SetHWTrigSlaveArmedStatus", errMsg);
			
		return SetHWTrigSlaveArmedStatus_Err_Failed;
	}
	
	return 0;
}


