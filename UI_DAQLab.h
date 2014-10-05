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

#define  TaskPan                          4
#define  TaskPan_Switchboard              2       /* control type: table, callback function: (none) */
#define  TaskPan_Plate                    3       /* control type: deco, callback function: (none) */
#define  TaskPan_TaskTree                 4       /* control type: tree, callback function: TaskTree_CB */
#define  TaskPan_Close                    5       /* control type: command, callback function: TaskTree_CB */
#define  TaskPan_ExecMode                 6       /* control type: ring, callback function: TaskTree_CB */

#define  TasksPan                         5

#define  TCDelPan                         6       /* callback function: DAQLab_TCDelPan_CB */
#define  TCDelPan_Plate                   2       /* control type: deco, callback function: (none) */
#define  TCDelPan_TaskControllers         3       /* control type: listBox, callback function: (none) */
#define  TCDelPan_DelBTTN                 4       /* control type: command, callback function: DAQLab_DelTaskControllersBTTN_CB */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

#define  MainMenu                         1
#define  MainMenu_Modules                 2       /* callback function: DAQLab_ModulesMenu_CB */
#define  MainMenu_TaskManager             3       /* callback function: DAQLab_DisplayTaskManagerMenu_CB */


     /* Callback Prototypes: */

int  CVICALLBACK CB_DAQLab_MainPan(int panel, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CloseDAQLabModulesPan_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK DAQLab_DelTaskControllersBTTN_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
void CVICALLBACK DAQLab_DisplayTaskManagerMenu_CB(int menubar, int menuItem, void *callbackData, int panel);
int  CVICALLBACK DAQLab_ManageDAQLabModules_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
void CVICALLBACK DAQLab_ModulesMenu_CB(int menubar, int menuItem, void *callbackData, int panel);
int  CVICALLBACK DAQLab_TCDelPan_CB(int panel, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK TaskTree_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif
