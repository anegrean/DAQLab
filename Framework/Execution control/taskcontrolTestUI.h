/**************************************************************************/
/* LabWindows/CVI User Interface Resource (UIR) Include File              */
/* Copyright (c) National Instruments 2014. All Rights Reserved.          */
/*                                                                        */
/* WARNING: Do not add to, delete from, or otherwise modify the contents  */
/*          of this include file.                                         */
/**************************************************************************/

#include <userint.h>

#ifdef __cplusplus
    extern "C" {
#endif

     /* Panels and Controls: */

#define  ControlPan                       1       /* callback function: CB_ControlPan */
#define  ControlPan_IterCompleteBTTN      2       /* control type: command, callback function: CB_TaskController */
#define  ControlPan_StepBTTN              3       /* control type: command, callback function: CB_TaskController */
#define  ControlPan_AbortBTTN             4       /* control type: command, callback function: CB_TaskController */
#define  ControlPan_StopBTTN              5       /* control type: command, callback function: CB_TaskController */
#define  ControlPan_StartBTTN             6       /* control type: command, callback function: CB_TaskController */
#define  ControlPan_ConfigureBTTN         7       /* control type: command, callback function: CB_TaskController */
#define  ControlPan_ExecutionLogBox       8       /* control type: textBox, callback function: (none) */
#define  ControlPan_LED                   9       /* control type: LED, callback function: (none) */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* Callback Prototypes: */

int  CVICALLBACK CB_ControlPan(int panel, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CB_TaskController(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif
