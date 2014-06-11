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

#define  ZStagePan                        1
#define  ZStagePan_MoveZDown              2       /* control type: pictButton, callback function: (none) */
#define  ZStagePan_MoveZUp                3       /* control type: pictButton, callback function: (none) */
#define  ZStagePan_ZRelPos                4       /* control type: numeric, callback function: (none) */
#define  ZStagePan_ZAbsPos                5       /* control type: numeric, callback function: (none) */
#define  ZStagePan_ZStepSize              6       /* control type: ring, callback function: (none) */
#define  ZStagePan_Cycle_RelEnd           7       /* control type: numeric, callback function: (none) */
#define  ZStagePan_Cycle_RelStart         8       /* control type: numeric, callback function: (none) */
#define  ZStagePan_NSteps                 9       /* control type: numeric, callback function: (none) */
#define  ZStagePan_Stop                   10      /* control type: command, callback function: (none) */
#define  ZStagePan_GoHomePos              11      /* control type: command, callback function: (none) */
#define  ZStagePan_UpdateHomePos          12      /* control type: command, callback function: (none) */
#define  ZStagePan_AddHomePos             13      /* control type: command, callback function: (none) */
#define  ZStagePan_SetEndPos              14      /* control type: command, callback function: (none) */
#define  ZStagePan_SetStartPos            15      /* control type: command, callback function: (none) */
#define  ZStagePan_RefPosList             16      /* control type: listBox, callback function: (none) */
#define  ZStagePan_ZStageStatus           17      /* control type: LED, callback function: (none) */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

#define  ZStage                           1
#define  ZStage_Limits                    2


     /* (no callbacks specified in the resource file) */ 


#ifdef __cplusplus
    }
#endif
