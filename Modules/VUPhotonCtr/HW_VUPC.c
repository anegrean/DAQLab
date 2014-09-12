//==============================================================================
//
// Title:		HW_VUPC.c
// Purpose:		A short description of the implementation.
//
// Created on:	2-7-2014 at 14:31:24 by Lex.
// Copyright:	Vrije Universiteit. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include "DAQLab.h"
#include <formatio.h>
#include "cvidef.h"
#include <toolbox.h>
#include <userint.h>
#include "TaskController.h"
#include "VUPCIkernel_dma.h"
#include "HW_VUPC.h"
#include "VUPhotonCtr.h"

//==============================================================================
// Constants

#define WDTE_ERR_BIT	0x01000000
#define RDTE_ERR_BIT	0x02000000
#define DTE_DONE_BIT	0x80000000
		
#define MAJOR_VERSION	2
#define MINOR_VERSION	0 

		//driver status bits
#define READYFORDATA	0x00000001
#define WORKERACTIVE	0x00000002
		
#define BYTESPERPIXEL	8  
	

#define QUEUE_LENGTH	100   
	
#define NUMTHREADS		10
		
#define MAXBYTES		0x100000  //1Mb    
		
#define MAXCHAR 255  //testval for filename
	


//==============================================================================
// Types

//states
typedef enum {
	PMT_ERROR,
	PMT_IDLE,
	PMT_WAITFORARMED,
	PMT_READREQ
} PMTcontroller_state;


typedef struct _PMTregcommand {
	unsigned long regaddress;
	unsigned long regvalue;
	int RW;
} PMTregcommand;


//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables
TaskControl_type* gtaskControl;

//test
char* rawfilepath="D:\\Rawdata\\";


//int              		QuitFlag;
unsigned short int* 	Samplebuffer	= NULL;

int         			measurementmode =MEASMODE_FINITE;  //default
int 		 			bufsize; 
CmtThreadPoolHandle 	poolHandle		= 0;


//DefineThreadSafeScalarVar(int,PMTstate,0); 
DefineThreadSafeScalarVar(int,PMTCommandFlag,0);				    //do a PMT controller register command in pmt thread
DefineThreadSafeScalarVar(unsigned long,PMTControllerData,0); 	    // current PMT controller data
DefineThreadSafeScalarVar(int ,PMTBufsize,0); 	    				// pmt buffer size        
DefineThreadSafeScalarVar(unsigned int,PMTnewbufsizeflag,0); 	    // newbufsize flag
DefineThreadSafeScalarVar(unsigned int,Controlreg,0); 	            // controlreg 
DefineThreadSafeScalarVar(unsigned int,AcqBusy,0); 	            // acquisition is busy 
DefineThreadSafeScalarVar(unsigned int,ReadyForReading,0); 	            // acquisition thread is ready for reading



unsigned int 	PMTThreadID; 
int 			PMTThreadFunctionID;
PMTregcommand 	newcommand;
int readerror=0; 
int readdata=0;

int 			nrsamples_in_iteration;   //requested amount of samples
int 			nrsamples;				  //number of samples measured 
int 			iterationnr;              //current iteration number, passed in data



//==============================================================================
// Global functions
int CVICALLBACK PMTThreadFunction(void *(functionData));
int StartDAQThread(CmtThreadPoolHandle poolHandle) ;
int StopDAQThread(CmtThreadPoolHandle poolHandle);
int PMTReset(void);
unsigned long ImReadReg(unsigned long regaddress);
int ImWriteReg(unsigned long regaddress,unsigned long regvalue);


unsigned int GetAcquisitionBusy(void)
{
	return GetAcqBusy();
}


void SetMeasurementMode(int mode)
{
	 measurementmode=mode;
}

void SetCurrentIterationnr(int iternr)
{
	 iterationnr=iternr;
}

void Setnrsamples_in_iteration(int mode,int samplerate_in_khz,int itsamples)
{
	 int bufsize;
	 
	 if (mode==1) {   //continuous mode, bufsize based on sample freq
		 if (samplerate_in_khz>500) {
			 bufsize=0x200000;  
		 }
		 else if (samplerate_in_khz>250) {
			 bufsize=0x100000;  
		 }
		 else if (samplerate_in_khz>125) {
			 bufsize=0x80000;  
		 }
		 else if (samplerate_in_khz>64) {
			 bufsize=0x40000;  
		 }
		 else if (samplerate_in_khz>32) {
			 bufsize=0x20000;  
		 }
		 else bufsize=0x10000;      
	 }
	 else {  //finite mode, bufsize based on sample requested samples
	 	nrsamples_in_iteration=itsamples;
	 	bufsize=8*itsamples;  //8 bytes per sample (pixel)
	 }
	 SetPMTBufsize(bufsize);  
	 SetPMTnewbufsizeflag(1);
}





 int ReadPMTReg(unsigned long regaddress,unsigned long *regval)
{
	int error=0;
	unsigned long reply=0;
	 
	if(!VUPCI_Get_Reg(regaddress,&reply)){ 
		error=-1;
	 };
	*regval=reply;
	
	return error;
	 
}


int WritePMTReg(unsigned long regaddress,unsigned long regvalue)
{
	 int error=0;
	 
	 if(!VUPCI_Set_Reg(regaddress,regvalue)){ 
		error=-1;
	 };

	 return error;
}

///  HIFN  Gets the PMT Controller Hardware Version
///  HIRET returns error, no error when 0
int GetPMTControllerVersion(void)
{
	int err=0;
	unsigned long version;
//	char buffer[255];
	int major,minor;
	
	err= ReadPMTReg(VERS_REG,&version);
	
	minor=version&0x0000000F;
	major=(version>>4)&0x0000000F;
//	Fmt (buffer, "%s<PMT controller version %i[w2p0].%i[w2p0]\n", major, minor);
//	AppendString(reply, buffer, -1); 
	if(!((major==MAJOR_VERSION)&&(minor==MINOR_VERSION))) err=-1;
	return err;
}


int PMTController_Init(void)
{
	int error=0;
	unsigned long timeout=5000;  //in ms
	
	InitializeAcqBusy();
	InitializeControlreg();
	InitializePMTCommandFlag();
	InitializePMTControllerData();
	InitializePMTBufsize();   
	InitializePMTnewbufsizeflag(); 
	InitializeReadyForReading();
	
	SetPMTCommandFlag(0);
	SetPMTBufsize(0);   
	SetPMTnewbufsizeflag(0);
	SetControlreg(0);     
	SetAcqBusy(0);
	SetReadyForReading(0);
	
	errChk(VUPCI_Open());
	errChk(GetPMTControllerVersion());
	
//	errChk(CmtNewThreadPool(NUMTHREADS,&poolHandle)); 
//	errChk(StartDAQThread(poolHandle)); 
	
	error=PMTReset();
	VUPCI_Set_Read_Timeout(timeout);
	
Error:
	
	return error;  
}

int PMTController_Finalize(void)	    
{    
	 int error=0;
	 
	 SetAcqBusy(0); 
	 UninitializeAcqBusy();  
	 UninitializeControlreg();  
	 UninitializePMTCommandFlag();
	 UninitializePMTControllerData();
	 UninitializePMTBufsize();   
	 UninitializePMTnewbufsizeflag();  
	 UninitializeReadyForReading();
	 
	 //clear control register, make sure PMT's are disabled
	 error=WritePMTReg(CTRL_REG,0);     
	 VUPCI_Close();
	 
	 return error;
}

//called in pmt thread
int ReadBuffer(unsigned short int* samplebuffer,int bufsize)
{
//	int err=0;
	int result;
//	DataPacket_type  datapacket_pmt1;
//	DataPacket_type  datapacket_pmt2;
//	DataPacket_type  datapacket_pmt3;
//	DataPacket_type  datapacket_pmt4;
	int numpixels;
	int numshorts;
	int ndatapoints;
	unsigned short* pmt1dataptr;
	unsigned short* pmt2dataptr;
	unsigned short* pmt3dataptr;
	unsigned short* pmt4dataptr;
	static int testcounter=0;
	long errcode;
	char* rawfilename;
//	unsigned long reply;
//	int i;
	
	result = VUPCI_Read_Buffer((unsigned short*)Samplebuffer,bufsize);
	if( result<0){
				//err
		//SetPMTState(PMT_ERROR);
		readerror=1;
		errcode=~result;
		//error, stop polling
		//inform task controller
	//	AbortTaskControlExecution(gtaskControl);
		TaskControlIterationDone (gtaskControl, -1, "Read Error");   
		nrsamples=0;
		//HWError();
		
	}
	else {
		if (result==0){
			//no data, just a timeout
			result=0;
		}
		else{
			//test data on tokens
			numshorts=bufsize/2;
		/*	for (i=0;i<numshorts;i++){
				if (Samplebuffer[i]>65530){
					//found a token.. program should'n break here!!
					break;
				}
			}		  */
	
			// deinterlace data here
			numpixels=result/8;  //8 bytes per pixel, result gives number of bytes
			numshorts=result/2;
			ndatapoints=numshorts/4;
			
			
			//transpose data array
			TransposeData(Samplebuffer,VAL_SHORT_INTEGER,numshorts,4);
			pmt1dataptr=malloc(ndatapoints*sizeof(unsigned short));
			pmt2dataptr=malloc(ndatapoints*sizeof(unsigned short)); 
			pmt3dataptr=malloc(ndatapoints*sizeof(unsigned short)); 
			pmt4dataptr=malloc(ndatapoints*sizeof(unsigned short)); 
			memcpy(pmt1dataptr,&Samplebuffer[0],ndatapoints*sizeof(unsigned short));
		//	memcpy(pmt2dataptr,&Samplebuffer[ndatapoints],ndatapoints*sizeof(unsigned short)); 
		//	memcpy(pmt3dataptr,&Samplebuffer[2*ndatapoints],ndatapoints*sizeof(unsigned short)); 
		//	memcpy(pmt4dataptr,&Samplebuffer[3*ndatapoints],ndatapoints*sizeof(unsigned short)); 
		  
			
			
			//test
			rawfilename=malloc(MAXCHAR*sizeof(char));
			Fmt (rawfilename, "%s<%srawdata_%i.bin", rawfilepath,iterationnr); 
			ArrayToFile(rawfilename,pmt1dataptr,VAL_SHORT_INTEGER,ndatapoints,1,VAL_GROUPS_TOGETHER,VAL_GROUPS_AS_COLUMNS,VAL_SEP_BY_TAB,0,VAL_BINARY,VAL_APPEND);
			free(rawfilename);
			
			//send datapackets
		//	if(init_DataPacket_type(VData_USHORT_Waveform, sizeof(unsigned short), ndatapoints, pmt1dataptr, NULL, 1, &datapacket_pmt1) < 0) return -1;  //insert saved IDstr and function
		//	datapacket_pmt1.imagetype=gimagetype;
		//	datapacket_pmt1.idstr=StrDup(gIDstr);
			
//			err=SendData(ActiveTaskIndex, PMT1_SOURCE, &datapacket_pmt1);
//			if (err<0){
//				MessagePopup("Error","Sending PMT Data");
//			}
			//ProcessSystemEvents();
			// CHECK HERE IF SendData IS NEGATIVE AND STOP TASKS IF SUCH ERROR OCCURS, THEN RELEASE DATA PACKETS LEFT IN THE QUEUES
		
		//	if(init_DataPacket_type(VData_USHORT_Waveform,, sizeof(short), ndatapoints, pmt2dataptr, NULL, 1, &datapacket_pmt2) < 0) return -1;  
		//	SendData(ActiveTaskIndex, PMT1_SOURCE, &datapacket_pmt2);
		//	if(init_DataPacket_type(VData_USHORT_Waveform,, sizeof(short), ndatapoints, pmt3dataptr, NULL, 1, &datapacket_pmt3) < 0) return -1;  
		//	SendData(ActiveTaskIndex, PMT1_SOURCE, &datapacket_pmt3);
		//	if(init_DataPacket_type(VData_USHORT_Waveform,, sizeof(short), ndatapoints, pmt4dataptr, NULL, 1, &datapacket_pmt4) < 0) return -1;  
		//	SendData(ActiveTaskIndex, PMT1_SOURCE, &datapacket_pmt4);
	
			OKfree(pmt1dataptr);
			OKfree(pmt2dataptr);
			OKfree(pmt3dataptr);
			OKfree(pmt4dataptr);	
		
			if(measurementmode==MEASMODE_FINITE){		 //need to count samples 
				//data is in pixels
				nrsamples=nrsamples+numpixels;
				//determine if iteration has completed
				if (nrsamples>=nrsamples_in_iteration){
					//iteration is done
					
					nrsamples=0;
					TaskControlIterationDone (gtaskControl, 0, NULL);   
					PMTStopAcq();       
				}
			}
		
		}
		
		   
		
	}
	
	return result;
}


int PMT_SetMode (int PMTnr, PMT_Mode_type mode)  
{
	int error = 0;  
	unsigned long controlreg;
	
	controlreg=GetControlreg();
	if(mode==PMT_MODE_ON){
		switch(PMTnr){
			case PMT1:
				controlreg=controlreg|PMT1HV_BIT;  //set bit
				break;
			case PMT2:
				controlreg=controlreg|PMT2HV_BIT;
				break;
			case PMT3:
				controlreg=controlreg|PMT3HV_BIT;
				break;
			case PMT4:
				controlreg=controlreg|PMT4HV_BIT;
				break;
		}
	}
	else {
		switch(PMTnr){
			case PMT1:
				controlreg=controlreg&(~PMT1HV_BIT);   //clear bit
				break;
			case PMT2:
				controlreg=controlreg&(~PMT2HV_BIT);
				break;
			case PMT3:
				controlreg=controlreg&(~PMT3HV_BIT);
				break;
			case PMT4:
				controlreg=controlreg&(~PMT4HV_BIT);
				break;
		}
	}
		//write control register
	error=WritePMTReg(CTRL_REG,controlreg);      
	
	return error;
}


int PMT_SetFan (int PMTnr, BOOL value)  
{
	int error = 0; 
	unsigned long controlreg;
	
	controlreg=GetControlreg();
	
	if(value){				//Fan On
		switch(PMTnr){
			case PMT1:
				controlreg=controlreg|PMT1FAN_BIT;  //set bit
				break;
			case PMT2:
				controlreg=controlreg|PMT2FAN_BIT;
				break;
			case PMT3:
				controlreg=controlreg|PMT3FAN_BIT;
				break;
			case PMT4:
				controlreg=controlreg|PMT4FAN_BIT;
				break;
		}
	}
	else {
		switch(PMTnr){
			case PMT1:
				controlreg=controlreg&(~PMT1FAN_BIT);   //clear bit
				break;
			case PMT2:
				controlreg=controlreg&(~PMT2FAN_BIT);
				break;
			case PMT3:
				controlreg=controlreg&(~PMT3FAN_BIT);
				break;
			case PMT4:
				controlreg=controlreg&(~PMT4FAN_BIT);
				break;
		}
	}
	//write control register
	error=WritePMTReg(CTRL_REG,controlreg);        
	
	return error;
}


int PMT_SetCooling (int PMTnr, BOOL value)  
{
	int error = 0; 
	unsigned long controlreg;
	
	controlreg=GetControlreg();
	
	if(value){			//Peltier ON
		switch(PMTnr){
			case PMT1:
				controlreg=controlreg|PMT1PELT_BIT;  //set bit
				break;
			case PMT2:
				controlreg=controlreg|PMT2PELT_BIT;
				break;
			case PMT3:
				controlreg=controlreg|PMT3PELT_BIT;
				break;
			case PMT4:
				controlreg=controlreg|PMT4PELT_BIT;
				break;
		}
	}
	else {
		switch(PMTnr){
			case PMT1:
				controlreg=controlreg&(~PMT1PELT_BIT);   //clear bit
				break;
			case PMT2:
				controlreg=controlreg&(~PMT2PELT_BIT);
				break;
			case PMT3:
				controlreg=controlreg&(~PMT3PELT_BIT);
				break;
			case PMT4:
				controlreg=controlreg&(~PMT4PELT_BIT);
				break;
		}
	}
	//write control register
	error=WritePMTReg(CTRL_REG,controlreg);        
	
	return error;
}


///  HIFN  Sets the PMT gain
///  HIPAR PMTnr/PMT number,PMTGain /Gain, range ......
///  HIRET returns error, no error when 0
int PMT_SetGainTresh(int PMTnr,unsigned int PMTGain,unsigned int PMTThreshold)
{
	int error=0;
	
	unsigned long combinedval;
	unsigned long THMASK=0x0000FFFF;
	unsigned long GAINMASK=0xFFFF0000; 
	unsigned long controlreg;
	
	controlreg=GetControlreg();
 

	combinedval=((PMTGain<<16)&GAINMASK)+(PMTThreshold&THMASK);   
	
	switch(PMTnr){
		case PMT1:
			error=WritePMTReg(PMT1_CTRL_REG,combinedval);  
			//write control register
			controlreg=controlreg|UPDPMT12_BIT; //set bit  
		//	controlreg=controlreg&~APPSTART_BIT;  //clear bit 
			//write control register
			error=WritePMTReg(CTRL_REG,controlreg);   
			controlreg=controlreg&~UPDPMT12_BIT;  //clear bit 
			break;
		case PMT2:
			error=WritePMTReg(PMT2_CTRL_REG,combinedval);  
				//write control register
			controlreg=controlreg|UPDPMT12_BIT;  //set bit 
			//write control register
			error=WritePMTReg(CTRL_REG,controlreg);   
			controlreg=controlreg&~UPDPMT12_BIT;  //clear bit 
			break;
		case PMT3:
			error=WritePMTReg(PMT3_CTRL_REG,combinedval);
				//write control register
			controlreg=controlreg|UPDPMT34_BIT;  //set bit   
			//write control register
			error=WritePMTReg(CTRL_REG,controlreg);   
			controlreg=controlreg&~UPDPMT34_BIT;  //clear bit 
			break;
		case PMT4:
			error=WritePMTReg(PMT4_CTRL_REG,combinedval); 
			//write control register
			controlreg=controlreg|UPDPMT34_BIT;  //set bit   
			//write control register
			error=WritePMTReg(CTRL_REG,controlreg);   
			controlreg=controlreg&~UPDPMT34_BIT;  //clear bit 
			break;
	}
	return error;
}


int PMT_SetTestMode(BOOL testmode)
{
	int error=0;
	unsigned long controlreg;
	
	controlreg=GetControlreg();
	
	if (testmode) controlreg=controlreg|TESTMODE0_BIT;  //set bit
	else controlreg=controlreg&(~TESTMODE0_BIT);  //clear bit 
	//write control register
	error=WritePMTReg(CTRL_REG,controlreg); 
	
	return error;
}

 

int PMTReadActions(unsigned short int* samplebuffer,int numbytes)
{
	//int i;
//	int err;
	int result;
	
	
	//prep buffer
//	for (i=0;i<bufsize;i++) samplebuffer[i]=0;
	//read buffer
	result = ReadBuffer(samplebuffer,numbytes);
	return result;
}



void SetNewBuffer(void)
{
	int bufsize;

	bufsize=GetPMTBufsize();
	
	OKfree(Samplebuffer); 
	Samplebuffer = malloc(bufsize); 
	SetPMTnewbufsizeflag(0); 
}


/// HIFN  starts the PMT Controller Acquisition
/// HIRET returns error, no error when 0
int PMTStartAcq(Measurement_type mode,int iternr,TaskControl_type* taskControl)
{
	int error=0;
	unsigned long controlreg;  
	
	gtaskControl=taskControl;
	
	SetMeasurementMode(mode);
	SetCurrentIterationnr(iternr); 
	error=PMTReset();   
	
	errChk(CmtNewThreadPool(NUMTHREADS,&poolHandle)); 
	errChk(StartDAQThread(poolHandle));
	
	
	
	//test
//	VUPCI_Close(); 
//	errChk(VUPCI_Open());  
	
	controlreg=GetControlreg();  
	//set app start bit  
//	controlreg=controlreg|DMASTART_BIT; 
	controlreg=controlreg|APPSTART_BIT;
	error=WritePMTReg(CTRL_REG,controlreg);
	
	while (GetReadyForReading()==0) {
		  //wait
		  ProcessSystemEvents();
	}
	
	VUPCI_Start_DMA();  
	readdata=1;   //start reading
	
	
Error:
	if (error<0){
		// break test!!!
		error=-1;
	}
	return error;
}																																				   

///  HIFN  stops the PMT Controller Acquisition
///  HIRET returns error, no error when 0
int PMTStopAcq(void)
{
	int error=0;
	unsigned long controlreg;    
	
	readdata=0;  //stop reading  
	VUPCI_Stop_DMA();
	
	controlreg=GetControlreg();  
	//set app start bit  
//	controlreg=controlreg|DMASTART_BIT; 
	controlreg=controlreg&~APPSTART_BIT; //clear appstart bit
	error=WritePMTReg(CTRL_REG,controlreg);
	
	errChk(StopDAQThread(poolHandle));
	CmtDiscardThreadPool(poolHandle);
	
	
//	controlreg=GetControlreg();  
//	controlreg=controlreg&~DMASTART_BIT;
//	controlreg=controlreg&~APPSTART_BIT;
//	error=WritePMTReg(CTRL_REG,controlreg);
	
Error:

	return error;
}


///  HIFN  resets the PMT Controller 
 /// HIRET returns error, no error when 0
int PMTReset(void)
{
	int error=0;
	unsigned long controlreg;

	//error=ReadPMTReg(CTRL_REG,controlreg);
//	controlreg=0;   //clear control reg 
	controlreg=GetControlreg();
	//set reset bit   
	controlreg=controlreg|RESET_BIT; 
	error=WritePMTReg(CTRL_REG,controlreg);
	//clear reset bit
	controlreg=controlreg&~RESET_BIT;
	error=WritePMTReg(CTRL_REG,controlreg); 
	
	
	return error;
}

//can only be called when acq is finished
int PMTClearFifo(void)
{
	int error=0;
	unsigned long controlreg;
	
	controlreg=GetControlreg();
	//set fifo reset bit
	controlreg=controlreg|FFRESET_BIT;
	error=WritePMTReg(CTRL_REG,controlreg);  
	
	//clear fifo reset bit
	controlreg=controlreg&~FFRESET_BIT;
	error=WritePMTReg(CTRL_REG,controlreg);
	
	//reset DMAChannel
	error=WritePMTReg(USCTRL,0x0200000A);       
	//  reset fifo's
	error=WritePMTReg(EBCR_REG,0x0A);    
	
	
	
	//here?
	nrsamples=0;
	
	return error;
}

int PMT_ClearControl(int PMTnr)
{
	int error=0;
	unsigned long controlreg;
	
	controlreg=GetControlreg();
	
	switch(PMTnr){
		case PMT1:
			controlreg=controlreg&~0x70000;    //clear control FAN,TEC and HV bitsfor PMT1
			break;
		case PMT2:
			controlreg=controlreg&~0x700000;   //clear control FAN,TEC and HV bitsfor PMT2        
			break;
		case PMT3:
			controlreg=controlreg&~0x7000000;  //clear control FAN,TEC and HV bitsfor PMT3       
			break;
		case PMT4:
			controlreg=controlreg&~0x70000000; //clear control FAN,TEC and HV bitsfor PMT4       
			break;
	}
	error=WritePMTReg(CTRL_REG,controlreg);
	
	return error;
}


int StartDAQThread(CmtThreadPoolHandle poolHandle)
{
	int error=0;
	
//	parentThreadID=CmtGetCurrentThreadID (); 
	
	SetAcqBusy(1);   
	
	error=CmtScheduleThreadPoolFunctionAdv(poolHandle,PMTThreadFunction,NULL,THREAD_PRIORITY_NORMAL,NULL,0,NULL,0,NULL);   	   //&PMTThreadFunctionID
	if (error<0) return error;  
	
	ProcessSystemEvents();  //to start the tread functions      
	
	
	//InitializePMTstate();  
Error:
	
	return error;
}



int StopDAQThread(CmtThreadPoolHandle poolHandle)
{
	int error=0;	
//	int DAQThreadFunctionStatus;
//	int PMTThreadFunctionStatus;   
//	int PMTSyncThreadFunctionStatus;   
//	char test[255];
	
//	error=PMTController_Finalize();
	
//	CmtGetThreadPoolFunctionAttribute(poolHandle,PMTThreadFunctionID,ATTR_TP_FUNCTION_EXECUTION_STATUS,&PMTThreadFunctionStatus);
//	if(PMTThreadFunctionStatus!=kCmtThreadFunctionComplete) 
//		CmtWaitForThreadPoolFunctionCompletion(poolHandle,PMTThreadFunctionID,OPT_TP_PROCESS_EVENTS_WHILE_WAITING);
//	CmtReleaseThreadPoolFunctionID(poolHandle,PMTThreadFunctionID);
	

	//UninitializePMTstate();     
	
	SetAcqBusy(0); 
	
	return error;
}


//thread function,
//run pmt read actions in this separate thread to prevent other daq tasks from underrunning
int CVICALLBACK PMTThreadFunction(void *(functionData))
{
	int result; 
	int numbytes=0x100000;  //default
	int abort=0;
	
	PMTThreadID=CmtGetCurrentThreadID ();  
	readerror=0;    

	SetReadyForReading(1);
	while (GetAcqBusy()==1)
	{
		if (GetPMTnewbufsizeflag()==1) {
			SetNewBuffer();
			numbytes=GetPMTBufsize();     
		}
		if ((readdata)&&(!readerror)) {
			result=PMTReadActions(Samplebuffer,numbytes);
		}
		if (gtaskControl!=NULL) {
			abort=GetTaskControlAbortIterationFlag(gtaskControl);  
			if (abort) {
				TaskControlIterationDone (gtaskControl, 0,NULL);
				SetAcqBusy(0);
			}
		}
    }
	SetReadyForReading(0);
    //quit
	SetPMTCommandFlag(0); 
	
	return 0;
}




