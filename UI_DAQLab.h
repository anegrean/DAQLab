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

#define  LogPan                           1
#define  LogPan_LogBox                    2       /* control type: textBox, callback function: (none) */

#define  MainPan                          2       /* callback function: CB_DAQLab_MainPan */

#define  TasksPan                         3

#define  TCDelPan                         4       /* callback function: DAQLab_TCDelPan_CB */
#define  TCDelPan_TaskControllers         2       /* control type: listBox, callback function: (none) */
#define  TCDelPan_DelBTTN                 3       /* control type: command, callback function: DAQLab_DelTaskControllersBTTN_CB */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* Callback Prototypes: */

int  CVICALLBACK CB_DAQLab_MainPan(int panel, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK DAQLab_DelTaskControllersBTTN_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK DAQLab_TCDelPan_CB(int panel, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif
