//==============================================================================
//
// Title:		DataStorage.c
// Purpose:		A short description of the implementation.
//
// Created on:	12-9-2014 at 12:08:22 by Lex van der Gracht.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
//#include "HDF5support.h"
#include "DAQLab.h" 		// include this first
     
#include "nivision.h"
#include "pathctrl.h"
#include "UI_DataStorage.h"
#include <formatio.h>
#include <userint.h>
#include "DataStorage.h"
#include "DisplayEngine.h"
#include "HDF5support.h"
//#include "tiffio.h"




//==============================================================================
// Constants
// Data Storage UI resource
#define UI_DataStorage			"./Modules/Data Storage/UI_DataStorage.uir"
#define MAXBASEFILEPATH			MAX_PATHNAME_LEN

//test
#define  DATAFILEBASEPATH 		"C:\\Rawdata\\"
#define VChanDataTimeout							1e4					// Timeout in [ms] for Sink VChans to receive data  


//==============================================================================
// Types


//==============================================================================
// Module implementation

struct DatStore {

	// SUPER, must be the first member to inherit from

	DAQLabModule_type 	baseClass;

	// DATA
	
		//-------------------------
		// Task Controller
		//-------------------------
		
	TaskControl_type*	taskController;
	
			//-------------------------
		// UI
		//-------------------------

	int					mainPanHndl;
	
	char*				basefilepath;
	char*				rawdatapath;    // test, for saving raw data
	char*				hdf5datafile;   // name of hdf5 data file 
	BOOL				overwrite_files;
	
		// Callback to install on controls from selected panel in UI_DataStorage.uir
		// Override: Optional, to change UI panel behavior. 
	CtrlCallbackPtr		uiCtrlsCB;
	
		// Callback to install on the panel from UI_DataStorage.uir
	PanelCallbackPtr	uiPanelCB;
	
		//-------------------------
		// Channels
		//-------------------------
		// Available datastorage channel data. Of DS_Channel_type* 
	ListType					channels;
	
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions
static void	RedrawDSPanel (DataStorage_type* ds);

static int DataReceivedTC (TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo);

//-----------------------------------------
// Data Storage Task Controller Callbacks
//-----------------------------------------
static int 					ConfigureTC 			(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);
static int 					UnConfigureTC 			(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo); 
//datastorage module shouldn't iterate
//static void				IterateTC				(TaskControl_type* taskControl, BOOL const* abortIterationFlag);
//static void 				AbortIterationTC		(TaskControl_type* taskControl, BOOL const* abortFlag);
//static int				StartTC					(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);
static int					DoneTC					(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);
static int					StoppedTC				(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);
static int					TaskTreeStatus 			(TaskControl_type* taskControl, TaskTreeStates status, char** errorInfo);
//static void				TCActive				(TaskControl_type* taskControl, BOOL UITCActiveFlag);
static int				 	ResetTC 				(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo); 
static void				 	ErrorTC 				(TaskControl_type* taskControl, int errorID, char* errorMsg);
//static int				ModuleEventHandler		(TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, void* eventData, BOOL const* abortFlag, char** errorInfo); 
//-----------------------------------------
// DataStorage Task Controller Callbacks
//-----------------------------------------




static int CVICALLBACK UIPan_CB (int panel, int event, void *callbackData, int eventData1, int eventData2);
static int CVICALLBACK UICtrls_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int Load (DAQLabModule_type* mod, int workspacePanHndl)  ;

//==============================================================================
// Global variables

//test
//char*				menudatapath;  

//==============================================================================
// Global functions


/// HIFN  Allocates memory and initializes the Data Storage module.
/// HIPAR mod/ if NULL both memory allocation and initialization must take place.
/// HIPAR mod/ if !NULL the function performs only an initialization of a DAQLabModule_type.
/// HIRET address of a DAQLabModule_type if memory allocation and initialization took place.
/// HIRET NULL if only initialization was performed.
DAQLabModule_type*	initalloc_DataStorage (DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl)
{
	DataStorage_type* 	ds;
	TaskControl_type*	tc;
	ListType			newTCList;
	char*				newTCName; 
	
	if (!mod) {
		ds = malloc (sizeof(DataStorage_type));
		if (!ds) return NULL;
	} else
		ds = (DataStorage_type*) mod;

	// initialize base class
	initalloc_DAQLabModule(&ds->baseClass, className, instanceName, workspacePanHndl);
	
	ds->basefilepath		= StrDup(DATAFILEBASEPATH);
	ds->rawdatapath 		= NULL;
	ds->hdf5datafile		= NULL;
	ds->overwrite_files		= FALSE;
	
	// create Data Storage Task Controller
	tc = init_TaskControl_type (instanceName, ds, DLGetCommonThreadPoolHndl(), ConfigureTC, UnConfigureTC, NULL, NULL,NULL , ResetTC,
								 DoneTC, StoppedTC, TaskTreeStatus, NULL, NULL, ErrorTC);
	if (!tc) {discard_DAQLabModule((DAQLabModule_type**)&ds); return NULL;}
	
	//------------------------------------------------------------

	//---------------------------
	// Parent Level 0: DAQLabModule_type
					
						
		// METHODS

		// overriding methods
	ds->baseClass.Discard 		= discard_DataStorage;
	ds->baseClass.Load			= Load;
	ds->baseClass.LoadCfg		= NULL; //LoadCfg;

	ds->mainPanHndl				= 0; 
	
		// assign default controls callback to UI_DataStorage.uir panel
	ds->uiCtrlsCB				= UICtrls_CB;
	
		// assign default panel callback to UI_DataStorage.uir
	ds->uiPanelCB				= UIPan_CB;
	
	
		// DATA
	ds->taskController			= tc;
	
	if (!(ds->channels			= ListCreate(sizeof(DS_Channel_type*))))	return NULL; 
	

	

	
		// METHODS
	


	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) ds;
	else
		return NULL;

}



static DS_Channel_type* init_DS_Channel_type (DataStorage_type* ds, int panHndl, char VChanName[])
{
	DLDataTypes allowedPacketTypes[] = {
		DL_Waveform_Char,						
		DL_Waveform_UChar,						
		DL_Waveform_Short,						
		DL_Waveform_UShort,					
		DL_Waveform_Int,						
		DL_Waveform_UInt,						
	//	DL_Waveform_SSize,	//what size?					
	//	DL_Waveform_Size,								   			
		DL_Waveform_Float,						
		DL_Waveform_Double,
		DL_Image};   	   
	
	DS_Channel_type* chan = malloc (sizeof(DS_Channel_type));
	if (!chan) return NULL;

	chan->dsInstance	= ds;
	chan->VChan			= init_SinkVChan_type(VChanName, allowedPacketTypes, NumElem(allowedPacketTypes), chan,VChanDataTimeout, NULL, NULL);
	chan->panHndl   	= panHndl;

	// add new channel to data storage module list of channels
	ListInsertItem(ds->channels, &chan, END_OF_LIST);


	return chan;
}


static void	discard_DS_Channel_type (DS_Channel_type** chan)
{   
		
	DS_Channel_type** 	chanPtr;
	DataStorage_type* 	ds				= (*chan)->dsInstance;
	size_t				nchannels		= ListNumItems(ds->channels);
	size_t				chIdx			= 1;
	int 				i;
	char*				vchanname		= NULL;
	char*				discvchanname		= NULL; 
	
	if (!*chan) return;

//	if ((*chan)->panHndl) {
//		DiscardPanel((*chan)->panHndl);
//		(*chan)->panHndl = 0;
//	}
	
	// remove channel     
	for (i = 1; i <= nchannels; i++) {	
		chanPtr = ListGetPtrToItem(ds->channels, i);
		vchanname=GetVChanName((*chanPtr)->VChan);
		discvchanname=GetVChanName((*chan)->VChan);
		if (strcmp(discvchanname,vchanname)==0){ 
			// remove from framework if vchan names match
			// unregister VChan from DAQLab framework
			DLUnregisterVChan((DAQLabModule_type*)ds, (VChan_type*)(*chanPtr)->VChan);
			// discard channel data structure
			ListRemoveItem(ds->channels, 0, chIdx);
			free(vchanname);
			free(discvchanname);
			break;
		}
		else {
			free(vchanname);
			free(discvchanname);
		}
		chIdx++;
	}

	// discard SourceVChan
	discard_VChan_type((VChan_type**)&(*chan)->VChan);
	
	

	OKfree(*chan);  // this also removes the channel from the device structure
}


/// HIFN Discards DataStorage data but does not free the structure memory.
void discard_DataStorage (DAQLabModule_type** mod)
{
	DataStorage_type* 		ds 			= (DataStorage_type*) (*mod);

	if (!ds) return;
	
	//---------------------------------------
	// discard DataStorage specific data
	//---------------------------------------
	// main panel and panels loaded into it (channels and task control)
	
	if (ds->mainPanHndl) {
		DiscardPanel(ds->mainPanHndl);
		ds->mainPanHndl = 0;
	}
	
	// discard Task Controller
	DLRemoveTaskController((DAQLabModule_type*)ds, ds->taskController);
	discard_TaskControl_type(&ds->taskController);

	//----------------------------------------
	// discard DAQLabModule_type specific data
	//----------------------------------------
	
	if (ds->channels) {
		size_t 				nItems = ListNumItems(ds->channels);
		DS_Channel_type**   channelPtr;
		for (size_t i = 1; i <= nItems; i++) {
			channelPtr = ListGetPtrToItem(ds->channels, nItems-i+1);
			if (channelPtr!=NULL) discard_DS_Channel_type(channelPtr); 
		}
	}
	
	
	
	ListDispose(ds->channels);  
	
	discard_DAQLabModule(mod);
	
	if (ds->basefilepath) OKfree(ds->basefilepath);
	if (ds->rawdatapath) OKfree(ds->rawdatapath); 
	if (ds->hdf5datafile) OKfree(ds->hdf5datafile);   
}







 //creates a usable data dir, returns nonzero if failed
int CreateRawDataDir(DataStorage_type* 	ds,TaskControl_type* taskControl)
{
	char*			  rootname; 
	char*			  tcname;
	char*			  rawdatapath;
	int 			  err=0;
	ssize_t 		  fileSize;
	char*			  pathName;
	BOOL			  uniquedir;
	int				  dircounter=0;
	char*			  dirctrstr;
	TaskControl_type* root_tc; 
	
	
	if (taskControl==NULL) return -1;
	
	//determine dir name  
	root_tc=GetTaskControlRootParent (taskControl);
	rootname=GetTaskControlName(root_tc);

	OKfree(ds->rawdatapath);
	ds->rawdatapath=StrDup(ds->basefilepath);
	AppendString(&ds->rawdatapath,"\\",-1);
	AppendString(&ds->rawdatapath,rootname,-1);
	free(rootname);

	
	if (FileExists (ds->rawdatapath, &fileSize)){
		if(ds->overwrite_files){
		//clear dir
			pathName=StrDup(ds->rawdatapath);
			AppendString(&pathName, "*.*", -1);	//create file name
			DeleteFile(pathName); 
			free(pathName);
		}
		else {
		//create unique dir
			uniquedir=FALSE;
			while (!uniquedir){
				pathName=StrDup(ds->rawdatapath); 
				dircounter++;
				dirctrstr=malloc(MAXBASEFILEPATH*sizeof(char));
				Fmt(dirctrstr,"%s<_%i",dircounter);
				AppendString(&pathName,dirctrstr,-1);
				OKfree(dirctrstr); 
				if (FileExists (pathName, &fileSize)!=1) {
					uniquedir=TRUE;
					OKfree(ds->rawdatapath);    
					ds->rawdatapath=StrDup(pathName);
					err=MakeDir(ds->rawdatapath);
				}
				free(pathName);
			}
			
		}
	}
	else  err=MakeDir(ds->rawdatapath);  
	
	
		
	return err;
	
}


char* CreateFullIterName(Iterator_type*		currentiter)
{
	
	Iterator_type* 	  childiter;
	Iterator_type* 	  parentiter;
	char*			  name;
	char*			  fullname; 
	char*			  tcname;
	size_t			  iteridx;
	
	if (currentiter==NULL) return NULL;
	
	parentiter = GetIteratorParent(currentiter);
	if (parentiter==NULL) return NULL;  //no parent
	tcname=GetCurrentIterationName(parentiter);
	iteridx=GetCurrentIterationIndex(parentiter);
	fullname=malloc(MAXBASEFILEPATH*sizeof(char));  
	Fmt (fullname, "%s<%s[w3]#%i",tcname,iteridx); 
	OKfree(tcname);
	while (parentiter!=NULL) {  
		childiter=parentiter;
		parentiter = GetIteratorParent(childiter);
		if (parentiter==NULL) {
			return fullname;  //no parent, return current name
		}
		tcname=GetCurrentIterationName(parentiter);
		iteridx=GetCurrentIterationIndex(parentiter);
		name=malloc(MAXBASEFILEPATH*sizeof(char));
		Fmt (name, "%s<%s[w3]#%i",tcname,iteridx);    		
		AddStringPrefix (&fullname,"/",-1);    
		AddStringPrefix (&fullname,name,-1);
		OKfree(tcname);
		OKfree(name);
	}
	return fullname;	
	
}




static int Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	DataStorage_type* 	ds 			= (DataStorage_type*) mod;
	int 				error		= 0; 
	int 				numListLines=5;

	// load main panel
	ds->mainPanHndl = LoadPanel(workspacePanHndl, UI_DataStorage, DSMain);
	
	// add module's task controller to the framework
	DLAddTaskController((DAQLabModule_type*)ds, ds->taskController);
	
	// connect module data and user interface callbackFn to all direct controls in the panel
	SetCtrlsInPanCBInfo(mod, ((DataStorage_type*)mod)->uiCtrlsCB, ds->mainPanHndl);
	
	SetCtrlVal(ds->mainPanHndl,DSMain_STRING,ds->basefilepath);
	
	
	DisplayPanel(ds->mainPanHndl);
	//default settings:

	TaskControlEvent(ds->taskController, TC_Event_Configure, NULL, NULL);


Error:

	return error;

}




//-----------------------------------------
// DataStorage Task Controller Callbacks
//-----------------------------------------

  
static int ConfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);
	
	return 0;
}

static int UnConfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);
	
	return 0;
}
 /*
static void	IterateTC	(TaskControl_type* taskControl,  BOOL const* abortIterationFlag)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);
	
	
	//return immediate
	TaskControlIterationDone (taskControl, 0, NULL,FALSE);

}

static void AbortIterationTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);    
}



static int	StartTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);
	TaskControl_type*		parent;
	char*					uiparentname;
	

	 
	return 0;
}
 */

static int DoneTC (TaskControl_type* taskControl,  BOOL const* abortFlag, char** errorInfo)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);

	return 0;
}

static int StoppedTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);

	return 0;
}


static int ResetTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);

	return 0;
}

static int	TaskTreeStatus (TaskControl_type* taskControl, TaskTreeStates status, char** errorInfo)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);
	
	int err=0;
	
	if (status) {
		//create a new data directory
		OKfree(ds->rawdatapath);			    // free previous path name
		CreateRawDataDir(ds,taskControl);
			//create HDF5 file
		OKfree(ds->hdf5datafile);               //free previous data file name
		ds->hdf5datafile=malloc(MAX_PATH*sizeof(char)); 
		Fmt(ds->hdf5datafile,"%s<%s\\data.h5",ds->rawdatapath);
		err=CreateHDF5file(ds->hdf5datafile,"/dset");
		if (err<0) MessagePopup("Error","Creating File");
	}
	
	return 0;
}

 /*
static void	TCActive (TaskControl_type* taskControl, BOOL UITCActiveFlag)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);
}
 */

static void	ErrorTC (TaskControl_type* taskControl, int errorID, char* errorMsg)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);
	int error=0;


	// print error message
	DLMsg(errorMsg, 1);
}
 /*
static int ModuleEventHandler (TaskControl_type* taskControl, TCStates taskState, BOOL taskActive,  void* eventData, BOOL const* abortFlag, char** errorInfo)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl); 
	
	return 0;
}

 */


static void	RedrawDSPanel (DataStorage_type* ds)
{
	size_t		nChannels		= 0;
	int			menubarHeight	= 0;
	int			chanPanHeight	= 0;
	int			chanPanWidth	= 0;
	int			taskPanWidth	= 0;

	// count the number of channels in use
	if (ds->channels) nChannels = ListNumItems(ds->channels); 
	
	if (nChannels>0) SetCtrlAttribute(ds->mainPanHndl,DSMain_COMMANDBUTTON_REM,ATTR_DIMMED,FALSE);
	else SetCtrlAttribute(ds->mainPanHndl,DSMain_COMMANDBUTTON_REM,ATTR_DIMMED,TRUE); 
	

}

static int CVICALLBACK UIPan_CB (int panel, int event, void *callbackData, int eventData1, int eventData2)
{
		DataStorage_type* 	ds 			= callbackData;
	
	return 0;
}


static int CVICALLBACK UICtrls_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	DataStorage_type* 	ds 			= callbackData;
	SinkVChan_type* 	sinkVChan;
	DS_Channel_type*	chan		= NULL; 
	DS_Channel_type**	chanPtr; 
	char				buff[DAQLAB_MAX_VCHAN_NAME + 50];  
	char*				vChanName;
	char				channame[DAQLAB_MAX_VCHAN_NAME];
	int 				numitems;
	int 				checked;
	int 				treeindex;
	int 				i;
	int 				channr=0;
	int					itemIndex;
	int 				reply;
	char*				currentbasepath;
	
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
					case DSMain_Channels: 
					break;
					
					case DSMain_COMMANDBUTTON_ADD:
					
						
						// channel checked, give new VChan name and add channel
						vChanName = DLGetUINameInput("New Virtual Channel", DAQLAB_MAX_VCHAN_NAME, DLValidateVChanName, NULL);
						if (!vChanName) {
							return 0;	// action cancelled
						}
						// rename label in settings panel
						sprintf(buff, "%s", vChanName);
						GetNumListItems(panel, DSMain_Channels,&numitems);
						InsertListItem(panel, DSMain_Channels, -1, buff,channr);        
					
						// create channel
						chan = init_DS_Channel_type(ds, panel, vChanName);
		
						// register VChan with DAQLab
						DLRegisterVChan((DAQLabModule_type*)ds, (VChan_type*)chan->VChan);
						AddSinkVChan(ds->taskController, chan->VChan, DataReceivedTC);

						// update main panel
						RedrawDSPanel(ds);
						free(vChanName);
					break;
					
					case DSMain_COMMANDBUTTON_REM:
						GetNumListItems(panel, DSMain_Channels,&numitems);    
						for(treeindex=0;treeindex<numitems;treeindex++){
							IsListItemChecked(panel,DSMain_Channels,treeindex, &checked);      
							if (checked==1) {
					   			// get channel name
								GetLabelFromIndex (panel, DSMain_Channels, treeindex, channame); 
								//get channel from list
								if (ds->channels) {
									numitems = ListNumItems(ds->channels); 
										for ( i= 1; i <= numitems; i++) {
											chanPtr = ListGetPtrToItem(ds->channels, i);
											if (chanPtr!=NULL) chan=*chanPtr;
											if (chan!=NULL) {
												if (strcmp(GetVChanName(chan->VChan),channame)==0){   
													//strings are equal; channel found
													// discard channel data
													discard_DS_Channel_type(&chan);
													// update channel list 
													DeleteListItem(panel,DSMain_Channels ,treeindex , 1);
													return 0;
												}
											}
										}
								}
							}
						}
						// update main panel
						RedrawDSPanel(ds);
					break;
					
					case DSMain_Change:
						currentbasepath=malloc(MAX_PATHNAME_LEN*sizeof(char));
						GetCtrlVal(ds->mainPanHndl,DSMain_STRING,currentbasepath);
						reply=DirSelectPopup (currentbasepath, "Select Base File Folder:", 1, 1, ds->basefilepath);
						if (reply) SetCtrlVal(ds->mainPanHndl,DSMain_STRING,ds->basefilepath);
						free(currentbasepath);
						break;
					case DSMain_STRING: 
						 //indicator only
					break;
					
					case DSMain_CHECKBOX_OVERWRITE: 
						 GetCtrlVal(panel,control,&ds->overwrite_files);
					break;
					
			}
	}
	return 0;
}


int SaveImage(char* filename,Image* image)
{
	int 			err=0;
	TIFFFileOptions options;

 	err=imaqWriteFile(image,filename, NULL); 
	
	return err;
}

int SaveTiffImage(char* filename,Image* image)
{
	int 			err=0;
	TIFFFileOptions options;

	options.photoInterp=IMAQ_BLACK_IS_ZERO;
	options.compressionType=IMAQ_NO_COMPRESSION;
 	err=imaqWriteTIFFFile(image,filename, &options, NULL); 
	
	return err;
}


static int DataReceivedTC (TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo)  
{
	
	DataStorage_type*	ds					= GetTaskControlModuleData(taskControl);
	FCallReturn_type*	fCallReturn			= NULL;
	unsigned int		nSamples;
	int					error;
	DataPacket_type**	dataPackets			= NULL;
	size_t				nPackets;
	size_t				nElem;
	char*				sinkVChanName		= GetVChanName((VChan_type*)sinkVChan);
	SourceVChan_type*   sourceVChan			= GetSourceVChan((VChan_type*)sinkVChan); 
	char*				sourceVChanName		= GetVChanName((VChan_type*)sourceVChan);  
	char* 				rawfilename; 
	int 				filehandle;
	int 				elementsize=2;
	char* 				errMsg 				= StrDup("Writing data to ");
	char				cmtStatusMessage[CMT_MAX_MESSAGE_BUF_SIZE];
	void*				dataPacketDataPtr;
	DLDataTypes			dataPacketType; 
	ImageDisplay_type*	imgDisplay;
	Image*				image;
	size_t 				i;
	DS_Channel_type*	chan; 
	DS_Channel_type**	chanPtr; 
	int 				numitems;
	Iterator_type*		currentiter;
	char*				fullitername;
	char*				channame;	
	Waveform_type*		waveform;
	Image_type*			receivedimage;
	
			
	if (ds->channels) {
		numitems = ListNumItems(ds->channels); 
		for ( i= 1; i <= numitems; i++) {
			chanPtr = ListGetPtrToItem(ds->channels, i);
			if (chanPtr!=NULL) chan=*chanPtr;
			if (chan!=NULL) {
				channame=GetVChanName(chan);
				if (strcmp(channame,sinkVChanName)==0){   
					//strings are equal; channel found
					free(channame);
					break;
				}
				else free(channame);
			}
		}
	}
			
	// get all available data packets
	errChk( GetAllDataPackets(sinkVChan, &dataPackets, &nPackets, &errMsg) );
	
				
	for (i= 0; i < nPackets; i++) {
		if (dataPackets[i]==NULL){
			ReleaseDataPacket(&dataPackets[i]); 
		}
		else{
			dataPacketDataPtr = GetDataPacketPtrToData(dataPackets[i], &dataPacketType); 
			currentiter=GetDataPacketCurrentIter(dataPackets[i]);
			switch (dataPacketType) {
				case DL_Waveform_Char:
					waveform=*(Waveform_type**)dataPacketDataPtr;
					error=WriteHDF5Data(ds->hdf5datafile,sourceVChanName,currentiter,waveform,DL_Char) ; 
					if (error<0) {
						FmtIOErrType fmterr=GetFmtIOError();
						AppendString(&errMsg, sinkVChanName, -1); 
						AppendString(&errMsg, " failed. Reason: file write failed", -1); 
					}	
				break;	
				case DL_Waveform_UChar:
					waveform=*(Waveform_type**)dataPacketDataPtr;
					error=WriteHDF5Data(ds->hdf5datafile,sourceVChanName,currentiter,waveform,DL_UChar) ;  
					if (error<0) {
						FmtIOErrType fmterr=GetFmtIOError();
						AppendString(&errMsg, sinkVChanName, -1); 
						AppendString(&errMsg, " failed. Reason: file write failed", -1); 
					}	
				break;	
				case DL_Waveform_Short:
					waveform=*(Waveform_type**)dataPacketDataPtr;
					error=WriteHDF5Data(ds->hdf5datafile,sourceVChanName,currentiter,waveform,DL_Short) ;
					if (error<0) {
						FmtIOErrType fmterr=GetFmtIOError();
						AppendString(&errMsg, sinkVChanName, -1); 
						AppendString(&errMsg, " failed. Reason: file write failed", -1); 
					}	
				break;
				case DL_Waveform_UShort:
					waveform=*(Waveform_type**)dataPacketDataPtr;
					error=WriteHDF5Data(ds->hdf5datafile,sourceVChanName,currentiter,waveform,DL_UShort) ; 
					if (error<0) {
						FmtIOErrType fmterr=GetFmtIOError();
						AppendString(&errMsg, sinkVChanName, -1); 
						AppendString(&errMsg, " failed. Reason: file write failed", -1); 
					}	
				break;
				case DL_Waveform_UInt:
					waveform=*(Waveform_type**)dataPacketDataPtr;
					error=WriteHDF5Data(ds->hdf5datafile,sourceVChanName,currentiter,waveform,DL_UInt) ;  
					if (error<0) {
						FmtIOErrType fmterr=GetFmtIOError();
						AppendString(&errMsg, sinkVChanName, -1); 
						AppendString(&errMsg, " failed. Reason: file write failed", -1); 
					}	
				break;
				case DL_Waveform_Int:
					waveform=*(Waveform_type**)dataPacketDataPtr;
					error=WriteHDF5Data(ds->hdf5datafile,sourceVChanName,currentiter,waveform,DL_Int) ;    
					if (error<0) {
						FmtIOErrType fmterr=GetFmtIOError();
						AppendString(&errMsg, sinkVChanName, -1); 
						AppendString(&errMsg, " failed. Reason: file write failed", -1); 
					}	
				break;
				case DL_Waveform_Float  :
					waveform=*(Waveform_type**)dataPacketDataPtr;
					error=WriteHDF5Data(ds->hdf5datafile,sourceVChanName,currentiter,waveform,DL_Float) ;
					if (error<0) {
						FmtIOErrType fmterr=GetFmtIOError();
						AppendString(&errMsg, sinkVChanName, -1); 
						AppendString(&errMsg, " failed. Reason: file write failed", -1); 
					}	
				break;
				case DL_Waveform_Double  :
					waveform=*(Waveform_type**)dataPacketDataPtr;
					error=WriteHDF5Data(ds->hdf5datafile,sourceVChanName,currentiter,waveform,DL_Double) ;
					if (error<0) {
						FmtIOErrType fmterr=GetFmtIOError();
						AppendString(&errMsg, sinkVChanName, -1); 
						AppendString(&errMsg, " failed. Reason: file write failed", -1); 
					}	
				break;
				case DL_Image:
					//saving tiff files:
					//get the image
				/*	receivedimage=*(Image_type**) dataPacketDataPtr;
					image=GetImageImage(receivedimage);
					rawfilename=malloc(MAXCHAR*sizeof(char)); 
					fullitername=CreateFullIterName(currentiter);
					free(rawfilename);
					free(fullitername);   */
				
				//saving into HDF5;
					receivedimage=*(Image_type**) dataPacketDataPtr;
					error=WriteHDF5Image(ds->hdf5datafile,sourceVChanName,currentiter,receivedimage) ;   
					
				break;
				default:
					//shouldn't happen!
				break;
			}
			ReleaseDataPacket(&dataPackets[i]);
		}
	}
		
	 
	OKfree(dataPackets);				
			
			
	
Error:
	
	OKfree(sinkVChanName);
	OKfree(sourceVChanName); 
	OKfree(errMsg);
	return error;
}


