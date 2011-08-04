/*=========================================================================

  Project                 : Icarus
  Module                  : XdmfH5MBCallback.h

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
#undef H5_DLL
#define H5_DLL
#include "H5MButil.h"
#include "vtkSmartPointer.h"

class ArrayMap;
class vtkDsmManager;
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
    void SetDsmManager(vtkDsmManager*);

  private:
    MPI_Comm Communicator;
    int Rank;
    int Size;
    H5MB_tree_type *tree;
    ArrayMap       *dataArrays;
    // Used for DSM write
    vtkSmartPointer<vtkDsmManager> DsmManager;
    hid_t AccessPlist;

}; 
//----------------------------------------------------------------------------
