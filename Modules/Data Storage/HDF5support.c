//==============================================================================
//
// Title:		HDF5test
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


#ifndef statChk
#define statChk(fCall) if (status = (fCall), status < 0) \
{goto HDF5Error;} else
#endif
	
	

//==============================================================================
// Constants
#define MAXITERDEPTH 100
#define MAXCHAR		 260
	
#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}   

//==============================================================================
// Types
	
typedef struct IteratorData		IteratorData_type; 
	
struct IteratorData {
	char*					name;					// Iterator name.  
	size_t					currentIterIdx; 		// 0-based iteration index used to iterate over iterObjects
//	size_t					totalIter;				// Total number of iterations
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables

//==============================================================================
// Global functions


int CreateRootGroup(hid_t file_id,char *group_name)
{
	herr_t      status=0;   
	hid_t		group_id;
	
	group_id = H5Gcreate(file_id, group_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  	statChk(H5Gclose (group_id));
	
	return status;
HDF5Error:
   return status;

}


int CreateRelativeGroup(hid_t parentgroup_id,char *group_name)
{
	herr_t      status=0;   
	hid_t		group_id;
	
	group_id = H5Gcreate2(parentgroup_id, group_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  	statChk(H5Gclose (group_id));
	
	return status;
HDF5Error:
   return status;

}



int CreateStringAttr(hid_t dataset_id,char *attr_name, char* attr_data )
{
	herr_t      status=0;
	hid_t     	dataspace_id;  
	hid_t       attribute_id;
	hid_t		datatype;
	hsize_t  	dims;			/* Return value */
	hsize_t  	dimsa[1]={1}; 
	
	dims=strlen(attr_data);
	dataspace_id  = H5Screate_simple (1, dimsa, NULL);
   	datatype = H5Tcopy(H5T_C_S1);
   	statChk(H5Tset_size(datatype, dims));
   	statChk(H5Tset_strpad(datatype,H5T_STR_NULLTERM));
   	attribute_id = H5Acreate2(dataset_id, attr_name, datatype, dataspace_id, H5P_DEFAULT, H5P_DEFAULT);
   	statChk(H5Awrite(attribute_id, datatype, attr_data));
	
	/* Close the attribute. */
  	statChk(H5Aclose (attribute_id));
	/* Close the dataspace. */
    statChk(H5Sclose(dataspace_id)); 
	
	return status;
   
HDF5Error:
   
   return status;
}


int CreateLongAttr(hid_t dataset_id,char *attr_name, long attr_data )
{
	herr_t      status=0;
	hid_t     	dataspace_id;  
	hid_t       attribute_id;
	
	dataspace_id  = H5Screate(H5S_SCALAR);
    attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_LONG, dataspace_id,H5P_DEFAULT,H5P_DEFAULT);
    statChk(H5Awrite(attribute_id, H5T_NATIVE_LONG, &attr_data)); 
	/* Close the attribute. */
  	statChk(H5Aclose (attribute_id));
	/* Close the dataspace. */
    statChk(H5Sclose(dataspace_id)); 
	
	return status;
   
HDF5Error:
   
   return status;
}

int CreateULongAttr(hid_t dataset_id,char *attr_name, unsigned long attr_data )
{
	herr_t      status=0;
	hid_t     	dataspace_id;  
	hid_t       attribute_id;
	
	dataspace_id  = H5Screate(H5S_SCALAR);
    attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_ULONG, dataspace_id,H5P_DEFAULT,H5P_DEFAULT);
    statChk(H5Awrite(attribute_id, H5T_NATIVE_ULONG, &attr_data)); 
	/* Close the attribute. */
  	statChk(H5Aclose (attribute_id));
	/* Close the dataspace. */
    statChk(H5Sclose(dataspace_id)); 
	
	return status;
   
HDF5Error:
   
   return status;
}

int CreateLLongAttr(hid_t dataset_id,char *attr_name, long long attr_data )
{
	herr_t      status=0;
	hid_t     	dataspace_id;  
	hid_t       attribute_id;
	
	dataspace_id  = H5Screate(H5S_SCALAR);
    attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_LLONG, dataspace_id,H5P_DEFAULT,H5P_DEFAULT);
    statChk(H5Awrite(attribute_id, H5T_NATIVE_LLONG, &attr_data)); 
	/* Close the attribute. */
  	statChk(H5Aclose (attribute_id));
	/* Close the dataspace. */
    statChk(H5Sclose(dataspace_id)); 
	
	return status;
   
HDF5Error:
   
   return status;
}

int CreateULLongAttr(hid_t dataset_id,char *attr_name, unsigned long long attr_data )
{
	herr_t      status=0;
	hid_t     	dataspace_id;  
	hid_t       attribute_id;
	
	dataspace_id  = H5Screate(H5S_SCALAR);
    attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_ULLONG, dataspace_id,H5P_DEFAULT,H5P_DEFAULT);
    statChk(H5Awrite(attribute_id, H5T_NATIVE_ULLONG, &attr_data)); 
	/* Close the attribute. */
  	statChk(H5Aclose (attribute_id));
	/* Close the dataspace. */
    statChk(H5Sclose(dataspace_id)); 
	
	return status;
   
HDF5Error:
   
   return status;
}


int CreateIntAttr(hid_t dataset_id,char *attr_name, int attr_data )
{
	herr_t      status=0;
	hid_t     	dataspace_id;  
	hid_t       attribute_id;
	
	dataspace_id  = H5Screate(H5S_SCALAR);
    attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_INT, dataspace_id,H5P_DEFAULT,H5P_DEFAULT);
    statChk(H5Awrite(attribute_id, H5T_NATIVE_INT, &attr_data)); 
	/* Close the attribute. */
  	statChk(H5Aclose (attribute_id));
	/* Close the dataspace. */
    statChk(H5Sclose(dataspace_id)); 
	
	return status;
   
HDF5Error:
   
   return status;
}


int CreateUIntAttr(hid_t dataset_id,char *attr_name, unsigned int attr_data )
{
	herr_t      status=0;
	hid_t     	dataspace_id;  
	hid_t       attribute_id;
	
	dataspace_id  = H5Screate(H5S_SCALAR);
    attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_UINT, dataspace_id,H5P_DEFAULT,H5P_DEFAULT);
    statChk(H5Awrite(attribute_id, H5T_NATIVE_UINT, &attr_data)); 
	/* Close the attribute. */
  	statChk(H5Aclose (attribute_id));
	/* Close the dataspace. */
    statChk(H5Sclose(dataspace_id)); 
	
	return status;
   
HDF5Error:
   
   return status;
}

int CreateShortAttr(hid_t dataset_id,char *attr_name, long attr_data )
{
	herr_t      status=0;
	hid_t     	dataspace_id;  
	hid_t       attribute_id;
	
	dataspace_id  = H5Screate(H5S_SCALAR);
    attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_SHORT, dataspace_id,H5P_DEFAULT,H5P_DEFAULT);
    statChk(H5Awrite(attribute_id, H5T_NATIVE_SHORT, &attr_data)); 
	/* Close the attribute. */
  	statChk(H5Aclose (attribute_id));
	/* Close the dataspace. */
    statChk(H5Sclose(dataspace_id)); 
	
	return status;
   
HDF5Error:
   
   return status;
}

int CreateUShortAttr(hid_t dataset_id,char *attr_name, unsigned long attr_data )
{
	herr_t      status=0;
	hid_t     	dataspace_id;  
	hid_t       attribute_id;
	
	dataspace_id  = H5Screate(H5S_SCALAR);
    attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_USHORT, dataspace_id,H5P_DEFAULT,H5P_DEFAULT);
    statChk(H5Awrite(attribute_id, H5T_NATIVE_USHORT, &attr_data)); 
	/* Close the attribute. */
  	statChk(H5Aclose (attribute_id));
	/* Close the dataspace. */
    statChk(H5Sclose(dataspace_id)); 
	
	return status;
   
HDF5Error:
   
   return status;
}


int CreateFloatAttr(hid_t dataset_id,char *attr_name, float attr_data )
{
	herr_t      status=0;
	hid_t     	dataspace_id;  
	hid_t       attribute_id;
	
	dataspace_id  = H5Screate(H5S_SCALAR);
    attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_FLOAT, dataspace_id,H5P_DEFAULT,H5P_DEFAULT);
    statChk(H5Awrite(attribute_id, H5T_NATIVE_FLOAT, &attr_data)); 
	/* Close the attribute. */
  	statChk(H5Aclose (attribute_id));
	/* Close the dataspace. */
    statChk(H5Sclose(dataspace_id)); 
	
	return status;
   
HDF5Error:
   
   return status;
}

int CreateDoubleAttr(hid_t dataset_id,char *attr_name, double attr_data)
{
	herr_t      status				= 0;
	hid_t     	dataspace_id;  
	hid_t       attribute_id;
	
	dataspace_id  	= H5Screate(H5S_SCALAR);
    attribute_id 	= H5Acreate2(dataset_id, attr_name, H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT,H5P_DEFAULT);
    statChk(H5Awrite(attribute_id, H5T_NATIVE_DOUBLE, &attr_data)); 
	/* Close the attribute. */
  	statChk(H5Aclose (attribute_id));
	/* Close the dataspace. */
    statChk(H5Sclose(dataspace_id)); 
	
	return status;
   
HDF5Error:
   
   return status;
}
	



herr_t CreateDataSet(hid_t file_id,char* dataset_name)
{
    int       	dataset_id;
	int			dataspace_id;  /* identifiers */
    hsize_t   	dims[2];
    herr_t      status=0;
	hid_t 		type_id=0;
	hid_t       attr_id=0;
	int attr_data;
	float attr_float;
	char* str_data="Teststring";
   
   
 /* Create the data space for the dataset. */
  	dims[0] = 4; 
  	dims[1] = 6; 
   dataspace_id = H5Screate_simple(2, dims, NULL);

   /* Create the dataset. */
   type_id=H5T_STD_I32BE;
   
   dataset_id = H5Dcreate2(file_id, dataset_name, type_id , dataspace_id,   	   
                          H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
   if (dataset_id<0) return -1;
    //test
   attr_data=21 ;
   attr_float=1.12354; 
   
   statChk(CreateIntAttr(dataset_id,"Int_attr", attr_data)); 
   statChk(CreateUShortAttr(dataset_id,"Ushort_attr", attr_data)); 
   statChk(CreateFloatAttr(dataset_id,"Float_attr", attr_float)); 
   statChk(CreateStringAttr(dataset_id,"Test_attr", str_data ));
	
   //statChk(writeattribute(dataset_id,"Units",H5T_STD_I32BE,&attr_data)); 
   /* End access to the dataset and release resources used by it. */
   statChk(H5Dclose(dataset_id));
   
   /* Terminate access to the data space. */ 
   statChk(H5Sclose(dataspace_id));
   return status;
   
HDF5Error:
   
   return status;
}


int CreateHDF5file(char *filename,char* dataset_name) 
   {
					 
      hid_t       file_id;   /* file identifier */
      herr_t      status=0;
	  
      

      /* Create a new file using default properties. */
      file_id = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
	  if(file_id<0) return -1 ;
	  
	  //CreateRootGroup(file_id,"Data");
	  
	  //statChk(CreateDataSet(file_id,dataset_name));
	  
      /* Terminate access to the file. */
      statChk(H5Fclose(file_id)); 
	  
	  return status;
HDF5Error:
	  return status;
	 
   }




herr_t ReadHDF5file(char *filename,char* dataset_name) 
{
   hid_t       	file_id;
   hid_t		dataset_id;  /* identifiers */
   herr_t     	status=0;
   int         	i;
   int			j;
   int			dset_data[4][6];

   /* Initialize the dataset. */
   for (i = 0; i < 4; i++)
      for (j = 0; j < 6; j++)
         dset_data[i][j] = 0;

   /* Open an existing file. */
   file_id = H5Fopen(filename, H5F_ACC_RDWR, H5P_DEFAULT);
   if (file_id<0) return -1; 
   /* Open an existing dataset. */
   dataset_id = H5Dopen2(file_id, dataset_name, H5P_DEFAULT);
   if (dataset_id<0) return -1;; 
   /* read the dataset. */
   statChk(H5Dread(dataset_id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, 
                    dset_data));

   /* Close the dataset. */
   statChk(H5Dclose(dataset_id));

   /* Close the file. */
   statChk(H5Fclose(file_id));

   return status;
   
HDF5Error:
   
   
   return status;
}



ListType ParseIterator(Iterator_type*		currentiter)
{
	
	Iterator_type* 	  	childiter;
	Iterator_type* 	  	parentiter;
	ListType*		  	parsediter;
	IteratorData_type* 	newelement;
	
	if (currentiter==NULL) return NULL;
	parsediter = ListCreate(sizeof(IteratorData_type*));

	parentiter = GetIteratorParent(currentiter);
	if (parentiter==NULL) return 0;  //no parent
	newelement=malloc(sizeof(IteratorData_type));
	newelement->name=GetCurrentIterationName(parentiter); 				  
	newelement->currentIterIdx=GetCurrentIterationIndex(parentiter);
	ListInsertItem(parsediter,&newelement ,FRONT_OF_LIST);
	while (parentiter!=NULL) {  
		childiter=parentiter;
		parentiter = GetIteratorParent(childiter);
		if (parentiter==NULL) {
			return parsediter;  //no parent, return current name
		}
		newelement=malloc(sizeof(IteratorData_type));
		newelement->name=GetCurrentIterationName(parentiter); 				  
		newelement->currentIterIdx=GetCurrentIterationIndex(parentiter);
		ListInsertItem(parsediter,&newelement ,FRONT_OF_LIST);
	}
	return parsediter;	
}

void discardParsed(ListType iterlist)
{
	 IteratorData_type** element=NULL;
	 int numitems=ListNumItems(iterlist); 
	 int i;
	 
	 for (i=0;i<numitems;i++){
		 element=ListGetPtrToItem(iterlist, i); 
		 OKfree((*element)->name);
		 OKfree(*element);
	 }
	 ListDispose(iterlist);
}

char* RemoveSlashes(char* name)
{
	 //char* 		result=NULL;
	 int		i=0;
	 size_t   	len=strlen(name);
	 
	 for (i=0;i<len;i++){
		if (name[i]=='/') name[i]='_';
	 }
	 return name;
}

//converts all waveform information into attributes, 
//and adds them to the dataset
int AddWaveformAttributes(hid_t dataset_id,Waveform_type* waveform)
{
	 int error						= 0;
	 char* name						= GetWaveformName(waveform);			// Name of signal represented by the waveform.    
	 char* unitname					= GetWaveformPhysicalUnit(waveform);	// Physical SI unit such as V, A, Ohm, etc. 
	 double datetimestamp			= GetWaveformDateTimestamp(waveform);	// Number of seconds since midnight, January 1, 1900 in the local time zone.
	 double samplingrate			= GetWaveformSamplingRate(waveform);  	// Sampling rate in [Hz]. If 0, sampling rate is not given.   
//	 unsigned long long numsamples	= GetWaveformNumSamples(waveform);  	// Number of samples in the waveform.   
	 
	 
	 if (name) errChk(CreateStringAttr(dataset_id,"Name", name));   	     
	 if (unitname) errChk(CreateStringAttr(dataset_id,"Units", unitname));   		
	 if (datetimestamp) errChk(CreateDoubleAttr(dataset_id,"DateTimeStamp", datetimestamp));      
	 errChk(CreateDoubleAttr(dataset_id,"SamplingRate", samplingrate));    	
//	 errChk(CreateULLongAttr(dataset_id,"NumSamples",numsamples )); 		
	 return error;
Error:
	 return error;
}


										   
int WriteHDF5file(char *filename,Iterator_type*		currentiter,char* dataset_name, unsigned short* dset_data,int nElem, DLDataTypes datatype,Waveform_type* waveform) {

   hid_t       			file_id;
   hid_t				dataspace_id;    
   hid_t				dataset_id;  
   herr_t      			status=0;
   hsize_t      		dims[1]={nElem};
   hsize_t      		maxdims[1] = {H5S_UNLIMITED};
   hid_t				group_id[MAXITERDEPTH];
   hid_t				newgroup_id;   
   ListType				iterset;
   size_t				i;
   size_t				numitems;
  // char* 				group_name;
   char* 				newgroup_name; 
   const char 			setOfDelimiters[]={'#','/'};
   int 					iterdepth;
   IteratorData_type** 	iterdata;
   hid_t       			cparms; 
   char**				groupname;
   hid_t 				mem_type_id =0;
   hid_t 				type_id		=0;
   hid_t        		filespace;
   hid_t				memspace;
   hsize_t				dimsr[2];
   hsize_t      		size[2];
   hsize_t      		offset[2];
	
  	
  

   /* Open an existing file. */
   file_id = H5Fopen(filename, H5F_ACC_RDWR, H5P_DEFAULT);
   if (file_id<0) {
	   return -1; 
   }
   /* Open an existing or new dataset. */
   dataspace_id = H5Screate_simple(1, dims, maxdims);
   
   //parse iterator,
   iterset=ParseIterator(currentiter);
   iterdepth=ListNumItems(iterset);
   groupname=malloc(iterdepth*sizeof(char*));
   //create groups if necessary
   for(i=0;i<MAXITERDEPTH;i++){
	    group_id[i]=0;
   }
   group_id[0]=file_id;
   for(i=1;i<=iterdepth;i++){
	   
	   iterdata = ListGetPtrToItem(iterset, i);   
	   groupname[i-1]=malloc(MAXCHAR*sizeof(char));
	   Fmt(groupname[i-1],"%s<%s#%i",(*iterdata)->name,(*iterdata)->currentIterIdx);
	   //datasetname shouldn't have slashes in it
   	   groupname[i-1]=RemoveSlashes(groupname[i-1]);
	   //open group if it exists
	   group_id[i]=H5Gopen2(group_id[i-1], groupname[i-1], H5P_DEFAULT );
	   if (group_id[i]<0) {
		   //create group
	   		group_id[i] = H5Gcreate2(group_id[i-1], groupname[i-1], H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
	   		if (group_id[i]<0) {
		   		return -1; 
	   		}
	   }
   	   
   }
   discardParsed(iterset);
   
   /* Modify dataset creation properties, i.e. enable chunking  */
   cparms = H5Pcreate (H5P_DATASET_CREATE);
   statChk(H5Pset_chunk ( cparms, 1, dims));

    /* Create a new dataset within the file using cparms
       creation properties.  */
  //  dataset = H5Dcreate (file, DATASETNAME, H5T_NATIVE_INT, dataspace,
  //                       cparms);
  
   //datatype switch
   switch (datatype){
		case DL_Char:
		   	type_id=H5T_STD_I8BE;
		   	mem_type_id=H5T_NATIVE_SCHAR;
		   break;
		case DL_UChar:
		   	type_id=H5T_STD_U8BE;
		   	mem_type_id=H5T_NATIVE_UCHAR;
		   break;	 
	   	case DL_Short:
		   	type_id=H5T_STD_I16BE;
		   	mem_type_id=H5T_NATIVE_SHORT;
		   break;
		case DL_UShort:
		   	type_id=H5T_STD_U16BE;
		   	mem_type_id=H5T_NATIVE_USHORT;
		   break;
		case DL_Int:
		    type_id=H5T_STD_I32BE;
		   	mem_type_id=H5T_NATIVE_INT;
		   break;
		case DL_UInt:
		   	type_id=H5T_STD_U32BE;
		   	mem_type_id=H5T_NATIVE_UINT;
		   break;
		case DL_Long:
		    type_id=H5T_STD_I64BE;
		   	mem_type_id=H5T_NATIVE_LONG;
		   break;
		case DL_ULong:
		   	type_id=H5T_STD_U64BE;
		   	mem_type_id=H5T_NATIVE_ULONG;
		   break;
		case DL_Float:
		    type_id=H5T_IEEE_F32BE;
		   	mem_type_id=H5T_NATIVE_FLOAT;
		   break;
		case DL_Double:
		   	type_id=H5T_IEEE_F64BE;
		   	mem_type_id=H5T_NATIVE_DOUBLE;
		   break;
   }
  
   //datasetname shouldn't have slashes in it
   dataset_name=RemoveSlashes(dataset_name);
   //open the dataset if it exists
   dataset_id=H5Dopen2(group_id[iterdepth], dataset_name, H5P_DEFAULT );
   if (dataset_id<0) {
   		dataset_id = H5Dcreate2(group_id[iterdepth], dataset_name,type_id, dataspace_id,H5P_DEFAULT, cparms,H5P_DEFAULT);
   		if (dataset_id<0) {
	   		return -1;
   		}
		/* Write the dataset. */
   		statChk(H5Dwrite(dataset_id, mem_type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, dset_data));
   		//add attributes to dataset
   		statChk(AddWaveformAttributes(dataset_id,waveform));
   }
   else {
	   //dataset existed, have to add the data to the current data set
	   //get the size of the dataset
	   	filespace = H5Dget_space (dataset_id);
    	//rank = H5Sget_simple_extent_ndims (filespace);
    	statChk(H5Sget_simple_extent_dims (filespace, dimsr, NULL));
    	size[0] = dimsr[0]+ dims[0];
    	size[1] = dimsr[1];
    	statChk(H5Dset_extent (dataset_id, size));

    	/* Select a hyperslab in extended portion of dataset  */
    	filespace = H5Dget_space (dataset_id);
    	offset[0] = dimsr[0];
    	offset[1] = 0;
    	statChk(H5Sselect_hyperslab (filespace, H5S_SELECT_SET, offset, NULL,dims, NULL));  

    	/* Define memory space */
   		memspace = H5Screate_simple (1, dims, NULL); 

    	/* Write the data to the extended portion of dataset  */
    	statChk(H5Dwrite (dataset_id, mem_type_id, memspace, filespace,H5P_DEFAULT, dset_data));
   }
   
  
   /* Close the dataset. */
   statChk(H5Dclose(dataset_id));
   
   /* Close all group ids */
   for(i=1;i<=iterdepth;i++){
   		statChk(H5Gclose (group_id[i]));
		free(groupname[i-1]);
   }
   
   free(groupname);
   
    /* Close the dataspace. */
   statChk(H5Sclose(dataspace_id));
   /* Close the file. */
   statChk(H5Fclose(file_id));
   
   return status;
   
HDF5Error:
   return status;
}



