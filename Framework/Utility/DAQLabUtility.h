//==============================================================================
//
// Title:		DAQLabUtility.h
// Purpose:		A short description of the interface.
//
// Created on:	11/12/2015 at 12:57:34 PM by popescu.andrei1991@gmail.com.
// Copyright:	Vrije University Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __DAQLabUtility_H__
#define __DAQLabUtility_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "cvidef.h"

//==============================================================================
// Constants

//==============================================================================
// Types

//==============================================================================
// External variables

//==============================================================================
// Global functions

int minmax_array(void *arr, int *(compareFn)(void*, void*), size_t numElements, size_t sizeOfElement);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DAQLabUtility_H__ */
