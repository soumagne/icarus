#include "XdmfHeavyData.h"
#include "xdmfH5MBCallback.h"
#include "H5MButil.h"
#include <map>
#include <utility>
#include "XdmfArray.h"
#include "XdmfHDF.h"
#include "hdf5_hl.h"
#include "H5FDdsm.h"
#include "vtkDSMManager.h"

typedef std::pair<std::string, XdmfArray *> HeavyType;
typedef std::pair<XdmfHeavyData*, HeavyType> MapType;
typedef std::map< XdmfHeavyData*, HeavyType> HeavyDataMap;

class ArrayMap {
  public: 
    ArrayMap() {}
    HeavyDataMap datamap;
};

//----------------------------------------------------------------------------
H5MBCallback::H5MBCallback(MPI_Comm comm) 
{
  this->Communicator = comm;
  MPI_Comm_size( this->Communicator, &this->Size );
  MPI_Comm_rank( this->Communicator, &this->Rank );
  this->tree        = NULL;
  this->dataArrays  = NULL;
  this->AccessPlist = -1;
}
//----------------------------------------------------------------------------
void  H5MBCallback::SetDSMManager(vtkDSMManager *dsmmanager)
{
  this->DSMManager = dsmmanager;
}
//----------------------------------------------------------------------------
XdmfInt32 H5MBCallback::DoOpen(XdmfHeavyData *ds, XdmfConstString name, XdmfConstString access)
{
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
    Debug("No Colons - simple HDF Filename");
  } 
  else if ( lastcolon != firstcolon ) {
    Domain   = std::string(name, firstcolon);;
    FileName = std::string(firstcolon+1, lastcolon); 
    FileName = "DSM.h5";
    Path     = std::string(lastcolon+1);
    ds->SetPath(Path.c_str());
    ds->SetFileName(FileName.c_str());
    ds->SetDomain(Domain.c_str());
    Debug("Two Colons -  Full HDF Filename Domain : " <<
      Domain.c_str() << " File " << FileName.c_str());
  } 
  else {
    Domain   = "FILE";
    FileName = std::string(name, firstcolon); 
    Path     = std::string(firstcolon+1);
    ds->SetPath(Path.c_str());
    ds->SetFileName(FileName.c_str());
    ds->SetDomain(Domain.c_str());
    Debug("One colon - file or domain? : " <<
      Domain.c_str() << " File " << FileName.c_str());
  }

  if (this->tree==NULL) {
    std::string filename = FileName;
    herr_t status;
    this->AccessPlist = H5Pcreate(H5P_FILE_ACCESS);
    if (this->AccessPlist>=0) {
      Debug("property list created with Id " << this->AccessPlist);
    }
    else {
      Error("file access property list creation failed");
    }
    if (this->DSMManager && ds->GetDomain()==std::string("DSM")) {
      // Set up file access property list with DSM
      H5FD_dsm_init();
      status = H5Pset_fapl_dsm(this->AccessPlist, H5FD_DSM_INCREMENT, this->DSMManager->GetDSMHandle());
    }
    else {
      filename = std::string(ds->GetWorkingDirectory()) + "/" + FileName;
      // Set up file access property list with parallel I/O access
      status = H5Pset_fapl_mpio(this->AccessPlist, this->Communicator, MPI_INFO_NULL);
    }
    if (status<0) {
      Error("setting file access property list information failed");
    }
    this->tree = H5MB_init(filename.c_str());
    this->dataArrays = new ArrayMap();
  }
  XdmfHDF *hdf = dynamic_cast<XdmfHDF*>(ds);
  hssize_t count[5] = { 0,0,0,0,0};
  hdf->GetShape(&count[0]);
  //
  this->dataArrays->datamap.insert( MapType(ds, HeavyType(Path, NULL)));
  //
  char datatype[256];
  size_t len=256;
  herr_t err = H5LTdtype_to_text(hdf->GetDataType(), datatype, H5LT_DDL, &len);
  if (err>=0) {
    H5MB_add(this->tree, Path.c_str(), datatype, hdf->GetRank(), count);
  }
  else {
    H5MB_add(this->tree, Path.c_str(), "TYPE_ERROR", hdf->GetRank(), count);
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
  newArray->CopyShape(array);
  newArray->CopyType(array);
  newArray->CopySelection(array);
  newArray->SetDataPointer(array->GetDataPointer());
  array->DropDataPointer();
  //
  HeavyDataMap::iterator it = this->dataArrays->datamap.find(ds);
  if (it!=this->dataArrays->datamap.end()) {
    it->second.second = newArray;
  }
  else {
    Error("A serious error occurred matching datasets during write ");
  }
  //
  return XDMF_SUCCESS;
}
//----------------------------------------------------------------------------
void H5MBCallback::Synchronize()
{
  H5MB_collect(this->tree, this->Communicator);
//  H5MB_print(this->tree);
  H5MB_create(this->tree, this->Communicator, this->AccessPlist);
  //
  HeavyDataMap::iterator it = this->dataArrays->datamap.begin();
  for (; it != this->dataArrays->datamap.end(); ++it) {
    const char *datasetpath = it->second.first.c_str();
    hid_t Dataset = H5MB_get(this->tree, datasetpath);

    XdmfArray     *data_array = it->second.second;
    XdmfHeavyData *heavy_data = it->first;

    herr_t status = H5Dwrite( Dataset,
      data_array->GetDataType(),
      H5S_ALL,
      H5S_ALL,
      H5P_DEFAULT,
      data_array->GetDataPointer() );

    if ( status < 0 ) {
      Error("writing " << datasetpath);
    }
    else {
      Debug("written " << datasetpath);
    }
  
  }
  // remove all 
  this->dataArrays->datamap.clear();

  // finished this timestep, close everything, but reuse tree/file
  H5MB_close(this->tree, true);
}
//----------------------------------------------------------------------------
void H5MBCallback::CloseTree()
{
  // finished this timestep, close and delete everything
  herr_t status = H5Pclose(this->AccessPlist);
  Debug("property list closed returning " << status);
  if ( status < 0 ) {
    Error("closing property list");
  }
  status = H5MB_close(this->tree, false);
  Debug("tree closed returning " << status);
  if ( status < 0 ) {
    Error("closing tree ");
  }
  this->tree = NULL;
  delete this->dataArrays;
  this->dataArrays = NULL;
}