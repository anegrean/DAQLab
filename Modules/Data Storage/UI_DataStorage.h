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

#define  DSMain                           1
#define  DSMain_Channels                  2       /* control type: listBox, callback function: (none) */
#define  DSMain_COMMANDBUTTON_REM         3       /* control type: command, callback function: CB_RemoveChannel */
#define  DSMain_COMMANDBUTTON_ADD         4       /* control type: command, callback function: CB_AddChannel */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* Callback Prototypes: */

int  CVICALLBACK CB_AddChannel(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CB_RemoveChannel(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif
