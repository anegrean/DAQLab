//==============================================================================
//
// Title:		NIDAQmx.c
// Purpose:		A short description of the implementation.
//
// Created on:	22-7-2014 at 15:54:27 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DAQLab.h" 		// include this first 
#include "DAQLabModule.h"
#include "NIDAQmx.h"
#include "UI_NIDAQmx.h"
//==============================================================================
// Constants

// NIDAQmx UI resource

#define MOD_NIDAQmx_UI_NIDAQmx 		"./Modules/NIDAQmx/UI_NIDAQmx.uir"

//==============================================================================
// Types

// Ranges for DAQmx IO, stored as pairs of low value and high value, e.g. -10 V, 10 V 
typedef struct {
	int Nrngs;    // number of ranges
	double* low;  // low value 
	double* high; // high value
} IORange_type;

// DAQ device parameters
typedef struct {
	char*        name;    	   // dynamically allocated string
	char*        type;    	   // dynamically allocated string
	int          serial;       // device serial number
	int          nAI;          // # AI chan
	int          nAO;          // # AO chan
	int          nDIlines;     // # DI lines
	int          nDIports;     // # DI ports
	int          nDOlines;     // # DO lines
	int          nDOports;     // # DO ports
	int          nCI;          // # Counter inputs
	int          nCO;          // # Counter outputs
	// The array of char* must be terminated with a NULL, so use malloc for n strings + 1 pointers.
	// If the char* array is not terminated by NULL deallocation will go wrong and unpredictable behavior will occur
	ListType     AIchan;       // list of strings
	ListType     AOchan;	   // list of strings
	ListType     DIlines;      // list of strings
	ListType     DIports;      // list of strings
	ListType     DOlines;      // list of strings
	ListType     DOports;      // list of strings
	ListType     CIchan;	   // list of strings
	ListType     COchan;	   // list of strings
	double       AISCmaxrate;  // Single channel AI max rate   [Hz]
	double       AIMCmaxrate;  // Multiple channel AI max rate [Hz]
	double       AIminrate;    // AI minimum rate              [Hz]
	double       AOmaxrate;    // AO maximum rate              [Hz]
	double       AOminrate;    // AO minimum rate              [Hz]
	double       DImaxrate;    // DI maximum rate              [Hz]
	double       DOmaxrate;    // DO maximum rate              [Hz]
	IORange_type AIVrngs;      // ranges for AI voltage input  [V]
	IORange_type AOVrngs;      // ranges for AO voltage output [V]
} DAQdev_type;

//==============================================================================
// Module implementation

struct NIDAQmx {
	
	// SUPER, must be the first member to inherit from
	
	DAQLabModule_type 	baseClass;
	
	// DATA
	
	
	// METHODS 
	
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static int 					init_DevList 					(ListType devlist);

static int	 				init_DAQdev_type				(DAQdev_type* a); 
static void 				discard_DAQdev_type				(DAQdev_type** a);

void 						init_IORange_type				(IORange_type* range);
void 						discard_IORange_type			(IORange_type* a);

static char* 				substr							(const char* token, char** idxstart);

//-----------------------------------------
// ZStage Task Controller Callbacks
//-----------------------------------------

static FCallReturn_type*	NIDAQmx_ConfigureTC				(TaskControl_type* taskControl, BOOL const* abortFlag);
static void					NIDAQmx_IterateTC				(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type*	NIDAQmx_StartTC					(TaskControl_type* taskControl, BOOL const* abortFlag);
static FCallReturn_type*	NIDAQmx_DoneTC					(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type*	NIDAQmx_StoppedTC				(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type* 	NIDAQmx_ResetTC 				(TaskControl_type* taskControl, BOOL const* abortFlag); 
static void				 	NIDAQmx_ErrorTC 				(TaskControl_type* taskControl, char* errorMsg, BOOL const* abortFlag);
static FCallReturn_type*	NIDAQmx_EventHandler			(TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag);  




//==============================================================================
// Global variables

int				devListPanHndl	= 0;		// Panel handle to select NI DAQmx device and load their Task Controllers in the workspace.
ListType		devList			= 0;		// List of DAQ devices available of DAQdev_type. This will be updated every time init_DevList(ListType) is executed.

//==============================================================================
// Global functions


/// HIFN  Allocates memory and initializes an NI DAQmx device.
/// HIPAR mod/ if NULL both memory allocation and initialization must take place.
/// HIPAR mod/ if !NULL the function performs only an initialization of a DAQLabModule_type.
/// HIRET address of a DAQLabModule_type if memory allocation and initialization took place.
/// HIRET NULL if only initialization was performed. 
DAQLabModule_type*	initalloc_NIDAQmx (DAQLabModule_type* mod, char className[], char instanceName[])
{
	NIDAQmx_type* nidaq;
	
	if (!mod) {
		nidaq = malloc (sizeof(NIDAQmx_type));
		if (!nidaq) return NULL;
	} else
		nidaq = (NIDAQmx_type*) mod;
		
	// initialize base class
	initalloc_DAQLabModule(&nidaq->baseClass, className, instanceName);
	//------------------------------------------------------------
	
	//---------------------------
	// Parent Level 0: DAQLabModule_type 
	
		// DATA
		
		// METHODS
		
			// overriding methods
			
		nidaq->baseClass.Load			= NIDAQmx_Load;  
			
			
	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) nidaq;
	else
		return NULL;
}

/// HIFN Discards NIDAQmx_type data but does not free the structure memory.
void discard_NIDAQmx (DAQLabModule_type** mod)
{
	NIDAQmx_type* nidaq = (NIDAQmx_type*) (*mod);
	
	if (!nidaq) return;
	
	// discard NIDAQmx_type specific data
	
	
	
	// discard DAQLabModule_type specific data
	discard_DAQLabModule(mod);
}

int	NIDAQmx_Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	int				error			= 0;
	NIDAQmx_type* 	nidaq			= (NIDAQmx_type*) mod;  
	
	// allocate resources
	if (!devListPanHndl)
		errChk(devListPanHndl 	= LoadPanel(workspacePanHndl, MOD_NIDAQmx_UI_NIDAQmx, DevListPan));
	
	if (!devList)
		nullChk(devList			= ListCreate(sizeof(DAQdev_type)));
	
	DisplayPanel(devListPanHndl);
	
	init_DevList (devList);
	
	return 0;
	Error:
	
	if (devListPanHndl) {DiscardPanel(devListPanHndl); devListPanHndl = 0;}
	if (devList)		{ListDispose(devList); devList = 0;}
	
	return error;
}

/// HIFN Populates table and a devlist of DAQdev_type with NIDAQmx devices and their properties.
/// HIRET positive value for the number of devices found, 0 if there are no devices and negative values for error.
static int init_DevList (ListType devlist)
{
	int				error		= 0;
    int 			buffersize  = 0;
	char* 			tmpnames	= NULL;
	char* 			tmpsubstr	= NULL;
	char* 			dev_pt     	= NULL;
	char** 			idxnames  	= NULL;          // Used to break up the names string
	char** 			idxstr    	= NULL;          // Used to break up the other strings like AI channels
	DAQdev_type 	dev; 
	int 			ndev        = 0;
	int 			columns     = 0;
	int 			elem        = 0;
	double* 		rngs     	= NULL;

	
	// Allocate memory for pointers to work on the strings
	nullChk(idxnames = malloc(sizeof(char*)));
	nullChk(idxstr = malloc(sizeof(char*)));
	*idxstr = NULL;
	
	// Get number of table columns
	GetNumTableColumns (devListPanHndl, DevListPan_DAQtable, &columns);
	
	// Empty list of devices detected in the computer
	empty_DevList(devlist);
	
	// Empty Table contents
	DeleteTableRows (devListPanHndl, DevListPan_DAQtable, 1, -1);
	
	// Get string of device names in the computer
	buffersize = DAQmxGetSystemInfoAttribute (DAQmx_Sys_DevNames, NULL);  
	if (!buffersize) {
		SetCtrlAttribute (devListPanHndl, DevListPan_DAQtable, ATTR_LABEL_TEXT,"No devices found"); 
		return 0;
	}
	
	nullChk(tmpnames = malloc (buffersize));
	DAQmxGetSystemInfoAttribute (DAQmx_Sys_DevNames, tmpnames, buffersize);
	
	// Get first dev name using malloc, dev_pt is dynamically allocated
	*idxnames = tmpnames;
	dev_pt = substr (", ", idxnames);
	
	/* Populate table and device structure with device info */
	while (dev_pt!=NULL)
	{
		ndev++; 
		// Init DAQdev_type dev structure and fill it with data
		
		init_DAQdev_type(&dev);  
		
		!!!MessagePopup ("Error","DAQdev_type structure could not be initialized."); 
		
		// 1. Name (dynamically allocated)
		dev.name = dev_pt;
		
		// 2. Type (dynamically allocated)
		buffersize = DAQmxGetDeviceAttribute(dev_pt,DAQmx_Dev_ProductType,NULL);
		dev.type = malloc(buffersize);
		MEMTEST(dev.type);
		DAQmxGetDeviceAttribute (dev_pt,DAQmx_Dev_ProductType,dev.type,buffersize);
		
		// 3. S/N
		DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_SerialNum, &(dev.serial),1); 
		
		// Macro to fill lists with channel names and count
		#define FILLSTRLIST(property, list, nitems) 							\
				buffersize = DAQmxGetDeviceAttribute (dev_pt, property, NULL);  \
				*idxstr = realloc (*idxstr, buffersize); 						\
				MEMTEST(*idxstr); 												\
				DAQmxGetDeviceAttribute (dev_pt, property, *idxstr, buffersize);\
				nitems = 0; 													\
				tmpsubstr = substr (", ", idxstr);								\
				while (tmpsubstr != NULL) {										\
					nitems++; 													\
					ListInsertItem (list, &tmpsubstr, END_OF_LIST);				\
					tmpsubstr = substr (", ", idxstr); 							\
				}; 																\
									
		
		// 4. AI
		FILLSTRLIST(DAQmx_Dev_AI_PhysicalChans, dev.AIchan, dev.nAI);  
		// 5. AO
		FILLSTRLIST(DAQmx_Dev_AO_PhysicalChans, dev.AOchan, dev.nAO);
		// 6. DI lines
		FILLSTRLIST(DAQmx_Dev_DI_Lines, dev.DIlines, dev.nDIlines);
		// 7. DI ports
		FILLSTRLIST(DAQmx_Dev_DI_Ports, dev.DIports, dev.nDIports);					
		// 8. DO lines
		FILLSTRLIST(DAQmx_Dev_DO_Lines, dev.DOlines, dev.nDOlines);  
		// 9. DO ports
		FILLSTRLIST(DAQmx_Dev_DO_Ports, dev.DOports, dev.nDOports);
		// 10. CI 
		FILLSTRLIST(DAQmx_Dev_CI_PhysicalChans, dev.CIchan, dev.nCI);  
		// 11. CO 
		FILLSTRLIST(DAQmx_Dev_CO_PhysicalChans, dev.COchan, dev.nCO);
		// 12. Single Channel AI max rate
		DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AI_MaxSingleChanRate, &(dev.AISCmaxrate)); // [Hz]
		// 13. Multiple Channel AI max rate
		DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AI_MaxMultiChanRate, &(dev.AIMCmaxrate));  // [Hz]
		// 14. AI min rate 
		DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AI_MinRate, &(dev.AIminrate));  	         // [Hz] 
		// 15. AO max rate  
		DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AO_MaxRate, &(dev.AOmaxrate)); 			 // [Hz]
		// 16. AO min rate  
		DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AO_MinRate, &(dev.AOminrate)); 			 // [Hz]
		// 17. DI max rate
		DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DI_MaxRate, &(dev.DImaxrate)); 			 // [Hz]
		// 18. DO max rate
		DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DO_MaxRate, &(dev.DOmaxrate));    		 // [Hz]
		
		/*---Get IO ranges---*/
		
		// AI ranges
		if((elem = DAQmxGetDeviceAttribute(dev.name, DAQmx_Dev_AI_VoltageRngs, NULL)) > 0){
			dev.AIVrngs.Nrngs = (int) elem/2;
			rngs = NULL;
			rngs = malloc(elem*sizeof(double));
			dev.AIVrngs.low = malloc((dev.AIVrngs.Nrngs)*sizeof(double));
			dev.AIVrngs.high = malloc((dev.AIVrngs.Nrngs)*sizeof(double));
			if (!DAQmxGetDeviceAttribute(dev.name, DAQmx_Dev_AI_VoltageRngs, rngs, elem)){
				for (int i = 0; i < dev.AIVrngs.Nrngs; i++){
					dev.AIVrngs.low[i] = rngs[2*i];	
					dev.AIVrngs.high[i] = rngs[2*i+1];
				};
				free(rngs);
			};
		};
		
		// AO ranges
		if((elem = DAQmxGetDeviceAttribute(dev.name, DAQmx_Dev_AO_VoltageRngs, NULL)) > 0){
			dev.AOVrngs.Nrngs = (int) elem/2;
			rngs = NULL;
			rngs = malloc(elem*sizeof(double));
			dev.AOVrngs.low = malloc((dev.AOVrngs.Nrngs)*sizeof(double));
			dev.AOVrngs.high = malloc((dev.AOVrngs.Nrngs)*sizeof(double));
			if (!DAQmxGetDeviceAttribute(dev.name, DAQmx_Dev_AO_VoltageRngs, rngs, elem)){
				for (int i = 0; i < dev.AOVrngs.Nrngs; i++){
					dev.AOVrngs.low[i] = rngs[2*i];	
					dev.AOVrngs.high[i] = rngs[2*i+1];
				};
				free(rngs);
			};
		};
			
		
		/* DAQ Table Update */
		
		// Insert the new row
    	InsertTableRows (TasksPanelHandle, TasksUIpan_DAQtable, -1, 1, VAL_USE_MASTER_CELL_TYPE);
		// Highlight first row
		if (ndev == 1) SetTableCellRangeAttribute (TasksPanelHandle, TasksUIpan_DAQtable, MakeRect(1,1,1,columns),ATTR_TEXT_BGCOLOR, VAL_YELLOW); 
		SetTableCellRangeAttribute (TasksPanelHandle, TasksUIpan_DAQtable, MakeRect(1,1,ndev,columns), ATTR_CELL_MODE , VAL_INDICATOR);
		// Set currently selected device from table
		currDev=1;
		
		// Macro to write DAQ Device data to table
		#define DAQTable(cell, info) SetTableCellVal (TasksPanelHandle,TasksUIpan_DAQtable , MakePoint (cell,ndev), info)     
        
		// 1. Name
		DAQTable(1, dev.name);
		// 2. Type
		DAQTable(2, dev.type);  
		// 3. S/N
		DAQTable(3, dev.serial);
		// 4. # AI chan 
		DAQTable(4, dev.nAI);
		// 5. # AO chan
		DAQTable(5, dev.nAO);
		// 6. # DI lines
		DAQTable(6, dev.nDIlines);
		// 7. # DI ports  
		DAQTable(7, dev.nDIports);
		// 8. # DO lines
		DAQTable(8, dev.nDOlines);
		// 9. # DO ports  
		DAQTable(9, dev.nDOports);
		// 10. # CI 
		DAQTable(10, dev.nCI);
		// 11. # CO ports  
		DAQTable(11, dev.nCO);
		// 12. Single Channel AI max rate  
		DAQTable(12, dev.AISCmaxrate/1000); // display in [kHz]
		// 13. Multiple Channel AI max rate 
		DAQTable(13, dev.AIMCmaxrate/1000); // display in [kHz] 
		// 14. AI min rate
		DAQTable(14, dev.AIminrate/1000);   // display in [kHz] 
		// 15. AO max rate
		DAQTable(15, dev.AOmaxrate/1000);   // display in [kHz]
		// 16. AO min rate  
		DAQTable(16, dev.AOminrate/1000);   // display in [kHz] 
		// 17. DI max rate
		DAQTable(17, dev.DImaxrate/1000);   // display in [kHz] 
		// 18. DO max rate  
		DAQTable(18, dev.DOmaxrate/1000);   // display in [kHz]   
		
		
		/* Add device to list */
		ListInsertItem(devlist, &dev, END_OF_LIST);
		
		//Get the next device name in the list
		dev_pt = substr (", ", idxnames); 
	}
	
	SetCtrlAttribute (TasksPanelHandle, TasksUIpan_DAQtable, ATTR_LABEL_TEXT,"Devices found");  
	
	FreeItemPtr(*idxnames);
	FreeItemPtr(*idxstr);
	
	Error:
	
	OKfree(idxnames);
	OKfree(idxstr);
	OKfree(tmpnames);
	
	return error;
}

void empty_DevList (ListType devList)
{
    DAQdev_type* devPtr;
	
	while (ListNumItems (devList)) {
		devPtr = ListGetPtrToItem (devList, END_OF_LIST);
		discard_DAQdev_type (&devPtr);
		ListRemoveItem (devList, NULL, END_OF_LIST);
	}
}

static int init_DAQdev_type(DAQdev_type* a)
{
	int error = 0;
	
	a -> name         	= NULL; // dynamically allocated string
	a -> type         	= NULL; // dynamically allocated string
	a -> serial       	= -1;   // device serial number
	a -> nAI          	= -1;   // # AI chan
	a -> nAO          	= -1;   // # AO chan
	a -> nDIlines     	= -1;   // # DI lines
	a -> nDIports     	= -1;   // # DI ports
	a -> nDOlines     	= -1;   // # DO lines
	a -> nDOports     	= -1;   // # DO ports
	a -> nCI          	= -1;   // # Counter inputs
	a -> nCO          	= -1;   // # Counter outputs
	a -> AISCmaxrate  	= -1;   // Single channel AI max rate
	a -> AIMCmaxrate  	= -1;   // Multiple channel AI max rate
	a -> AIminrate    	= -1;
	a -> AOmaxrate    	= -1;
	a -> AOminrate    	= -1;
	a -> DImaxrate    	= -1;
	a -> DOmaxrate		= -1;
	
	// init lists
	a -> AIchan			= 0;
	a -> AOchan			= 0;
	a -> DIlines		= 0;
	a -> DIports		= 0;
	a -> DOlines		= 0;
	a -> DOports		= 0;
	a -> CIchan			= 0;
	a -> COchan			= 0;
	
	// list of channel names
	nullChk(a -> AIchan   = ListCreate (sizeof(char *)));
	nullChk(a -> AOchan	  = ListCreate (sizeof(char *)));
	nullChk(a -> DIlines  = ListCreate (sizeof(char *)));
	nullChk(a -> DIports  = ListCreate (sizeof(char *)));
	nullChk(a -> DOlines  = ListCreate (sizeof(char *)));
	nullChk(a -> DOports  = ListCreate (sizeof(char *)));
	nullChk(a -> CIchan   = ListCreate (sizeof(char *)));
	nullChk(a -> COchan   = ListCreate (sizeof(char *)));
	
	init_IORange_type(a -> AIVrngs);
	init_IORange_type(a -> AOVrngs);
	
	return 0;
	Error:
	
	if (a->AIchan) 		ListDispose(a->AIchan);
	if (a->AOchan) 		ListDispose(a->AOchan);
	if (a->DIlines) 	ListDispose(a->DIlines);
	if (a->DIports) 	ListDispose(a->DIports);
	if (a->DOlines) 	ListDispose(a->DOlines);
	if (a->DOports) 	ListDispose(a->DOports);
	if (a->CIchan) 		ListDispose(a->CIchan);
	if (a->COchan) 		ListDispose(a->COchan);
	
	return error;
}

static void discard_DAQdev_type(DAQdev_type** a)
{
	if (!(*a)) return;
	
	OKfree((*a) -> name);
	OKfree((*a) -> type);
	
	// Empty lists and free pointers within lists. Pointers must be allocated first by malloc, realloc or calloc.
	ListDisposePtrList ((*a) -> AIchan); 
	ListDisposePtrList ((*a) -> AOchan);	  
	ListDisposePtrList ((*a) -> DIlines);  
	ListDisposePtrList ((*a) -> DIports);  
	ListDisposePtrList ((*a) -> DOlines);  
	ListDisposePtrList ((*a) -> DOports);  
	ListDisposePtrList ((*a) -> CIchan);   
	ListDisposePtrList ((*a) -> COchan);
	
	discard_IORange_type((*a) -> AIVrngs);
	discard_IORange_type((*a) -> AOVrngs);
	
	*a = NULL;
}

void init_IORange_type(IORange_type* range)
{
	range -> Nrngs 	= 0;
	range -> low 	= NULL;
	range -> high 	= NULL;
}

void discard_IORange_type(IORange_type* a)
{
	a->Nrngs = 0;
	OKfree(a->low);
	OKfree(a->high);
}

/// HIFN Breaks up a string into substrings separated by a token. Call first with idxstart = &string which will keep track of the string extraction process. 
/// HIFN Returned strings are allocated with malloc.
static char* substr(const char* token, char** idxstart)
{
	char* tmp = NULL;
	char* idxend;
	
	if (*idxstart == NULL) return NULL;
	
	idxend = strstr(*idxstart, token);
	if (idxend != NULL){
		if (**idxstart != 0) {
			tmp=malloc((idxend-*idxstart+1)*sizeof(char)); // allocate memory for characters to copy plus null character
			if (tmp == NULL){
				MessagePopup("Error","Not enough memory.");
				return NULL;
			};
        	// Copy substring
			memcpy(tmp, *idxstart, (idxend-*idxstart)*sizeof(char));
			// Add null character
			*(tmp+(idxend-*idxstart)) = 0; 
			// Update
			*idxstart = idxend+strlen(token);
			return tmp;
		};} else {
		tmp = malloc((strlen(*idxstart)+1)*sizeof(char));
		if (tmp == NULL){
			MessagePopup("Error","Not enough memory.");
			return NULL;
		};
		// Initialize with a null character
		*tmp = 0;
		// Copy substring
		strcpy(tmp, *idxstart);
		*idxstart = NULL; 
		return tmp;
	};
	return NULL;
}






