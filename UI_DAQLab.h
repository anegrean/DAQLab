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

#define  LogPan                           1
#define  LogPan_LogBox                    2       /* control type: textBox, callback function: (none) */

#define  MainPan                          2       /* callback function: CB_DAQLab_MainPan */

#define  ModulesPan                       3
#define  ModulesPan_Plate                 2       /* control type: deco, callback function: (none) */
#define  ModulesPan_Loaded                3       /* control type: listBox, callback function: DAQLab_ManageDAQLabModules_CB */
#define  ModulesPan_Available             4       /* control type: listBox, callback function: DAQLab_ManageDAQLabModules_CB */
#define  ModulesPan_Close                 5       /* control type: command, callback function: CloseDAQLabModulesPan_CB */

#define  TasksPan                         4

#define  TCDelPan                         5       /* callback function: DAQLab_TCDelPan_CB */
#define  TCDelPan_Plate                   2       /* control type: deco, callback function: (none) */
#define  TCDelPan_TaskControllers         3       /* control type: listBox, callback function: (none) */
#define  TCDelPan_DelBTTN                 4       /* control type: command, callback function: DAQLab_DelTaskControllersBTTN_CB */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

#define  MainMenu                         1
#define  MainMenu_Modules                 2       /* callback function: DAQLab_MenuModules_CB */


     /* Callback Prototypes: */

int  CVICALLBACK CB_DAQLab_MainPan(int panel, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CloseDAQLabModulesPan_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK DAQLab_DelTaskControllersBTTN_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK DAQLab_ManageDAQLabModules_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
void CVICALLBACK DAQLab_MenuModules_CB(int menubar, int menuItem, void *callbackData, int panel);
int  CVICALLBACK DAQLab_TCDelPan_CB(int panel, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif
