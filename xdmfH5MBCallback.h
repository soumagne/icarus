#include "XdmfHeavyData.h"
#undef H5_DLL
#define H5_DLL
#include "H5MButil.h"
#include "vtkSmartPointer.h"

class ArrayMap;
class vtkDSMManager;
//----------------------------------------------------------------------------
class H5MBCallback :
  public XdmfOpenCallback,
  public XdmfWriteCallback,
  public XdmfCloseCallback
{
  public:
    H5MBCallback(MPI_Comm comm);
    //
    XdmfInt32  DoOpen(XdmfHeavyData *ds, XdmfConstString name, XdmfConstString access);
    XdmfInt32  DoClose(XdmfHeavyData *ds);
    XdmfInt32  DoWrite(XdmfHeavyData *ds, XdmfArray *array);

    void Synchronize();
    void CloseTree();

    // Description:
    // Set/Get DsmBuffer Manager and enable DSM data writing instead of using
    // disk. The DSM manager is assumed to be created and initialized before
    // being passed into this routine
    void SetDSMManager(vtkDSMManager*);

  private:
    MPI_Comm Communicator;
    int Rank;
    int Size;
    H5MB_tree_type *tree;
    ArrayMap       *dataArrays;
    // Used for DSM write
    vtkSmartPointer<vtkDSMManager> DSMManager;
    hid_t AccessPlist;

}; 
//----------------------------------------------------------------------------
