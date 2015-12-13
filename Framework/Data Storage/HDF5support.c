//==============================================================================
//
// Title:		HDF5support
// Purpose:		A short description of the application.
//
// Created on:	25-3-2015 at 9:41:17 by Adrian Negrean.
// Copyright:	Vrije Universiteit. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include <formatio.h>
#include <cvirte.h>		
#include <userint.h>
#include "toolbox.h"
#include "hdf5.h"
#include "hdf5_hl.h"
#include "Iterator.h"
#include "HDF5support.h"
#include "DAQLabErrHandling.h"
#include <stdio.h>
#include <stdlib.h>
								   

#ifndef hdf5ErrChk
#define hdf5ErrChk(fCall) if (errorInfo.error = (fCall), errorInfo.line = __LINE__, errorInfo.error < 0) {goto HDF5Error;} else
#endif

	
//==============================================================================
// Constants
#define MAXITERDEPTH 				100
#define MAXCHAR		 				260
	
#define IMAGE1_NAME  				"image8bit"
#define NUMELEMENTS_NAME 			"NumElements"
#define PIXELSIZE_NAME				"element_size_um"
#define DATETIME_FORMATSTRING		"%c"


// compression
#define GZIP_CompressionLevel		6		// Sets GNU ZIP compression level. 0 - no compression, 9 - maximum compression
#define SZIP_PixelsPerBlock			16

//==============================================================================
// Types
	
typedef struct IteratorData		IteratorData_type; 
	
struct IteratorData {
	char*					name;					// Iterator name.  
	size_t					currentIterIdx; 		// 0-based iteration index used to iterate over iterObjects
	BOOL					stackdata;
};


//==============================================================================
// Static global variables

//==============================================================================
// Static functions

	//----------------------------------
	// Groups
	//----------------------------------
static int 						CreateRootGroup 					(hid_t fileID, char *group_name, char** errorMsg);
static int 						CreateRelativeGroup 				(hid_t parentgroupID, char *group_name, char** errorMsg);
static int 						CreateHDF5Group 					(char* fileName, DSInfo_type* dsInfo, hid_t* fileID, hid_t* groupID, char** errorMsg);

	//----------------------------------
	// Attributes
	//----------------------------------

static int 						CreateStringAttr 					(hid_t datasetID, char attr_name[], char* attr_data, char** errorMsg);
static int 						CreateLongAttr						(hid_t datasetID, char attr_name[], long attr_data, char** errorMsg);
static int 						CreateULongAttr 					(hid_t datasetID, char attr_name[], unsigned long attr_data, char** errorMsg);
static int 						CreateLLongAttr 					(hid_t datasetID, char attr_name[], long long attr_data, char** errorMsg);
static int 						CreateULLongAttr 					(hid_t datasetID, char attr_name[], unsigned long long attr_data, char** errorMsg);
static int 						CreateULLongAttrArr 				(hid_t datasetID, char attr_name[], unsigned long long attr_data, size_t size, char** errorMsg);
static int 						ReadNumElemAttr 					(hid_t datasetID, hsize_t index, unsigned long long* attr_data, size_t size, char** errorMsg);
static int 						WriteNumElemAttr 					(hid_t datasetID, hsize_t index, unsigned long long attr_data, hsize_t size, char** errorMsg);
static int 						CreateIntAttr 						(hid_t datasetID, char attr_name[], int attr_data, char** errorMsg);
static int 						CreateUIntAttr 						(hid_t datasetID, char attr_name[], unsigned int attr_data, char** errorMsg);
static int 						CreateShortAttr 					(hid_t datasetID, char attr_name[], long attr_data, char** errorMsg);
static int 						CreateUShortAttr 					(hid_t datasetID, char attr_name[], unsigned long attr_data, char** errorMsg);
static int 						CreateFloatAttr						(hid_t datasetID, char attr_name[], float attr_data, char** errorMsg);
static int 						CreateDoubleAttr 					(hid_t datasetID, char attr_name[], double attr_data, char** errorMsg);
static int 						CreateDoubleAttrArr					(hid_t datasetID, char attr_name[], double attr_data, hsize_t size, char** errorMsg);

	// Waveforms
static int 						AddWaveformAttr 					(hid_t datasetID, Waveform_type* waveform, char** errorMsg);
static int 						AddWaveformAttrArr 					(hid_t datasetID, unsigned long long numelements, hsize_t size, char** errorMsg);

	// Images
static int 						CreatePixelSizeAttr 				(hid_t datasetID, double* attr_data, char** errorMsg);
static int 						AddImagePixSizeAttributes 			(hid_t datasetID, Image_type* image, char** errorMsg);
static int 						AddImageAttributes					(hid_t datasetID, Image_type* image, hsize_t size, char** errorMsg);

	// ROIs (regions of interest)
//static int						


	//----------------------------------
	// Utility
	//----------------------------------
static char* 					RemoveSlashes 						(char* name);
	// coverts waveform data types to HDF5 data types
static void						WaveformDataTypeToHDF5				(WaveformTypes waveformType, hid_t* typeIDPtr, hid_t* memTypeIDPtr);


//==============================================================================
// Global variables

//==============================================================================
// Global functions

int CreateHDF5File(char fileName[], char datasetName[], char** errorMsg) 
{
INIT_ERR

	hid_t       fileID		= 0;   // file identifier
	
    // create a new file using default properties
	hdf5ErrChk( fileID = H5Fcreate(fileName, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT) );
	
    // terminate access to the file
    hdf5ErrChk( H5Fclose(fileID) ); 

HDF5Error:
	
Error:
	
RETURN_ERR
}

int WriteHDF5Waveform (char fileName[], char datasetName[], DSInfo_type* dsInfo, Waveform_type* waveform, CompressionMethods compression, char** errorMsg) 
{
INIT_ERR

	//=============================================================================================
	// INITIALIZATION (no fail)
	//=============================================================================================

	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	// data
	
	size_t 					nElem					= 0;	
	void* 					waveformData			= *(void**)GetWaveformPtrToData(waveform, &nElem);
	unsigned int			dataRank				= GetDSDataRank(dsInfo);	 		// Data rank, 1 for waveforms, 2 for images.
	unsigned int 			indicesRank				= GetDSInfoDatasetRank(dsInfo);  	// Dataset rank determined by the experiment, equals number of indices.
	unsigned int 			totalRank				= indicesRank + dataRank; 			// Number of dimensions used in the HDF5 dataspace.
	unsigned int*			indices					= GetDSInfoIterIndices(dsInfo);   
   
	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	// HDF5
	
	hid_t       			fileID					= 0;
	hid_t        			fileSpaceID				= 0;
	hid_t					groupID					= 0;
	hid_t					datasetID				= 0;
	hid_t					dataSpaceID				= 0;
	hid_t					memSpaceID				= 0;
	hid_t 					memTypeID 				= 0;
	hid_t 					typeID					= 0;
	hid_t       			propertyListID			= 0; 
	
	hsize_t*				dims 				    = NULL;
	hsize_t*      			maxDims 				= NULL;
	hsize_t*				dimsr 					= NULL;
	hsize_t* 				offset					= NULL;
	hsize_t*      			size					= NULL;
	
	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	
	BOOL					are_equal				= TRUE; 
	unsigned long long   	numelements				= 0;
	
	//=============================================================================================
	// MEMORY ALLOCATION (can fail)
	//=============================================================================================
 
	// mem alloc
	nullChk( dims		= malloc(totalRank * sizeof(hsize_t)) );
	nullChk( maxDims	= malloc(totalRank * sizeof(hsize_t)) );
	nullChk( dimsr		= malloc(totalRank * sizeof(hsize_t)) );
	nullChk( offset		= malloc(totalRank * sizeof(hsize_t)) );
	nullChk( size		= malloc(totalRank * sizeof(hsize_t)) );
	
	// open the file and the group or creates one if necessary
	errChk( CreateHDF5Group(fileName, dsInfo, &fileID, &groupID, &errorInfo.errMsg) );
	
	// init dataspace dimensions
	for(size_t i = 0; i < totalRank; i++) {
		dims[i] 	= 1;
		maxDims[i] 	= H5S_UNLIMITED;
	}
	dims[dataRank-1] = nElem;   // number of elements in the wavefornm = width of dataspace
	
	// modify dataset creation properties, i.e. enable chunking
	hdf5ErrChk( propertyListID = H5Pcreate (H5P_DATASET_CREATE) );
	hdf5ErrChk( H5Pset_chunk(propertyListID, totalRank, dims) );
	
	// add compression if requested
	switch (compression) {
		
		case Compression_None:
			
			break;
			
		case Compression_GZIP:
			
			hdf5ErrChk( H5Pset_deflate (propertyListID, GZIP_CompressionLevel) );
			break;
			
		case Compression_SZIP:
			
			unsigned int szipOptionsMask 		= H5_SZIP_NN_OPTION_MASK;
			unsigned int szipPixelsPerBlock  	= SZIP_PixelsPerBlock;
			
			hdf5ErrChk( H5Pset_szip (propertyListID, szipOptionsMask, szipPixelsPerBlock) );
			break;
	}
  
	// convert waveform type to HDF5 types
	WaveformDataTypeToHDF5(GetWaveformDataType(waveform), &typeID, &memTypeID); 
   
	// create data space
	hdf5ErrChk( dataSpaceID = H5Screate_simple(totalRank, dims, maxDims) );
	
	// dataset name shouldn't have slashes in it
	datasetName = RemoveSlashes(datasetName);
	
	// Open the dataset if it exists
	datasetID = H5Dopen2(groupID, datasetName, H5P_DEFAULT );
	if (datasetID < 0) {
		//-----------------------------------------------
		// Dataset doesn't exist. A new one is created
		//-----------------------------------------------
		
		hdf5ErrChk( datasetID = H5Dcreate2(groupID, datasetName, typeID, dataSpaceID,H5P_DEFAULT, propertyListID,H5P_DEFAULT) );
		
		// write the dataset
   		hdf5ErrChk( H5Dwrite(datasetID, memTypeID, H5S_ALL, H5S_ALL, H5P_DEFAULT, waveformData) );
		
   		// add attributes to dataset
   		errChk( AddWaveformAttr(datasetID, waveform, &errorInfo.errMsg) );
		errChk( AddWaveformAttrArr(datasetID, nElem, 1, &errorInfo.errMsg) );  
		
	} else {
		//------------------------------------------------
		// Dataset exists. Add new to the current dataset
		//------------------------------------------------
		
		// get the size of the dataset
	   	hdf5ErrChk( fileSpaceID = H5Dget_space (datasetID) );
		// get dataspace dimension size and maximum size. 
    	hdf5ErrChk( H5Sget_simple_extent_dims (fileSpaceID, dimsr, NULL) );
		//check if saved size equals the indices set
		//if so, add data to current set
		//copy read dims into size and offset
		
		for (size_t i = 0; i < indicesRank; i++) {
			size[dataRank+i] = indices[i] + 1;      //adjust size to indices   /indices start at zero; dimsr base is  one          
			if (size[dataRank+i] != dimsr[dataRank+i])
				are_equal = FALSE;  
		}
		
		if (are_equal){
			errChk( ReadNumElemAttr(datasetID, size[1] - 1, &numelements, size[1], &errorInfo.errMsg) );   
			//just add to current set
		
			for (size_t i = 0; i < indicesRank; i++)
				// adjust offset for multidimensional data
				offset[dataRank+i]= size[dataRank+i]-dims[dataRank+i]; 		
			
			// adjust dataset size for added data
			if (numelements + dims[0] > dimsr[0])
				size[0] = numelements + dims[0];
			else 
				size[0] = dimsr[0];
			
			// adjust offset
			offset[0] = numelements;      
			numelements += nElem;
			
    		hdf5ErrChk(H5Dset_extent (datasetID, size));
			
    		// Select a hyperslab in extended portion of dataset
    		hdf5ErrChk( fileSpaceID = H5Dget_space (datasetID) );
    	  
		} else { 
			
			//create a larger dataset and put data there
			for (size_t i = 0; i < indicesRank; i++) {       
				size[dataRank+i] = indices[i] + 1;
				offset[dataRank+i] = size[dataRank+i] - dims[dataRank+i];    	  
			}
										   
			//adjust data size  
			size[0]   	= dimsr[0];		// size of data equals previous data
			offset[0] 	= 0;			// new iteration, offset from beginning
			numelements	= nElem;    	// set data offset 
			
    		hdf5ErrChk( H5Dset_extent (datasetID, size) );

    		// Select a hyperslab in extended portion of dataset  //
    		hdf5ErrChk( fileSpaceID = H5Dget_space (datasetID) );
    		
		}
		
    	hdf5ErrChk( H5Sselect_hyperslab (fileSpaceID, H5S_SELECT_SET, offset, NULL, dims, NULL) );  

    	// Define memory space
   		memSpaceID = H5Screate_simple (totalRank, dims, NULL); 

    	// Write the data to the extended portion of dataset
    	hdf5ErrChk( H5Dwrite(datasetID, memTypeID, memSpaceID, fileSpaceID,H5P_DEFAULT, waveformData) );
		errChk( AddWaveformAttrArr(datasetID, numelements, size[1], &errorInfo.errMsg) );     
	}
   
	// Close the dataset
	hdf5ErrChk(H5Dclose(datasetID));
   
	// Close the group ids
	hdf5ErrChk(H5Gclose (groupID));
   
	// Close the dataspace
	hdf5ErrChk(H5Sclose(dataSpaceID));
   
	// Close the file
	hdf5ErrChk(H5Fclose(fileID));
   
	OKfree(dims);
	OKfree(maxDims);
	OKfree(dimsr);
	OKfree(offset);
	OKfree(size);
   
	return 0;

HDF5Error:
   
	/*
	FILE* tmpFile = tmpfile();			<--- wrong FILE type as defined by CVI's implementation of stdio compared to what H5Eprint
	H5Eprint(H5E_DEFAULT, tmpFile);
	fseek(tmpFile, 0, SEEK_END);
	long fsize = ftell(tmpFile);
	fseek(tmpFile, 0, SEEK_SET);
	errMsg = malloc((fsize+1) * sizeof(char));
	fread(errMsg, fsize, 1, tmpFile);
	fclose(tmpFile);
    ReturnErrMsg("Data Storage WriteHDF5Waveform");
	*/
	
Error:
   
	OKfree(dims);
	OKfree(maxDims);
	OKfree(dimsr);
	OKfree(offset);
	OKfree(size);
   
RETURN_ERR
}

int WriteHDF5WaveformList (char fileName[], ListType waveformList, CompressionMethods compression, char** errorMsg)
{
#define WriteHDF5WaveformList_Err_NoFileName	-1
	
INIT_ERR
	
	size_t					nWaveforms				= ListNumItems(waveformList);
	Waveform_type*			waveform				= NULL;
	void* 					waveformData			= NULL;
	char					datasetName[100]		= "";
	
	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	// HDF5
	
	hid_t       			fileID					= 0;
	hid_t 					memTypeID 				= 0;
	hid_t 					typeID					= 0;
	hid_t       			propertyListID			= 0;
	hid_t					dataSpaceID 			= 0;
	hid_t					datasetID				= 0;
	
	hsize_t					dims[1] 			    = {0};				 // number of elements in the waveform
	hsize_t      			maxDims[1] 				= {H5S_UNLIMITED};
	
	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	
	// if there are no waveforms to be saved, then do nothing
	if (!nWaveforms) return 0;
	
	// check if a file name was given
	if (!fileName || !fileName[0])
		SET_ERR(WriteHDF5WaveformList_Err_NoFileName, "No file name was provided.");
	
	// create file
   	hdf5ErrChk( fileID = H5Fcreate(fileName, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT) );
	// create dataset creation property list
	hdf5ErrChk( propertyListID = H5Pcreate (H5P_DATASET_CREATE) );
	
	// create data set for each waveform and add it to the file
	for (size_t i = 1; i <= nWaveforms; i++) {
		waveform 		= *(Waveform_type**)ListGetPtrToItem(waveformList, i);
		waveformData	= *(void**)GetWaveformPtrToData(waveform, &dims[0]); 
		
		// modify the dataset creation property list to enable chunking
		hdf5ErrChk( H5Pset_chunk(propertyListID, 1, dims) );
	
		// add compression if requested
		switch (compression) {
		
			case Compression_None:
			
				break;
			
			case Compression_GZIP:
			
				hdf5ErrChk( H5Pset_deflate (propertyListID, GZIP_CompressionLevel) );
				break;
			
			case Compression_SZIP:
			
				unsigned int szipOptionsMask 		= H5_SZIP_NN_OPTION_MASK;
				unsigned int szipPixelsPerBlock  	= SZIP_PixelsPerBlock;
			
				hdf5ErrChk( H5Pset_szip (propertyListID, szipOptionsMask, szipPixelsPerBlock) );
				break;
		}
		
		// convert waveform type to HDF5 types
		WaveformDataTypeToHDF5(GetWaveformDataType(waveform), &typeID, &memTypeID);
		
		// create data space
		hdf5ErrChk( dataSpaceID = H5Screate_simple(1, dims, maxDims) );
		
		// create data set
		Fmt(datasetName, "%d", i);
		hdf5ErrChk( datasetID = H5Dcreate(fileID, datasetName, typeID, dataSpaceID, H5P_DEFAULT, propertyListID, H5P_DEFAULT) );
		
		// add attributes to the dataset
   		errChk( AddWaveformAttr(datasetID, waveform, &errorInfo.errMsg) );
		
		// write waveform data to the dataset
   		hdf5ErrChk( H5Dwrite(datasetID, memTypeID, H5S_ALL, H5S_ALL, H5P_DEFAULT, waveformData) );
		
		// close the dataspace
		if (dataSpaceID > 0) {H5Sclose(dataSpaceID); dataSpaceID = 0;}
		// close dataset
		if (datasetID > 0) {H5Dclose(datasetID); datasetID = 0;}
	}
	
	
HDF5Error:
	

Error:
	
	// close dataset creation property list
	if (propertyListID > 0) {H5Pclose(propertyListID); propertyListID = 0;}
	// close the dataspace
	if (dataSpaceID > 0) {H5Sclose(dataSpaceID); dataSpaceID = 0;}
	// close data set
	if (datasetID > 0) {H5Dclose(datasetID); datasetID = 0;}
	
	// close file
	if (fileID > 0) {H5Fclose(fileID); fileID = 0;}
	
RETURN_ERR
}

int WriteHDF5Image(char fileName[], char datasetName[], DSInfo_type* dsInfo, Image_type* image, CompressionMethods compression, char** errorMsg) 
{ 
INIT_ERR
	
	hid_t 					fileID					= 0;
	hid_t					groupID					= 0;
	char**					groupName				= NULL; 
	hid_t       			propertyListID			= 0; 
   	hid_t 					memTypeID 				= 0;
   	hid_t 					typeID					= 0;
   	hid_t        			fileSpaceID				= 0;
   	hid_t					memSpaceID				= 0;
   	hsize_t					dimsr[3]				= {0};
   	hsize_t      			size[3]					= {0};
   	hsize_t      			offset[3]				= {0};
	hid_t					datasetID				= 0;
	hid_t					dataSpaceID				= 0;
	int 					height					= 0;
    int 					width					= 0;
	
	GetImageSize(image, &width, &height);
	
    int						numimages				= 1;  			
    void* 					DataPtr					= NULL;
    hsize_t      			imagedims[3]			= {1, height, width};
	hsize_t      			stackdims[3]			= {numimages, height, width}; 
    hsize_t      			maxstackdims[3] 		= {H5S_UNLIMITED, H5S_UNLIMITED, H5S_UNLIMITED};
	

	// opens the file and the group, 
	// creates one if necessary
	errChk( CreateHDF5Group(fileName, dsInfo, &fileID, &groupID, &errorInfo.errMsg) );
   
	ImageTypes type = GetImageType(image);
   
    // Modify dataset creation properties, i.e. enable chunking
	hdf5ErrChk( propertyListID = H5Pcreate (H5P_DATASET_CREATE) );
	hdf5ErrChk( H5Pset_chunk ( propertyListID, 3, imagedims) );
	
	// add compression if requested
	switch (compression) {
		
		case Compression_None:
			
			break;
			
		case Compression_GZIP:
			
			hdf5ErrChk( H5Pset_deflate (propertyListID, GZIP_CompressionLevel) );
			break;
			
		case Compression_SZIP:
			
			unsigned int szipOptionsMask 		= H5_SZIP_NN_OPTION_MASK;
			unsigned int szipPixelsPerBlock  	= SZIP_PixelsPerBlock;
			
			hdf5ErrChk( H5Pset_szip (propertyListID, szipOptionsMask, szipPixelsPerBlock) );
			break;
	}
  
	//datatype switch
	switch (type) {
		   
		case Image_UChar:
			
			typeID		= H5T_STD_U8BE;
		   	memTypeID	= H5T_NATIVE_UCHAR;
			break;
		   
		case Image_Short:
			
			typeID		= H5T_STD_I16BE;
			memTypeID	= H5T_NATIVE_SHORT;
			break;
		   
		case Image_UShort:
			
			typeID		= H5T_STD_U16BE;
			memTypeID	= H5T_NATIVE_USHORT;
			break;
		   
		case Image_Int:
			typeID		= H5T_STD_I32BE;
			memTypeID	= H5T_NATIVE_INT;
			break;
		   
		case Image_UInt:
			typeID		= H5T_STD_U32BE;
			memTypeID	= H5T_NATIVE_UINT;
			break;
		   
		case Image_Float:
			typeID		= H5T_IEEE_F32BE;
			memTypeID	= H5T_NATIVE_FLOAT;
			break;
	}
   
	// open an existing or new dataset
	dataSpaceID = H5Screate_simple(3, stackdims, maxstackdims);

   	// datasetname shouldn't have slashes in it
	datasetName = RemoveSlashes(datasetName);
	DataPtr = GetImagePixelArray(image);
   
	// open the dataset if it exists
 	datasetID = H5Dopen2(groupID, datasetName, H5P_DEFAULT );
	if (datasetID < 0) {
		hdf5ErrChk( datasetID = H5Dcreate2(groupID, datasetName,typeID, dataSpaceID,H5P_DEFAULT, propertyListID,H5P_DEFAULT) );
		
		// write the dataset
		hdf5ErrChk( H5Dwrite(datasetID, memTypeID, H5S_ALL, H5S_ALL, H5P_DEFAULT, DataPtr) );
   		// add attributes to dataset
		errChk( AddImagePixSizeAttributes(datasetID,image, &errorInfo.errMsg) );
		errChk( AddImageAttributes(datasetID, image, 1, &errorInfo.errMsg) ); 
  	} else {
		// dataset existed, have to add the data to the current data set
	   	// get the size of the dataset
		hdf5ErrChk( fileSpaceID = H5Dget_space (datasetID) );
		hdf5ErrChk( H5Sget_simple_extent_dims (fileSpaceID, dimsr, NULL) );
		size[0] = dimsr[0]+1;  
		size[1] = dimsr[1];  
		size[2] = dimsr[2];
		hdf5ErrChk( H5Dset_extent (datasetID, size) );

    	// select a hyperslab in extended portion of dataset
    	hdf5ErrChk( fileSpaceID = H5Dget_space (datasetID) );
    	offset[0] = dimsr[0]; 
    	offset[1] = 0;
		offset[2] = 0;  
    	hdf5ErrChk( H5Sselect_hyperslab (fileSpaceID, H5S_SELECT_SET, offset, NULL, imagedims, NULL) );

    	// define memory space
   		hdf5ErrChk( memSpaceID = H5Screate_simple (3, imagedims, NULL) ); 

    	// Write the data to the extended portion of dataset
    	hdf5ErrChk( H5Dwrite(datasetID, memTypeID, memSpaceID, fileSpaceID,H5P_DEFAULT, DataPtr) );
		errChk( AddImageAttributes(datasetID,image, size[0], &errorInfo.errMsg) );
   	}
	   
    // close the dataset
	hdf5ErrChk( H5Dclose(datasetID) );
   
	// close the group ids
	hdf5ErrChk( H5Gclose (groupID) );
   
	OKfree(groupName);
   
	// close the dataspace
	hdf5ErrChk( H5Sclose(dataSpaceID) );
	
	// close the file. */
	hdf5ErrChk( H5Fclose(fileID) );
   
HDF5Error:
	
Error:
	
RETURN_ERR
}

static int CreateRootGroup (hid_t fileID, char *group_name, char** errorMsg)
{
INIT_ERR

	hid_t		groupID	= 0;
	
	hdf5ErrChk( groupID = H5Gcreate(fileID, group_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT) );
  	hdf5ErrChk( H5Gclose (groupID) );
	
HDF5Error:
	
Error:

RETURN_ERR
}

static int CreateRelativeGroup (hid_t parentgroupID, char *group_name, char** errorMsg)
{
INIT_ERR

	hid_t		groupID	= 0;
	
	hdf5ErrChk( groupID = H5Gcreate2(parentgroupID, group_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT) );
  	hdf5ErrChk( H5Gclose (groupID) );
	
HDF5Error:
	
Error:

RETURN_ERR
}

static int CreateHDF5Group (char* fileName, DSInfo_type* dsInfo, hid_t* fileIDPtr, hid_t* groupIDPtr, char** errorMsg)
{
#define CreateHDF5Group_Err_NoFile		-1
	
INIT_ERR
	
	hid_t					fileID			= 0;
	hid_t* 					groupIDs 		= NULL;
	char**					groupNames		= NULL;
	size_t 					nGroupNames		= 0;
	char*					tmpGroupName	= NULL;
	char* 					pch				= NULL;
	
	
	// init
	*fileIDPtr 	= 0;
	*groupIDPtr = 0;
	
	// check if a file name is given
	if (!fileName || !fileName[0])
		return CreateHDF5Group_Err_NoFile;
	
	// open an existing file
   	errChk( fileID = H5Fopen(fileName, H5F_ACC_RDWR, H5P_DEFAULT) );
   	
	// get group names
	nullChk( groupNames = malloc(MAXITERDEPTH * sizeof(char*)) );
	nullChk( tmpGroupName = GetDSInfoGroupName(dsInfo) );
  	pch = strtok (tmpGroupName,"/");
	while (pch != NULL) {
    	nullChk( groupNames[nGroupNames] = StrDup(pch) );
		nGroupNames++;
    	pch = strtok (NULL, "/");
	}
	
	// create groups if necessary
	nullChk( groupIDs = malloc((nGroupNames + 1) * sizeof(hid_t)) );
	groupIDs[0] = fileID; // initialize the first groupID with the fileID
	for (size_t i = 1; i <= nGroupNames; i++)
		groupIDs[i] = 0;
   	
	for (size_t i = 0; i < nGroupNames; i++){
		// remove any slashes from dataset name
		groupNames[i] = RemoveSlashes(groupNames[i]);
		
		// open group if it exists
		groupIDs[i+1] = H5Gopen2(groupIDs[i], groupNames[i], H5P_DEFAULT );
		
		if (groupIDs[i+1] < 0) {
			
			// create group if it didn't exist (reply is negative)
			hdf5ErrChk( groupIDs[i+1] = H5Gcreate2(groupIDs[i], groupNames[i], H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT) );
			*groupIDPtr = groupIDs[i+1]; 
			
		} else 
			*groupIDPtr = groupIDs[i+1]; 
	}
   
	// assign file ID
	*fileIDPtr = fileID;
   
	//--------------------
	// cleanup
	//--------------------
   
	// close all groupIDs except the last one and the first one which is the fileID
	for (size_t i = 1; i < nGroupNames; i++)
		H5Gclose(groupIDs[i]);
	
	OKfree(groupIDs);   
	
	// group names
	for (size_t i = 0; i < nGroupNames; i++)
			OKfree(groupNames[i]);
	OKfree(groupNames);
	
   	OKfree(tmpGroupName);
    
	return 0;
	
HDF5Error:
   
Error:
   
	// clear group names
	if (groupNames)
		for (size_t i = 0; i < nGroupNames; i++) {
			OKfree(groupNames[i]);
		}
	
	OKfree(groupNames);
	
	// clear group IDs
	if (groupIDs)
		for (size_t i = 1; i <= nGroupNames; i++)
			if (groupIDs[i] > 0) H5Gclose(groupIDs[i]);
	OKfree(groupIDs);
	
	// clear file ID (same as groupID[0])
	if (fileID > 0) H5Fclose(fileID);
	
	OKfree(tmpGroupName);
	
RETURN_ERR
}

static int CreateStringAttr (hid_t datasetID, char attr_name[], char* attr_data, char** errorMsg)
{
INIT_ERR

	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	hid_t		datatype		= 0;
	size_t  	dims			= 0; 	// Return value
	hsize_t  	dimsa[1]		= {1}; 
	
	dims 					= strlen(attr_data);
	hdf5ErrChk( dataSpaceID = H5Screate_simple (1, dimsa, NULL) );
   	datatype 				= H5Tcopy(H5T_C_S1);
   	hdf5ErrChk( H5Tset_size(datatype, dims) );
   	hdf5ErrChk( H5Tset_strpad(datatype, H5T_STR_NULLTERM) );
   	hdf5ErrChk( attributeID = H5Acreate2(datasetID, attr_name, datatype, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
   	hdf5ErrChk( H5Awrite(attributeID, datatype, attr_data) );
	
	// Close the attribute
  	hdf5ErrChk( H5Aclose (attributeID) );
	// Close the dataspace
    hdf5ErrChk( H5Sclose(dataSpaceID) ); 

HDF5Error:
	
Error:
	
RETURN_ERR
}

static int CreateLongAttr (hid_t datasetID, char attr_name[], long attr_data, char** errorMsg)
{
INIT_ERR

	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	hdf5ErrChk( dataSpaceID = H5Screate(H5S_SCALAR) );
    hdf5ErrChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_LONG, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    hdf5ErrChk( H5Awrite(attributeID, H5T_NATIVE_LONG, &attr_data) ); 
	
	// Close the attribute
  	hdf5ErrChk( H5Aclose (attributeID) );
	// Close the dataspace
    hdf5ErrChk( H5Sclose(dataSpaceID) ); 
	
HDF5Error:
	
Error:
	
RETURN_ERR
}

static int CreateULongAttr (hid_t datasetID, char attr_name[], unsigned long attr_data, char** errorMsg)
{
INIT_ERR

	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	hdf5ErrChk( dataSpaceID = H5Screate(H5S_SCALAR) );
    hdf5ErrChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_ULONG, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    hdf5ErrChk( H5Awrite(attributeID, H5T_NATIVE_ULONG, &attr_data) ); 
	
	// Close the attribute
  	hdf5ErrChk( H5Aclose (attributeID) );
	// Close the dataspace
    hdf5ErrChk( H5Sclose(dataSpaceID) ); 
	
HDF5Error:
	
Error:
	
RETURN_ERR
}

static int CreateLLongAttr (hid_t datasetID, char attr_name[], long long attr_data, char** errorMsg)
{
INIT_ERR

	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	hdf5ErrChk( dataSpaceID = H5Screate(H5S_SCALAR) );
    hdf5ErrChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_LLONG, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    hdf5ErrChk( H5Awrite(attributeID, H5T_NATIVE_LLONG, &attr_data) ); 
	
	// Close the attribute
  	hdf5ErrChk( H5Aclose (attributeID) );
	// Close the dataspace
    hdf5ErrChk( H5Sclose(dataSpaceID) ); 
	
HDF5Error:
	
Error:
	
RETURN_ERR
}

static int CreateULLongAttr (hid_t datasetID, char attr_name[], unsigned long long attr_data, char** errorMsg)
{
INIT_ERR

	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	hdf5ErrChk( dataSpaceID = H5Screate(H5S_SCALAR) );
    hdf5ErrChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_ULLONG, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    hdf5ErrChk( H5Awrite(attributeID, H5T_NATIVE_ULLONG, &attr_data) ); 
	
	// Close the attribute
  	hdf5ErrChk( H5Aclose (attributeID) );
	// Close the dataspace
    hdf5ErrChk( H5Sclose(dataSpaceID) );
	
HDF5Error:
	
Error:
	
RETURN_ERR
}

static int CreateULLongAttrArr (hid_t datasetID, char attr_name[], unsigned long long attr_data, size_t size, char** errorMsg)
{
INIT_ERR

	hid_t     				dataSpaceID			= 0;  
	hid_t					attributeID			= 0;
	hsize_t     			dims[1]				= {size};
	unsigned long long*		attr_array			= NULL;
	herr_t  				ret					= 0;			     // Return value
	
	nullChk( attr_array = malloc(size * sizeof(unsigned long long)) );
	
	hdf5ErrChk( dataSpaceID = H5Screate(H5S_SIMPLE) );
    ret	= H5Sset_extent_simple(dataSpaceID, 1, dims, NULL);
 
	attributeID = H5Aopen( datasetID, attr_name, 0);
	if (attributeID < 0) {
		//attribute didn't extist, create one
		hdf5ErrChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_ULLONG, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
		hdf5ErrChk( H5Awrite(attributeID, H5T_NATIVE_ULLONG, &attr_data) );  
	} else {
		hdf5ErrChk( H5Aread(attributeID, H5T_NATIVE_ULLONG, attr_array) );
		// delete previous attribute
		H5Aclose(attributeID);
		H5Adelete(datasetID, attr_name);
		 
		hdf5ErrChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_ULLONG, dataSpaceID, H5P_DEFAULT,H5P_DEFAULT) );  
		attr_array[size-1] = attr_data;
		hdf5ErrChk( H5Awrite(attributeID, H5T_NATIVE_ULLONG, attr_array) );
	}
   
	// Close the attribute
  	hdf5ErrChk( H5Aclose (attributeID) );
	// Close the dataspace.
    hdf5ErrChk( H5Sclose(dataSpaceID) ); 

HDF5Error:
	
Error:
	
	// cleanup
	OKfree(attr_array);
	
RETURN_ERR
}

static int ReadNumElemAttr (hid_t datasetID, hsize_t index, unsigned long long* attr_data, size_t size, char** errorMsg)
{
INIT_ERR

	hid_t     				dataSpaceID			= 0;  
	hid_t					attributeID			= 0;
	hsize_t     			dims[1]				= {size};
	unsigned long long*		attr_array			= NULL;
	herr_t  				ret					= 0;                // Return value
	
	nullChk( attr_array = malloc(size*sizeof(unsigned long long)) );
	
	hdf5ErrChk( dataSpaceID = H5Screate(H5S_SIMPLE) );
    ret = H5Sset_extent_simple(dataSpaceID, 1, dims, NULL);

	//open attribute
	hdf5ErrChk( attributeID = H5Aopen(datasetID, NUMELEMENTS_NAME, 0) ); 
	hdf5ErrChk( H5Aread(attributeID, H5T_NATIVE_ULLONG, attr_array) );
	*attr_data = attr_array[index];	 
   
	// Close the attribute
  	hdf5ErrChk( H5Aclose (attributeID) );
	// Close the dataspace
    hdf5ErrChk( H5Sclose(dataSpaceID) ); 

HDF5Error:
	
Error:
	
	//cleanup
	OKfree(attr_array);
	
RETURN_ERR
}

static int WriteNumElemAttr (hid_t datasetID, hsize_t index, unsigned long long attr_data, hsize_t size, char** errorMsg)
{
INIT_ERR

	hid_t     				dataSpaceID			= 0;  
	hid_t					attributeID			= 0;
	hsize_t     			dims[1]				= {size};
	unsigned long long*		attr_array			= NULL;
	herr_t  				ret					= 0;                // Return value
	
	nullChk( attr_array = malloc(size * sizeof(unsigned long long)) );
	
	hdf5ErrChk( dataSpaceID = H5Screate(H5S_SIMPLE) );
    ret  = H5Sset_extent_simple(dataSpaceID, 1, dims, NULL);

	//open attribute
	hdf5ErrChk( attributeID = H5Aopen(datasetID, NUMELEMENTS_NAME, NULL) );
	hdf5ErrChk( H5Aread(attributeID, H5T_NATIVE_ULLONG, attr_array) ); 
	attr_array[index] = attr_data;
	hdf5ErrChk( H5Awrite(attributeID, H5T_NATIVE_ULLONG, attr_array) );
		 
	// Close the attribute
  	hdf5ErrChk( H5Aclose (attributeID) );
	// Close the dataspace
    hdf5ErrChk( H5Sclose(dataSpaceID) ); 

HDF5Error:
	
Error:
	
	// cleanup
	OKfree(attr_array);
	
RETURN_ERR
}

static int CreateIntAttr (hid_t datasetID, char attr_name[], int attr_data, char** errorMsg)
{
INIT_ERR

	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	hdf5ErrChk( dataSpaceID = H5Screate(H5S_SCALAR) );
    hdf5ErrChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_INT, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    hdf5ErrChk( H5Awrite(attributeID, H5T_NATIVE_INT, &attr_data) );
	
	// Close the attribute
  	hdf5ErrChk( H5Aclose(attributeID) );
	// Close the dataspace
    hdf5ErrChk( H5Sclose(dataSpaceID) ); 

HDF5Error:
	
Error:
	
RETURN_ERR
}

static int CreateUIntAttr (hid_t datasetID, char attr_name[], unsigned int attr_data, char** errorMsg)
{
INIT_ERR

	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	hdf5ErrChk( dataSpaceID = H5Screate(H5S_SCALAR) );
    hdf5ErrChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_UINT, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    hdf5ErrChk( H5Awrite(attributeID, H5T_NATIVE_UINT, &attr_data) );
	
	// Close the attribute
  	hdf5ErrChk( H5Aclose(attributeID) );
	// Close the dataspace
    hdf5ErrChk( H5Sclose(dataSpaceID) ); 

HDF5Error:
	
Error:
	
RETURN_ERR
}

static int CreateShortAttr (hid_t datasetID, char attr_name[], long attr_data, char** errorMsg)
{
INIT_ERR

	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	hdf5ErrChk( dataSpaceID = H5Screate(H5S_SCALAR) );
    hdf5ErrChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_SHORT, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    hdf5ErrChk( H5Awrite(attributeID, H5T_NATIVE_SHORT, &attr_data) ); 
	
	// Close the attribute
  	hdf5ErrChk( H5Aclose(attributeID) );
	// Close the dataspace
    hdf5ErrChk( H5Sclose(dataSpaceID) ); 

HDF5Error:
	
Error:
	
RETURN_ERR
}

static int CreateUShortAttr (hid_t datasetID, char attr_name[], unsigned long attr_data, char** errorMsg)
{
INIT_ERR

	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	hdf5ErrChk( dataSpaceID = H5Screate(H5S_SCALAR) );
    hdf5ErrChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_USHORT, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    hdf5ErrChk( H5Awrite(attributeID, H5T_NATIVE_USHORT, &attr_data) ); 
	
	// Close the attribute
  	hdf5ErrChk( H5Aclose(attributeID) );
	// Close the dataspace
    hdf5ErrChk( H5Sclose(dataSpaceID) ); 

HDF5Error:
	
Error:
	
RETURN_ERR
}

static int CreateFloatAttr(hid_t datasetID, char attr_name[], float attr_data, char** errorMsg)
{
INIT_ERR

	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	hdf5ErrChk( dataSpaceID = H5Screate(H5S_SCALAR) );
    hdf5ErrChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_FLOAT, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    hdf5ErrChk( H5Awrite(attributeID, H5T_NATIVE_FLOAT, &attr_data) );
	
	// Close the attribute. */
  	hdf5ErrChk( H5Aclose(attributeID) );
	// Close the dataspace. */
    hdf5ErrChk( H5Sclose(dataSpaceID) ); 

HDF5Error:
	
Error:
	
RETURN_ERR
}

static int CreateDoubleAttr (hid_t datasetID, char attr_name[], double attr_data, char** errorMsg)
{
INIT_ERR

	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	hdf5ErrChk( dataSpaceID = H5Screate(H5S_SCALAR) );
	hdf5ErrChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_DOUBLE, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT));
    hdf5ErrChk( H5Awrite(attributeID, H5T_NATIVE_DOUBLE, &attr_data) );
	
	// Close the attribute
  	hdf5ErrChk( H5Aclose(attributeID) );
	// Close the dataspace
    hdf5ErrChk( H5Sclose(dataSpaceID) ); 
	
HDF5Error:
	
Error:
	
RETURN_ERR
}

static int CreateDoubleAttrArr(hid_t datasetID, char attr_name[], double attr_data, hsize_t size, char** errorMsg)
{
INIT_ERR
	
	hid_t     	dataSpaceID			= 0;  
	hid_t		attributeID			= 0;
	hsize_t     dims[1]				= {size};
	double*		attr_array			= NULL;
	herr_t  	ret					= 0;                // Return value
	
	nullChk( attr_array = malloc(size * sizeof(double)) );
	
	hdf5ErrChk( dataSpaceID = H5Screate(H5S_SIMPLE) );
    ret = H5Sset_extent_simple(dataSpaceID, 1, dims, NULL);

	if (size==1) {
		//create attribute
    	hdf5ErrChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_DOUBLE, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
		hdf5ErrChk( H5Awrite(attributeID, H5T_NATIVE_DOUBLE, &attr_data) );  
	} else {
		//open attribute
		hdf5ErrChk( attributeID = H5Aopen( datasetID, attr_name, NULL) ); 
		hdf5ErrChk( H5Aread(attributeID, H5T_NATIVE_DOUBLE, attr_array) );
		//test
		hdf5ErrChk( H5Aclose(attributeID) );
		H5Adelete(datasetID, attr_name);
		 
		hdf5ErrChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_DOUBLE, dataSpaceID, H5P_DEFAULT,H5P_DEFAULT) );  
		attr_array[size-1] = attr_data;
		hdf5ErrChk( H5Awrite(attributeID, H5T_NATIVE_DOUBLE, attr_array) );      
	}
   
	// Close the attribute
  	hdf5ErrChk( H5Aclose(attributeID) );
	// Close the dataspace
    hdf5ErrChk( H5Sclose(dataSpaceID) ); 
	
HDF5Error:
	
Error:
	
	//cleanup
	OKfree(attr_array);
	
RETURN_ERR
}

// Adds waveform information as dataset attributes
static int AddWaveformAttr (hid_t datasetID, Waveform_type* waveform, char** errorMsg)
{
INIT_ERR

	char* 	waveformName		= GetWaveformName(waveform);			// Name of signal represented by the waveform.    
	char* 	unitName			= GetWaveformPhysicalUnit(waveform);	// Physical SI unit such as V, A, Ohm, etc. 
	double 	timestamp			= GetWaveformDateTimestamp(waveform);	// Number of seconds since midnight, January 1, 1900 in the local time zone.
	double 	samplingRate		= GetWaveformSamplingRate(waveform);  	// Sampling rate in [Hz]. If 0, sampling rate is not given.   
	size_t 	dateTimeBuffLen		= 0;
	char*	dateTimeString		= NULL;
	
	// waveform name
	if (waveformName) {
		errChk( CreateStringAttr(datasetID, "Name", waveformName, &errorInfo.errMsg) );
	}
	
	// waveform unit name
	if (unitName) { 
		errChk( CreateStringAttr(datasetID, "Units", unitName, &errorInfo.errMsg) );
	}
	
	// waveform start timestamp
	errChk( dateTimeBuffLen = FormatDateTimeString(timestamp, DATETIME_FORMATSTRING, NULL, 0) );
	nullChk( dateTimeString	= malloc((dateTimeBuffLen + 1) * sizeof(char)) );
	errChk( FormatDateTimeString(timestamp, DATETIME_FORMATSTRING, dateTimeString, dateTimeBuffLen + 1 ) );
	errChk( CreateStringAttr(datasetID, "Timestamp", dateTimeString, &errorInfo.errMsg) );
	
	// sampling rate
	errChk( CreateDoubleAttr(datasetID, "SamplingRate", samplingRate, &errorInfo.errMsg) );
	
	// sampling interval
	if (samplingRate) {
		errChk( CreateDoubleAttr(datasetID, "dt", 1/samplingRate, &errorInfo.errMsg) );
	}
	
Error: 
	 
	// cleanup
	OKfree(waveformName);
	OKfree(unitName);
	OKfree(dateTimeString);
	 
RETURN_ERR
}

static char* RemoveSlashes (char* name)
{
	size_t	len = strlen(name);
	
	for (int i = 0; i < len; i++)
		if (name[i] == '/') name[i]='_';
	
	return name;
}

static void WaveformDataTypeToHDF5 (WaveformTypes waveformType, hid_t* typeIDPtr, hid_t* memTypeIDPtr)
{
	switch (waveformType) {
		   
		case Waveform_Char:
			
			*typeIDPtr		= H5T_STD_I8BE;
			*memTypeIDPtr	= H5T_NATIVE_SCHAR;
			break;
		   
		case Waveform_UChar:
			
			*typeIDPtr		= H5T_STD_U8BE;
			*memTypeIDPtr	= H5T_NATIVE_UCHAR;
			break;	 
		   
		case Waveform_Short:
			
			*typeIDPtr		= H5T_STD_I16BE;
			*memTypeIDPtr	= H5T_NATIVE_SHORT;
			break;
		   
		case Waveform_UShort:
			
			*typeIDPtr		= H5T_STD_U16BE;
			*memTypeIDPtr	= H5T_NATIVE_USHORT;
			break;
		   
		case Waveform_Int:
			
			*typeIDPtr		= H5T_STD_I32BE;
			*memTypeIDPtr	= H5T_NATIVE_INT;
			break;
			
		case Waveform_UInt:
			
			*typeIDPtr		= H5T_STD_U32BE;
			*memTypeIDPtr	= H5T_NATIVE_UINT;
			break;
			
		case Waveform_Int64:
			
			*typeIDPtr		= H5T_STD_I64BE;
			*memTypeIDPtr	= H5T_NATIVE_LLONG;
			break;
			
		case Waveform_UInt64:
			
			*typeIDPtr		= H5T_STD_U64BE;
		   	*memTypeIDPtr	= H5T_NATIVE_ULLONG;
			break;
			
		case Waveform_SSize:
			
			*typeIDPtr		= H5T_STD_I64BE;
			*memTypeIDPtr	= H5T_NATIVE_HSSIZE;
			break;
			
		case Waveform_Size:
			
			*typeIDPtr		= H5T_STD_U64BE;
		   	*memTypeIDPtr	= H5T_NATIVE_HSIZE;
			break;
		
		case Waveform_Float:
			
			*typeIDPtr		= H5T_IEEE_F32BE;
		   	*memTypeIDPtr	= H5T_NATIVE_FLOAT;
			break;
			
		case Waveform_Double:
			
			*typeIDPtr		= H5T_IEEE_F64BE;
			*memTypeIDPtr	= H5T_NATIVE_DOUBLE;
			break;
	}
}

static int AddWaveformAttrArr (hid_t datasetID, unsigned long long numelements, hsize_t size, char** errorMsg)
{
INIT_ERR
	 
	 errChk( CreateULLongAttrArr(datasetID, NUMELEMENTS_NAME, numelements, size, &errorInfo.errMsg) ); 		
	 
Error:
	 
RETURN_ERR
}

static int CreatePixelSizeAttr (hid_t datasetID, double* attr_data, char** errorMsg)
{
INIT_ERR

	hid_t     	dataSpaceID			= 0;  
	hid_t       attributeID			= 0;
	int 		rank				= 1;
	hsize_t 	dims[]			    = {3};
	hsize_t 	maxDims[]			= {3}; 
	
	hdf5ErrChk( dataSpaceID = H5Screate_simple(rank, dims, maxDims) );
	   
    hdf5ErrChk( attributeID = H5Acreate2(datasetID, PIXELSIZE_NAME, H5T_NATIVE_DOUBLE, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    hdf5ErrChk( H5Awrite(attributeID, H5T_NATIVE_DOUBLE, attr_data) );
	
	// Close the attribute
  	hdf5ErrChk( H5Aclose (attributeID) );
	// Close the dataspace
    hdf5ErrChk( H5Sclose(dataSpaceID) ); 
	
HDF5Error:
	
Error:
	
RETURN_ERR
}

// Adds image pixel size info attribute to the image data set.
static int AddImagePixSizeAttributes (hid_t datasetID, Image_type* image, char** errorMsg)
{
INIT_ERR

	 double	pixSize				= GetImagePixSize(image);		// Image pixel size in [um].
	 double pixsize[3]			= {pixSize, pixSize, pixSize}; // Z, X, Y
	 
	 errChk( CreatePixelSizeAttr(datasetID,pixsize, &errorInfo.errMsg) );          
	 
Error:

RETURN_ERR
}	

// Adds all image information attributes.
static int AddImageAttributes (hid_t datasetID, Image_type* image, hsize_t size, char** errorMsg)
{
INIT_ERR

	 double	imgTopLeftXCoord	= 0;	// Image top-left corner X-Axis coordinates in [um].
	 double	imgTopLeftYCoord	= 0;	// Image top-left corner Y-Axis coordinates in [um].
	 double	imgZCoord			= 0;	// Image z-axis (height) location in [um].
	 
	 GetImageCoordinates(image, &imgTopLeftXCoord, &imgTopLeftYCoord, &imgZCoord);
		 
	 errChk(CreateDoubleAttrArr(datasetID,"TopLeftXCoord", imgTopLeftXCoord, size, &errorInfo.errMsg));    	
	 errChk(CreateDoubleAttrArr(datasetID,"TopLeftYCoord", imgTopLeftYCoord, size, &errorInfo.errMsg));  
	 errChk(CreateDoubleAttrArr(datasetID,"ZCoord", imgZCoord, size, &errorInfo.errMsg)); 
          

Error:

RETURN_ERR
}


