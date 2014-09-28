/**************************************************************************/
/* LabWindows/CVI User Interface Resource (UIR) Include File              */
/*                                                                        */
/* WARNING: Do not add to, delete from, or otherwise modify the contents  */
/*          of this include file.                                         */
/**************************************************************************/

#include <userint.h>

#ifdef __cplusplus
    extern "C" {
#endif

     /* Panels and Controls: */

#define  ZSetPan                          1
#define  ZSetPan_Plate                    2       /* control type: deco, callback function: (none) */
#define  ZSetPan_MaximumLimit             3       /* control type: numeric, callback function: (none) */
#define  ZSetPan_MinimumLimit             4       /* control type: numeric, callback function: (none) */
#define  ZSetPan_VelocityHigh             5       /* control type: numeric, callback function: (none) */
#define  ZSetPan_VelocityMid              6       /* control type: numeric, callback function: (none) */
#define  ZSetPan_VelocityLow              7       /* control type: numeric, callback function: (none) */
#define  ZSetPan_OKBTTN                   8       /* control type: command, callback function: (none) */

#define  ZStagePan                        2
#define  ZStagePan_Plate                  2       /* control type: deco, callback function: (none) */
#define  ZStagePan_MoveZDown              3       /* control type: pictButton, callback function: (none) */
#define  ZStagePan_MoveZUp                4       /* control type: pictButton, callback function: (none) */
#define  ZStagePan_ZRelPos                5       /* control type: numeric, callback function: (none) */
#define  ZStagePan_ZAbsPos                6       /* control type: numeric, callback function: (none) */
#define  ZStagePan_ZStepSize              7       /* control type: ring, callback function: (none) */
#define  ZStagePan_EndRelPos              8       /* control type: numeric, callback function: (none) */
#define  ZStagePan_StartAbsPos            9       /* control type: numeric, callback function: (none) */
#define  ZStagePan_Step                   10      /* control type: numeric, callback function: (none) */
#define  ZStagePan_NSteps                 11      /* control type: numeric, callback function: (none) */
#define  ZStagePan_Stop                   12      /* control type: command, callback function: (none) */
#define  ZStagePan_AddRefPos              13      /* control type: command, callback function: (none) */
#define  ZStagePan_RefPosList             14      /* control type: listBox, callback function: (none) */
#define  ZStagePan_Status                 15      /* control type: LED, callback function: (none) */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

#define  ZStage                           1
#define  ZStage_MENU1                     2


     /* (no callbacks specified in the resource file) */ 


#ifdef __cplusplus
    }
#endif
