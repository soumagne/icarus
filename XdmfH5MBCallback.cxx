/*=========================================================================

  Project                 : Icarus
  Module                  : XdmfH5MBCallback.cxx

  Authors:
     John Biddiscombe     Jerome Soumagne
     biddisco@cscs.ch     soumagne@cscs.ch

  Copyright (C) CSCS - Swiss National Supercomputing Centre.
  You may use modify and and distribute this code freely providing
  1) This copyright notice appears on all copies of source code
  2) An acknowledgment appears with any substantial usage of the code
  3) If this code is contributed to any other open source project, it
  must not be reformatted such that the indentation, bracketing or
  overall style is modified significantly.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  This work has received funding from the European Community's Seventh
  Framework Programme (FP7/2007-2013) under grant agreement 225967 “NextMuSE”

=========================================================================*/
#include "XdmfHeavyData.h"
#include "XdmfH5MBCallback.h"
#include "H5MButil.h"
#include <map>
#include <utility>
#include "XdmfArray.h"
#include "XdmfHDF.h"
#include "hdf5_hl.h"
#include "vtkDsmManager.h"
#include "H5FDdsm.h"

typedef std::pair<std::string, XdmfArray *> HeavyType;
typedef std::map< std::string, XdmfArray *> HeavyDataMap;

class ArrayMap {
  public: 
    ArrayMap() {}
    HeavyDataMap datamap;
};

//----------------------------------------------------------------------------
H5MBCallback::H5MBCallback(MPI_Comm comm) 
{
  this->Communicator = comm;
  if (!this->Communicator) {
    this->Size = 1;
    this->Rank = 0;
  }
  else {
    MPI_Comm_size( this->Communicator, &this->Size );
    MPI_Comm_rank( this->Communicator, &this->Rank );
  }
  this->tree        = NULL;
  this->dataArrays  = NULL;
  this->AccessPlist = -1;
}
//----------------------------------------------------------------------------
void  H5MBCallback::SetDsmManager(vtkDsmManager *dsmmanager)
{
  this->DsmManager = dsmmanager;
}
//----------------------------------------------------------------------------
XdmfInt32 H5MBCallback::DoOpen(XdmfHeavyData *ds, XdmfConstString name, XdmfConstString access)
{
  //
  XdmfConstString lastcolon;
  XdmfConstString firstcolon;
  std::string Domain, FileName, Path;

  if ( name == NULL ) {
    Error("No Filename");
    return XDMF_FAIL;
  }

  lastcolon = strrchr( name, ':' );
  firstcolon = strchr( name, ':' );

  if ( ( lastcolon == NULL ) && ( firstcolon == NULL ) ){
    Path = name;
    ds->SetPath(Path.c_str());
//    Debug("No Colons - simple HDF Filename");
  } 
  else if ( lastcolon != firstcolon ) {
    Domain   = std::string(name, firstcolon);;
    FileName = std::string(firstcolon+1, lastcolon); 
    FileName = "DSM.h5";
    Path     = std::string(lastcolon+1);
    ds->SetPath(Path.c_str());
    ds->SetFileName(FileName.c_str());
    ds->SetDomain(Domain.c_str());
//    Debug("Two Colons -  Full HDF Filename Domain : " <<
//      Domain.c_str() << " File " << FileName.c_str());
  } 
  else {
    Domain   = "FILE";
    FileName = std::string(name, firstcolon); 
    Path     = std::string(firstcolon+1);
    ds->SetPath(Path.c_str());
    ds->SetFileName(FileName.c_str());
    ds->SetDomain(Domain.c_str());
//    Debug("One colon - file or domain? : " <<
//      Domain.c_str() << " File " << FileName.c_str());
  }

  if (this->tree==NULL) {
    std::string filename = FileName;
    herr_t status = -1;
    this->AccessPlist = H5Pcreate(H5P_FILE_ACCESS);
    if (this->AccessPlist>=0) {
      Debug("property list created with Id " << this->AccessPlist);
    }
    else {
      Error("file access property list creation failed");
    }
    // If using DSM - Set up file access property list with DSM handle
    if (this->DsmManager && ds->GetDomain()==std::string("DSM")) {
      H5FD_dsm_set_manager(this->DsmManager->GetDsmManager());
      status = H5Pset_fapl_dsm(this->AccessPlist,
          this->DsmManager->GetDsmManager()->GetMpiComm(), NULL, 0);
    }
    else {
      filename = std::string(ds->GetWorkingDirectory()) + "/" + FileName;
      // if we are running in parallel, setup with MPI_IO
      if (this->Size>1) { 
        status = H5Pset_fapl_mpio(this->AccessPlist, this->Communicator, MPI_INFO_NULL);
      }
    }
    if (status<0) {
      Error("setting file access property list information failed");
    }
    this->tree = H5MB_init(filename.c_str());
    this->dataArrays = new ArrayMap();
  }
  XdmfHDF *hdf = dynamic_cast<XdmfHDF*>(ds);
  hssize_t   dims[5] = { 0,0,0,0,0};
  hssize_t  start[5] = { 0,0,0,0,0};
  hssize_t stride[5] = { 0,0,0,0,0};
  hssize_t  count[5] = { 0,0,0,0,0};
  hdf->GetShape(dims);
  if (hdf->GetSelectionType()==XDMF_HYPERSLAB) {
    hdf->GetHyperSlab(&start[0],&stride[0],&count[0]);
  }
  //
  this->dataArrays->datamap.insert( HeavyType(Path, static_cast<XdmfArray*>(NULL)));
//  Debug("DoOpen with map size " << this->dataArrays->datamap.size());
  //
  char datatype[256];
  size_t len=256;
  herr_t err = H5LTdtype_to_text(hdf->GetDataType(), datatype, H5LT_DDL, &len);
  if (err>=0) {
    H5MB_add(this->tree, Path.c_str(), datatype, hdf->GetRank(), dims, start, stride, count);
  }
  else {
    H5MB_add(this->tree, Path.c_str(), "TYPE_ERROR", hdf->GetRank(), dims, start, stride, count);
  }

  return XDMF_SUCCESS;
}
//----------------------------------------------------------------------------
XdmfInt32 H5MBCallback::DoClose(XdmfHeavyData *ds)
{
  return XDMF_SUCCESS;
}
//----------------------------------------------------------------------------
XdmfInt32 H5MBCallback::DoWrite(XdmfHeavyData* ds, XdmfArray* array)
{
  XdmfArray *newArray = new XdmfArray();
  newArray->SetAllowAllocate(0);
  newArray->CopyShape(array);
  newArray->CopyType(array);
  newArray->CopySelection(array);
  newArray->SetDataPointer(array->GetDataPointer());
  array->DropDataPointer();
//  Error("array->DropDataPointer() has been skipped, please check memory usage here");
  
  //
  HeavyDataMap::iterator it = this->dataArrays->datamap.find(ds->GetPath());
  if (it!=this->dataArrays->datamap.end()) {
    it->second = newArray;
    Debug("Set array for " << ds->GetPath());
  }
  else {
    Error("A serious error occurred matching datasets during write ");
  }
  //
//  Debug("DoWrite with map size " << this->dataArrays->datamap.size());
  return XDMF_SUCCESS;
}
//----------------------------------------------------------------------------
namespace H5MB_utility {
  extern std::string TextVector(const char *title, int rank, hsize_t *var);
};
//----------------------------------------------------------------------------
void H5MBCallback::Synchronize()
{
  //
  if (!this->tree) return;
  //
  H5MB_collect(this->tree, this->Communicator);
  // H5MB_print(this->tree);
  H5MB_create(this->tree, this->Communicator, this->AccessPlist);
  //
  HeavyDataMap::iterator it = this->dataArrays->datamap.begin();
  for (; it != this->dataArrays->datamap.end(); ++it) {
    const char *datasetpath = it->first.c_str();
    Debug("Fetching array for " << datasetpath);
    hid_t Dataset = H5MB_get(this->tree, datasetpath);
    herr_t status;

    XdmfArray *data_array = it->second;
    if (data_array->GetSelectionType()==XDMF_HYPERSLAB) {
      hsize_t   dims[5], start[5], stride[5], count[5];
      int rank = data_array->GetRank();
      data_array->GetShape((XdmfInt64*)(&dims[0]));
      data_array->GetHyperSlab((XdmfInt64*)(start), (XdmfInt64*)(stride), (XdmfInt64*)(count));
      hid_t diskshape = H5Screate_simple(data_array->GetRank(), dims, NULL);

      Debug("Hyperslab setup  using " 
        << H5MB_utility::TextVector("Dimensions", rank, dims).c_str()
        << H5MB_utility::TextVector("Start",      rank, start).c_str()
        << H5MB_utility::TextVector("Stride",     rank, stride).c_str()
        << H5MB_utility::TextVector("Count",      rank, count).c_str()
      );

      status = H5Sselect_hyperslab(diskshape, H5S_SELECT_SET, start, stride, count, NULL);
      if ( status < 0 ) {
        Error("Hyperslab selection " << datasetpath);
      }
      hid_t memshape  = H5Screate_simple(data_array->GetRank(), count, NULL);
      if ( status < 0 ) {
        Error("creating hyperslab " << datasetpath);
      }
      status = H5Dwrite(Dataset,  
          data_array->GetDataType(),
          memshape,
          diskshape,
          H5P_DEFAULT,
          data_array->GetDataPointer() );
      H5Sclose(memshape);
      H5Sclose(diskshape);
    }
    else {
      status = H5Dwrite( Dataset,
          data_array->GetDataType(),
          H5S_ALL,
          H5S_ALL,
          H5P_DEFAULT,
          data_array->GetDataPointer() );
    }
    if ( status < 0 ) {
      Error("writing " << datasetpath);
    }
    else {
      Debug("written " << datasetpath);
    }
    delete data_array;
  }
  // remove all 
  this->dataArrays->datamap.clear();

  // finished this timestep, close everything, but reuse tree/file
  herr_t status = H5MB_close(this->tree, true);
  Debug("tree closed returning " << (status<0 ? "Fail" : "OK"));
  if ( status < 0 ) {
    Error("closing tree ");
  }
}
//----------------------------------------------------------------------------
void H5MBCallback::CloseTree()
{
  if (!this->tree) return;
  //
  // finished this timestep, close and delete everything
  herr_t status = H5Pclose(this->AccessPlist);
  if ( status < 0 ) {
    Error("closing property list");
  }
  status = H5MB_close(this->tree, false);
  Debug("tree closed returning " << (status<0 ? "Fail" : "OK"));
  if ( status < 0 ) {
    Error("closing tree ");
  }
  this->tree = NULL;
  delete this->dataArrays;
  this->dataArrays = NULL;
}
