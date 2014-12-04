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
#include "NIDAQmxManager.h"

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
TaskControl_type*	gtaskControl;
Channel_type*		gchannels[MAX_CHANNELS];


//int              		QuitFlag;
//unsigned short int* 	Samplebuffer	= NULL;
//unsigned short int* 	Samplebuffer2	= NULL;  
//unsigned short* pmt1dataptr;
//unsigned short* pmt2dataptr;
//unsigned short* pmt3dataptr;
//unsigned short* pmt4dataptr;

int         			measurementmode =MeasFinite;  //default
int 		 			bufsize; 
CmtThreadPoolHandle 	poolHandle		= 0;


//DefineThreadSafeScalarVar(int,PMTstate,0); 
DefineThreadSafeScalarVar(int,PMTCommandFlag,0);				    //do a PMT controller register command in pmt thread
DefineThreadSafeScalarVar(unsigned long,PMTControllerData,0); 	    // current PMT controller data
DefineThreadSafeScalarVar(int ,PMTBufsize,0); 	    				// pmt buffer size        
DefineThreadSafeScalarVar(unsigned int,PMTnewbufsizeflag,0); 	    // newbufsize flag
DefineThreadSafeScalarVar(unsigned int,AcqBusy,0); 	            // acquisition is busy 
DefineThreadSafeScalarVar(unsigned int,ReadyForReading,0); 	            // acquisition thread is ready for reading



unsigned int 	PMTThreadID; 
unsigned int 	PMTThread2ID;
//int 			PMTThreadFunctionID;
PMTregcommand 	newcommand;
int readerror=0; 
int readdata=0;

int 			nrsamples_in_iteration;   //requested amount of samples
size_t 			nrsamples;				  //number of samples measured 
int 			iterationnr;              //current iteration number, passed in data



//==============================================================================
// Global functions
int CVICALLBACK PMTThreadFunction(void *(functionData));
int CVICALLBACK PMTThreadFunction2(void *(functionData));    
int StartDAQThread(int mode,CmtThreadPoolHandle poolHandle) ;
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
	InitializePMTCommandFlag();
	InitializePMTControllerData();
	InitializePMTBufsize();   
	InitializePMTnewbufsizeflag(); 
	InitializeReadyForReading();
	
	SetPMTCommandFlag(0);
	SetPMTBufsize(0);   
	SetPMTnewbufsizeflag(0);
	SetAcqBusy(0);
	SetReadyForReading(0);
	
	errChk(VUPCI_Open());
	errChk(GetPMTControllerVersion());
	
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
int ReadBuffer(int bufsize)
{
	int error=0;
	int result;
	DataPacket_type*  dataPacket;
	FCallReturn_type* fCallReturn;
	
	unsigned short int* 	Samplebuffer	= NULL;   
	unsigned short int*     pmtdataptr;
	int numpixels;
	int numshorts;
	int ndatapoints;
	int i;
	double refSamplingRate=1000;

	long errcode;
	unsigned long statreg; 
	Waveform_type* waveform;
	
	
	//if(GetAcqBusy()==1){
	
	Samplebuffer = malloc(bufsize); 
	
	result = VUPCI_Read_Buffer(Samplebuffer,bufsize);
	
/*	error=ReadPMTReg(STAT_REG,&statreg);
	if ((statreg&FFOVERFLOW_BIT)==FFOVERFLOW_BIT) {
		readerror=1;
		errcode=~result;
		TaskControlIterationDone (gtaskControl, -1, "Fifo Overflow");   
		nrsamples=0;
		return errcode;
	}	  */
	
	if((result<0)&&(GetAcqBusy()==0)){ //only pass errors when acq is busy  
		OKfree(Samplebuffer);  
		return 0;
	}
	if (result<0){  
				//err
		//SetPMTState(PMT_ERROR);
		readerror=1;
		errcode=~result;
		//error, stop polling
		//inform task controller
		//	AbortTaskControlExecution(gtaskControl);
	//	TaskControlIterationDone (gtaskControl, -1, "Read Error",FALSE);   
	//	nrsamples=0;
		//HWError();
		
		result=0;
		//timeout occured, skip this read
		//TaskControlIterationDone (gtaskControl, -2, "Read Timeout",FALSE);  
		
	}
	else {
		if (result==0){
			//no data, just a timeout
			result=0;
			//skip this read
		//	TaskControlIterationDone (gtaskControl, -2, "Read Timeout",FALSE);   
		}
		else{
			// deinterlace data here
			numpixels=result/8;  //8 bytes per pixel, result gives number of bytes
			numshorts=result/2;
			ndatapoints=numshorts/4;
			
			
			//transpose data array
			TransposeData(Samplebuffer,VAL_SHORT_INTEGER,numshorts,4);
	
			
			//send datapackets
			//index i is index of active channels; chanIdx contains the PMT channel index
			
			for (i=0;i<MAX_CHANNELS;i++){
				if (gchannels[i]!=NULL){
					if (gchannels[i]->VChan!=NULL){
						pmtdataptr = malloc(ndatapoints*sizeof(unsigned short));
						memcpy(pmtdataptr,&Samplebuffer[i*ndatapoints],ndatapoints*sizeof(unsigned short));
						waveform = init_Waveform_type(Waveform_UShort, refSamplingRate, ndatapoints, &pmtdataptr);  
					    dataPacket = init_DataPacket_type(DL_Waveform_UShort, waveform, (DiscardPacketDataFptr_type) discard_Waveform_type);       
						// send data packet with waveform
						fCallReturn = SendDataPacket(gchannels[i]->VChan, dataPacket, 0);
						discard_FCallReturn_type(&fCallReturn);
					
					}
				}
			}
			
			if(measurementmode==MeasFinite){		 //need to count samples 
				//data is in pixels
				nrsamples=nrsamples+numpixels;
				//determine if iteration has completed
				if (nrsamples>=nrsamples_in_iteration){
					//iteration is done
					
					nrsamples=0;
					
					//send null packet(s)
					for (i=0;i<MAX_CHANNELS;i++){   
						if (gchannels[i]!=NULL){
							if (gchannels[i]->VChan!=NULL){
							fCallReturn = SendDataPacket(gchannels[i]->VChan, NULL, 0);
							discard_FCallReturn_type(&fCallReturn);
							}
						}
					}
					
					PMTStopAcq(); 
					
					TaskControlIterationDone (gtaskControl, 0, NULL,FALSE);   
					     
				}
			}
		
		}
		
		   
		
	}
	
	OKfree(Samplebuffer);
	
	return result;
}



int PMT_SetMode (int PMTnr, PMT_Mode_type mode)  
{
	int error = 0;  
	unsigned long controlreg;
	
	error=ReadPMTReg(CTRL_REG,&controlreg);
	
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
	
	
	error=ReadPMTReg(CTRL_REG,&controlreg);    
	
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
	
	error=ReadPMTReg(CTRL_REG,&controlreg);    
	
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
	
	error=ReadPMTReg(CTRL_REG,&controlreg);    
 

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
	
	error=ReadPMTReg(CTRL_REG,&controlreg);    
	
	if (testmode) controlreg=controlreg|TESTMODE0_BIT;  //set bit
	else controlreg=controlreg&(~TESTMODE0_BIT);  //clear bit 
	//write control register
	error=WritePMTReg(CTRL_REG,controlreg); 
	
	return error;
}

 


/// HIFN  starts the PMT Controller Acquisition
/// HIRET returns error, no error when 0
int PMTStartAcq(MeasMode_type mode,int iternr,TaskControl_type* taskControl,Channel_type** channels)
{
	int error=0;
	unsigned long controlreg;
	int i;
	
	gtaskControl=taskControl;		  
	for (i=0;i<MAX_CHANNELS;i++){
		if (channels[i]!=NULL) {
			gchannels[i]=malloc(sizeof(Channel_type));
		 	*gchannels[i]=*channels[i];
		}
		else gchannels[i]=NULL;
	}
	
	SetMeasurementMode(mode);
	SetCurrentIterationnr(iternr); 
	error=PMTClearFifo(); 
	
	
	errChk(CmtNewThreadPool(NUMTHREADS,&poolHandle)); 
	errChk(StartDAQThread(mode,poolHandle));
	
	
	readdata=1;   //start reading       
	
	error=ReadPMTReg(CTRL_REG,&controlreg);     
	//set app start bit  
	controlreg=controlreg|APPSTART_BIT;
	error=WritePMTReg(CTRL_REG,controlreg);
	
	while (GetReadyForReading()==0) {
		  //wait until thread has started
		  ProcessSystemEvents();
	}
	

	
	
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
	int i;
	
	readdata=0;  //stop reading  
	

	
	errChk(StopDAQThread(poolHandle));

	
	for (i=0;i<MAX_CHANNELS;i++){
		OKfree(gchannels[i]);
	}
	
	//tell hardware to stop
	error=ReadPMTReg(CTRL_REG,&controlreg);    
	//set app start bit  
	controlreg=controlreg&~APPSTART_BIT; //clear appstart bit
	error=WritePMTReg(CTRL_REG,controlreg);
	
	
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
	controlreg=0;   //clear control reg 
//	error=ReadPMTReg(CTRL_REG,&controlreg);    
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
	unsigned long statreg;
	
//	error=ReadPMTReg(CTRL_REG,&controlreg);    
	//set fifo reset bit
//	controlreg=controlreg|FFRESET_BIT;
//	error=WritePMTReg(CTRL_REG,controlreg);  
	
	//clear fifo reset bit
//	controlreg=controlreg&~FFRESET_BIT;
//	error=WritePMTReg(CTRL_REG,controlreg);
	
	//reset DMAChannel
	error=WritePMTReg(USCTRL,0x0200000A);       
	//  reset fifo's
	error=WritePMTReg(EBCR_REG,0x0A);    
	

	error=ReadPMTReg(STAT_REG,&statreg);
		//clear fifo status bits if set
	if ((statreg&FFALMFULL_BIT)||(statreg&FFOVERFLOW_BIT)){ 
			if (statreg&FFALMFULL_BIT) statreg=statreg&~FFALMFULL_BIT;  //clear bit
			if (statreg&FFOVERFLOW_BIT) statreg=statreg&~FFOVERFLOW_BIT;  //clear bit    
			error=WritePMTReg(STAT_REG,statreg);    
	}
	
	
	
	//here?
	nrsamples=0;
	
	return error;
}

int PMT_ClearControl(int PMTnr)
{
	int error=0;
	unsigned long controlreg;
	
	error=ReadPMTReg(CTRL_REG,&controlreg);    
	
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


int StartDAQThread(int mode,CmtThreadPoolHandle poolHandle)
{
	int error=0;
	
	
	SetAcqBusy(1);   
	
	error=CmtScheduleThreadPoolFunctionAdv(poolHandle,PMTThreadFunction,NULL,THREAD_PRIORITY_NORMAL,NULL,0,NULL,0,NULL);   	   //&PMTThreadFunctionID
	if (error<0) return error;
	//only launch second acq thread in movie mode
	if (mode==MeasCont){
		error=CmtScheduleThreadPoolFunctionAdv(poolHandle,PMTThreadFunction2,NULL,THREAD_PRIORITY_NORMAL,NULL,0,NULL,0,NULL);   	   //&PMTThreadFunctionID
		if (error<0) return error; 
	}
	ProcessSystemEvents();  //to start the tread functions      
	
	
	//InitializePMTstate();  
Error:
	
	return error;
}



int StopDAQThread(CmtThreadPoolHandle poolHandle)
{
	int error=0;	
	int DAQThreadFunctionStatus;
	int PMTThreadFunctionStatus;   
	int PMTSyncThreadFunctionStatus;   
//	char test[255];
	
//	error=PMTController_Finalize();
	
	
	

	//UninitializePMTstate();     
	
	SetAcqBusy(0);  //should stop the acq thread 
	
//	CmtGetThreadPoolFunctionAttribute(poolHandle,PMTThreadFunctionID,ATTR_TP_FUNCTION_EXECUTION_STATUS,&PMTThreadFunctionStatus);
//	if(PMTThreadFunctionStatus!=kCmtThreadFunctionComplete) 
//		CmtWaitForThreadPoolFunctionCompletion(poolHandle,PMTThreadFunctionID,OPT_TP_PROCESS_EVENTS_WHILE_WAITING);
//	CmtReleaseThreadPoolFunctionID(poolHandle,PMTThreadFunctionID);
	
	return error;
}


//thread function,
//run pmt read actions in this separate thread to prevent other daq tasks from underrunning
int CVICALLBACK PMTThreadFunction(void *(functionData))
{
	int result; 
	int abort=0;
	
	PMTThreadID=CmtGetCurrentThreadID ();  
	readerror=0;    

	SetReadyForReading(1);
	while (GetAcqBusy()==1)
	{
	//	if (GetPMTnewbufsizeflag()==1) {
	//		SetNewBuffer();
	//		numbytes=GetPMTBufsize();     
	//	}
		if ((readdata)&&(!readerror)) {
			result=ReadBuffer(GetPMTBufsize());
		}
		if (gtaskControl!=NULL) {
			abort=GetTaskControlAbortIterationFlag(gtaskControl);  
			if (abort) {
				TaskControlIterationDone (gtaskControl, 0,NULL,FALSE);
				SetAcqBusy(0);
			}
		}
    }
	SetReadyForReading(0);
    //quit
	SetPMTCommandFlag(0); 
	CmtDiscardThreadPool(poolHandle); 
	
	return 0;
}


//thread function,
//run pmt read actions in this separate thread to prevent other daq tasks from underrunning
int CVICALLBACK PMTThreadFunction2(void *(functionData))
{
	int result;
	
	PMTThread2ID=CmtGetCurrentThreadID ();  
	
//parallel thread requesting data when in movie mode
	while (GetAcqBusy()==1)     
	{
		if (measurementmode !=MeasFinite) {
			result=ReadBuffer(GetPMTBufsize());     
		}
		ProcessSystemEvents();
    }
    //quit

	return 0;
}




