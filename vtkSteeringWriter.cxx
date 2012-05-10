/*=========================================================================

  Project                 : Icarus
  Module                  : vtkSteeringWriter.cxx

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
#include "vtkSteeringWriter.h"
//
#include "vtkInformation.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkCellData.h"
#include "vtkUnstructuredGrid.h"
#include "vtkCellArray.h"
#include "vtkCleanUnstructuredGrid.h"
#include "vtkTriangleFilter.h"
//
#include "vtkToolkits.h" // For VTK_USE_MPI
#ifdef VTK_USE_MPI
  #include "vtkMPI.h"
  #include "vtkMPIController.h"
  #include "vtkMPICommunicator.h"
#endif
// Otherwise
#include "vtkMultiProcessController.h"
//
// vtksys
//
#include <vtksys/SystemTools.hxx>
#include <vtksys/RegularExpression.hxx>
//
#include "vtkDsmManager.h"
#include "H5FDdsm.h"
//
#include <numeric>
//----------------------------------------------------------------------------
vtkCxxRevisionMacro(vtkSteeringWriter, "$Revision$");
vtkStandardNewMacro(vtkSteeringWriter);
#ifdef VTK_USE_MPI
vtkCxxSetObjectMacro(vtkSteeringWriter, DsmManager, vtkDsmManager);
vtkCxxSetObjectMacro(vtkSteeringWriter, Controller, vtkMultiProcessController);
#endif
//----------------------------------------------------------------------------
vtkSteeringWriter::vtkSteeringWriter()
{
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(0);
  //
  this->DsmManager = NULL;
  this->ArrayName  = NULL;
  this->WriteDescription = NULL;
  //
  this->TimeValue                 = 0.0;
  this->NumberOfParticles         = 0;
  this->H5GroupId                 = H5I_BADID;
  this->H5FileId                  = H5I_BADID;
  this->GroupPath                 = NULL;
  this->UpdatePiece               = -1;
  this->UpdateNumPieces           = -1;
  this->WriteFloatAsDouble        = 1;
#ifdef VTK_USE_MPI
  this->Controller = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());
#endif
}
//----------------------------------------------------------------------------
vtkSteeringWriter::~vtkSteeringWriter()
{ 
  delete []this->ArrayName;
  this->CloseFile();
  this->SetDsmManager(NULL);
#ifdef VTK_USE_MPI
  this->SetController(NULL);
#endif
}
//----------------------------------------------------------------------------
int vtkSteeringWriter::FillInputPortInformation(int, vtkInformation *info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkPointSet");
  return 1;
}
//----------------------------------------------------------------------------
int vtkSteeringWriter::FillOutputPortInformation(
    int vtkNotUsed(port), vtkInformation* info)
{
  // we might be multiblock, we might be dataset
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkPolyData");
  return 1;
}
//----------------------------------------------------------------------------
vtkSmartPointer<vtkDataSet> SW_Copy(vtkDataSet *d) {
  vtkSmartPointer<vtkDataSet> result;
  result.TakeReference(d->NewInstance());
  result->ShallowCopy(d);
  result->CopyInformation(d);
  result->SetSource(NULL);
  return result;
}
//----------------------------------------------------------------------------
vtkSmartPointer<vtkDataSet> SW_CopyOutput(vtkAlgorithm *a, int port) {
  a->Update();
  vtkDataSet *dobj = vtkDataSet::SafeDownCast(a->GetOutputDataObject(port));
  return SW_Copy(dobj);
}
//----------------------------------------------------------------------------
int vtkSteeringWriter::RequestInformation(
    vtkInformation *vtkNotUsed(request),
    vtkInformationVector **inputVector,
    vtkInformationVector *outputVector)
{
  //  vtkInformation *inInfo  = inputVector[0]->GetInformationObject(0);

  return 1;
}
//----------------------------------------------------------------------------
vtkPointSet* vtkSteeringWriter::GetInput()
{
  return vtkPointSet::SafeDownCast(this->GetInput(0));
}
//----------------------------------------------------------------------------
vtkPointSet* vtkSteeringWriter::GetInput(int port)
{
  return vtkPointSet::SafeDownCast(vtkAbstractParticleWriter::GetInput(port));
}
//----------------------------------------------------------------------------
void vtkSteeringWriter::CloseFile() 
{
  this->CloseGroup(this->H5GroupId);
  //
  if (this->H5FileId != H5I_BADID) {
    if (H5Fclose(this->H5FileId) < 0) {
      vtkErrorMacro(<<"CloseFile failed");
  }
    this->H5FileId = H5I_BADID;
}
}
//----------------------------------------------------------------------------
void vtkSteeringWriter::CloseGroup(hid_t gid) {
  if (this->H5GroupId != H5I_BADID) {
  if (H5Gclose(this->H5GroupId) < 0) {
    vtkErrorMacro(<<"CloseGroup failed");
  }
  this->H5GroupId = H5I_BADID;
}
}
//----------------------------------------------------------------------------
int vtkSteeringWriter::OpenGroup(const char *pathwithoutname) {
   H5E_auto2_t  errfunc;
   void        *errdata;

   // Prevent HDF5 to print out handled errors, first save old error handler
   H5Eget_auto(H5E_DEFAULT, &errfunc, &errdata);
   // Turn off error handling
   H5Eset_auto(H5E_DEFAULT, NULL, NULL);

   this->H5GroupId = H5Gopen(this->H5FileId, pathwithoutname, H5P_DEFAULT);

   if (this->H5GroupId < 0) {
     // Restore previous error handler

     this->H5GroupId = H5Gcreate(this->H5FileId, pathwithoutname, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
     if (this->H5GroupId < 0) {
       // skip leading "/"
       int cursor = 1;
       std::string currentGroupName;
       hid_t new_group = H5I_BADID;
       hid_t old_group = this->H5FileId;
       do {
         if (pathwithoutname[cursor] == '/' || pathwithoutname[cursor] != '\0') {
           // try to open and create groups
           new_group = H5Gopen(old_group, currentGroupName.c_str(), H5P_DEFAULT);
           if (new_group < 0) {
             new_group = H5Gcreate(old_group, currentGroupName.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
           }
           H5Gclose(old_group);
           old_group = new_group;
           currentGroupName.clear();
           currentGroupName = "";
         } else {
           currentGroupName += pathwithoutname[cursor];
         }
         cursor++;
       } while (pathwithoutname[cursor] != '\0');
     }
   }
   H5Eset_auto(H5E_DEFAULT, errfunc, errdata); 
   return 1;
}
//----------------------------------------------------------------------------
int vtkSteeringWriter::OpenFile()
{
  if (this->H5FileId != H5I_BADID) {
    vtkErrorMacro(<<"File not closed");
  }
  hid_t dsmFapl = H5Pcreate(H5P_FILE_ACCESS);
  H5FD_dsm_set_manager(this->DsmManager->GetDsmManager());
  H5Pset_fapl_dsm(dsmFapl, this->DsmManager->GetDsmManager()->GetMpiComm(), NULL, 0);
  this->H5FileId = H5Fopen("dsm", H5F_ACC_RDWR, dsmFapl);
  H5Pclose(dsmFapl);
  dsmFapl = H5I_BADID;
  //
  std::string path = vtksys::SystemTools::GetFilenamePath(this->GroupPathInternal.c_str());

/*
  std::vector<std::string> dirs;
  //
  vtksys::SystemTools::SplitPath(this->GroupPath,dirs,false);
  hid_t location = this->H5FileId;
  // last part of path is name
  for (int i=0; i<dirs.size()-1; i++) {
    std::string dir = dirs[i];
    hid_t gid = H5Gcreate(location, dir.c_str(), H5P_DEFAULT);
    this->group_ids.push_back(gid);
    location = gid;
  }
  this->H5GroupId = location;
*/
  if (!this->H5GroupId) {
    vtkErrorMacro(<< "Initialize: Could not open group " << this->GroupPathInternal.c_str());
    return 0;
  }
   return 1;
}
//----------------------------------------------------------------------------
template <class T1, class T2>
void CopyFromVector_T(int offset, vtkDataArray *source, vtkDataArray *dest)
{
  int N = source->GetNumberOfTuples();
  T1 *sptr = static_cast<T1*>(source->GetVoidPointer(0)) + offset;
  T2 *dptr = static_cast<T2*>(dest->WriteVoidPointer(0,N));
  for (int i=0; i<N; ++i) {
    *dptr++ = *sptr;
    sptr += 3;
  }
}
//----------------------------------------------------------------------------
void vtkSteeringWriter::CopyFromVector(int offset, vtkDataArray *source, vtkDataArray *dest)
{
  switch (source->GetDataType()) 
  {
  case VTK_CHAR:
  case VTK_SIGNED_CHAR:
  case VTK_UNSIGNED_CHAR:
    CopyFromVector_T<char,char>(offset, source, dest);
    break;
  case VTK_SHORT:
    CopyFromVector_T<short int,short int>(offset, source, dest);
    break;
  case VTK_UNSIGNED_SHORT:
    CopyFromVector_T<unsigned short int,unsigned short int>(offset, source, dest);
    break;
  case VTK_INT:
    CopyFromVector_T<int,int>(offset, source, dest);
    break;
  case VTK_UNSIGNED_INT:
    CopyFromVector_T<unsigned int,unsigned int>(offset, source, dest);
    break;
  case VTK_LONG:
    CopyFromVector_T<long int,long int>(offset, source, dest);
    break;
  case VTK_UNSIGNED_LONG:
    CopyFromVector_T<unsigned long int,unsigned long int>(offset, source, dest);
    break;
  case VTK_LONG_LONG:
    CopyFromVector_T<long long,long long>(offset, source, dest);
    break;
  case VTK_UNSIGNED_LONG_LONG:
    CopyFromVector_T<unsigned long long,unsigned long long>(offset, source, dest);
    break;
  case VTK_FLOAT:
    CopyFromVector_T<float,float>(offset, source, dest);
    break;
  case VTK_DOUBLE:
    CopyFromVector_T<double,double>(offset, source, dest);
    break;
  case VTK_ID_TYPE:
    CopyFromVector_T<vtkIdType,vtkIdType>(offset, source, dest);
    break;
  default:
    break;
    vtkErrorMacro(<<"Unexpected data type");
  }
}
//----------------------------------------------------------------------------
void vtkSteeringWriter::H5WriteDataArray(hid_t mem_space, hid_t disk_space,
    hsize_t mem_type, hid_t group_id, const char *array_name, vtkDataArray *dataarray, bool convert)
{
  hid_t H5DataSetID = H5Dcreate(this->H5GroupId, array_name, mem_type, disk_space, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  if (H5DataSetID<0) {
    vtkErrorMacro(<<"Dataset open failed for " << array_name);
  } else {
    void *dataptr = dataarray->GetVoidPointer(0);
    if (convert) mem_type = H5T_NATIVE_FLOAT;
    H5Dwrite(H5DataSetID, mem_type, mem_space, disk_space, H5P_DEFAULT, dataptr);
    H5Dclose(H5DataSetID);
  }
}
//----------------------------------------------------------------------------
struct vtkSW_datainfo {
  int  datatype;
  int  numC;
  vtkSW_datainfo() : datatype(-1), numC(-1) {};
};
//----------------------------------------------------------------------------
bool vtkSteeringWriter::GatherDataArrayInfo(vtkDataArray *data, int &datatype, int &numComponents)
{
#ifdef VTK_USE_MPI
  std::vector< vtkSW_datainfo > datatypes(this->UpdateNumPieces);
  if (data) {
    ((vtkSW_datainfo*)&datatypes[this->UpdatePiece])->datatype = data->GetDataType();
    ((vtkSW_datainfo*)&datatypes[this->UpdatePiece])->numC     = data->GetNumberOfComponents();
  }
  vtkMPICommunicator* com = vtkMPICommunicator::SafeDownCast(
    this->Controller->GetCommunicator()); 
  int result = com->AllGather((char*)&datatypes[this->UpdatePiece], (char*)&datatypes[0], sizeof(vtkSW_datainfo));
  for (int i=0; i<this->UpdateNumPieces; i++) {
    vtkSW_datainfo &newdata = datatypes[i];
    if (newdata.datatype!=-1) {
      datatype = newdata.datatype;
      numComponents = newdata.numC;
    }
  }
  return (result == 1) ;
#else
  return 1;
#endif
}
//----------------------------------------------------------------------------
void vtkSteeringWriter::WriteDataArray(const char *name, vtkDataArray *indata, 
  bool store_offsets, std::vector<int> &parallelOffsets)
{
  vtkMPICommunicator* com = vtkMPICommunicator::SafeDownCast(this->Controller->GetCommunicator()); 
  if (com == 0) {
    vtkErrorMacro("MPICommunicator needed for this filter");
    return;
  }
  // if a process has zero points/scalars, then this routine is entered with
  // a null pointer, we must find out what the other processes are sending 
  // and do an empty send with the same type etc.
  vtkSmartPointer<vtkDataArray> data;
  if (0 && this->UpdateNumPieces>1) {
    int correctType = -1, numComponents = -1;
    std::string correctName;
//    GatherDataArrayInfo(indata, correctType, numComponents);
    if (!indata) {
      vtkDebugMacro(<<"NULL data found, used MPI_Gather to find :" 
        << " DataType " << correctType
        << " Name " << name
        << " NumComponents " << numComponents);
      data = vtkDataArray::CreateDataArray(correctType);
      data->Delete(); // smartpointer copied it
      data->SetNumberOfComponents(numComponents);
      data->SetName(correctName.c_str());
    }
    else {
      data = indata;
    }
  }
  else data = indata;

  //
  int Nt = data->GetNumberOfTuples();
  int Nc = data->GetNumberOfComponents();
  int totaltuples = Nt;
  int rank = (Nc>1) ? 2 : 1 ;

  //
  // if parallel, we must find the total amount (on disk) to be written for this array
  //
  std::vector<int> ArrayOffsets(this->UpdateNumPieces+1);
  ArrayOffsets[0] = 0;
  if (this->UpdateNumPieces>1) {
    std::vector<int> TuplesPerProcess(this->UpdateNumPieces);
    com->AllGather(&Nt, &TuplesPerProcess[0], 1);
    totaltuples = std::accumulate(TuplesPerProcess.begin(), TuplesPerProcess.end(), 0);
    std::partial_sum(TuplesPerProcess.begin(), TuplesPerProcess.end(), ArrayOffsets.begin()+1);
    if (store_offsets) {
      parallelOffsets = ArrayOffsets;
    }
  }
  //
  hsize_t  mem_size[2]    = { Nt, Nc };
  hsize_t  disk_size[2]   = { totaltuples, Nc };
  hsize_t  disk_slab[2]   = { Nt, Nc };
  hsize_t  disk_offset[2] = { ArrayOffsets[this->UpdatePiece], 0 };
  hid_t    mem_space      = H5Screate_simple(rank, mem_size, NULL); 
  hid_t    disk_space     = H5Screate_simple(rank, disk_size, NULL); 
  if (Nt!=totaltuples) {
    H5Sselect_hyperslab(disk_space, H5S_SELECT_SET, disk_offset, NULL, disk_slab, NULL);    
  }

  char typestring[128];

  switch (data->GetDataType()) {
  case VTK_FLOAT:
    if (this->WriteFloatAsDouble) {
      H5WriteDataArray(mem_space, disk_space, H5T_NATIVE_DOUBLE, this->H5FileId, name, data, true);
    }
    else {
      H5WriteDataArray(mem_space, disk_space, H5T_NATIVE_FLOAT, this->H5FileId, name, data);
    }
    break;
  case VTK_DOUBLE:
    H5WriteDataArray(mem_space, disk_space, H5T_NATIVE_DOUBLE, this->H5FileId, name, data);
    break;
  case VTK_CHAR:
    if (VTK_TYPE_CHAR_IS_SIGNED) {
      H5WriteDataArray(mem_space, disk_space, H5T_NATIVE_SCHAR, this->H5FileId, name, data);
    }
    else {
      H5WriteDataArray(mem_space, disk_space, H5T_NATIVE_UCHAR, this->H5FileId, name, data);
    }
    break;
  case VTK_SIGNED_CHAR:
    H5WriteDataArray(mem_space, disk_space, H5T_NATIVE_SCHAR, this->H5FileId, name, data);
    break;
  case VTK_UNSIGNED_CHAR:
    H5WriteDataArray(mem_space, disk_space, H5T_NATIVE_UCHAR, this->H5FileId, name, data);
    break;
  case VTK_SHORT:
    H5WriteDataArray(mem_space, disk_space, H5T_NATIVE_SHORT, this->H5FileId, name, data);
    break;
  case VTK_UNSIGNED_SHORT:
    H5WriteDataArray(mem_space, disk_space, H5T_NATIVE_USHORT, this->H5FileId, name, data);
    break;
  case VTK_INT:
    H5WriteDataArray(mem_space, disk_space, H5T_NATIVE_INT, this->H5FileId, name, data);
    break;
  case VTK_UNSIGNED_INT:
    H5WriteDataArray(mem_space, disk_space, H5T_NATIVE_UINT, this->H5FileId, name, data);
    break;
  case VTK_LONG:
    H5WriteDataArray(mem_space, disk_space, H5T_NATIVE_LONG, this->H5FileId, name, data);
    break;
  case VTK_UNSIGNED_LONG:
    H5WriteDataArray(mem_space, disk_space, H5T_NATIVE_ULONG, this->H5FileId, name, data);
    break;
  case VTK_LONG_LONG:
    H5WriteDataArray(mem_space, disk_space, H5T_NATIVE_LLONG, this->H5FileId, name, data);
    break;
  case VTK_UNSIGNED_LONG_LONG:
    H5WriteDataArray(mem_space, disk_space, H5T_NATIVE_ULLONG, this->H5FileId, name, data);
    break;
  case VTK_ID_TYPE:
    if (VTK_SIZEOF_ID_TYPE==8) {
      H5WriteDataArray(mem_space, disk_space, H5T_NATIVE_LLONG, this->H5FileId, name, data);
    }
    else if (VTK_SIZEOF_ID_TYPE==4) {
      H5WriteDataArray(mem_space, disk_space, H5T_NATIVE_LONG, this->H5FileId, name, data);
    }
    break;
  default:
    vtkErrorMacro(<<"Unexpected data type");
  }
  if (mem_space!=H5S_ALL) {
    if (H5Sclose(mem_space)<0) vtkErrorMacro(<<"memshape : HANDLE_H5S_CLOSE_ERR");
    mem_space = H5S_ALL;
  }
  vtkDebugMacro(<<"Wrote " << name << " " << Nt << " " << Nc << " " << typestring); 
  if (disk_space!=H5S_ALL) {
    if (H5Sclose(disk_space)<0) vtkErrorMacro(<<"disk_space : HANDLE_H5S_CLOSE_ERR");
  }
}
//----------------------------------------------------------------------------
void vtkSteeringWriter::WriteData()
{
#ifdef VTK_USE_MPI
  if (this->Controller) {
    this->UpdatePiece = this->Controller->GetLocalProcessId();
    this->UpdateNumPieces = this->Controller->GetNumberOfProcesses();
  }
  else {
    this->UpdatePiece = 0;
    this->UpdateNumPieces = 1;
  }
#else
  this->UpdatePiece = 0;
  this->UpdateNumPieces = 1;
#endif

  //
  // Get the input to write and Set Num-Particles
  //
  vtkDataSet *input = this->GetInput();
  if (!input) {
    vtkWarningMacro(<<"No input to select arrays from ");
    return;
  }
  //
  // To correctly write connectivity, we need to have an unstructured grid
  // (for convenience, we could do it using polydata, but it makes the writer simpler).
  //
  vtkSmartPointer<vtkUnstructuredGrid> grid = vtkUnstructuredGrid::SafeDownCast(input);
  if (!grid && input->IsA("vtkPolyData")) {
    vtkSmartPointer<vtkTriangleFilter> triF = vtkSmartPointer<vtkTriangleFilter>::New();
    vtkSmartPointer<vtkCleanUnstructuredGrid> CtoG = vtkSmartPointer<vtkCleanUnstructuredGrid>::New();
    triF->SetInput(SW_Copy(input));
    CtoG->SetInputConnection(triF->GetOutputPort(0));
    grid = vtkUnstructuredGrid::SafeDownCast(SW_CopyOutput(CtoG, 0));
    input = grid;
  }

  std::vector<std::string> GroupPaths;
  std::vector<std::string> ArrayNames;
  std::vector<int>         ArrayTypes;
  //
  if (this->WriteDescription) {
    std::string str = this->WriteDescription;
    vtksys::RegularExpression regex_f(":full_path:([^:]*):");
    vtksys::RegularExpression regex_g(":geometry_path:([^:]*):");
    vtksys::RegularExpression regex_t(":topology_path:([^:]*):");
    vtksys::RegularExpression regex_a(":field_path:([^:]*):");
    regex_f.find(str.c_str());
    regex_g.find(str.c_str());
    regex_t.find(str.c_str());
    regex_a.find(str.c_str());
    //
    if (regex_f.match(1).size()>0) {
      // @TODO : Fix this to write the entire dataset
    }
    // points/geometry
    if (regex_g.match(1).size()>0) {
      ArrayTypes.push_back(0);
      GroupPaths.push_back(vtksys::SystemTools::GetFilenamePath(regex_g.match(1)));
      ArrayNames.push_back(vtksys::SystemTools::GetFilenameName(regex_g.match(1)));
    }
    // topology/connectivity
    if (regex_t.match(1).size()>0) {
      ArrayTypes.push_back(1);
      GroupPaths.push_back(vtksys::SystemTools::GetFilenamePath(regex_t.match(1)));
      ArrayNames.push_back(vtksys::SystemTools::GetFilenameName(regex_t.match(1)));
    }
    // scalar/vector
    if (regex_a.match(1).size()>0) {
      // @TODO : make sure this handles the array by name
      ArrayTypes.push_back(2);
      GroupPaths.push_back(vtksys::SystemTools::GetFilenamePath(regex_a.match(1)));
      ArrayNames.push_back(vtksys::SystemTools::GetFilenameName(regex_a.match(1)));
    }
  }
  else {
    ArrayTypes.push_back(this->ArrayType);
    ArrayNames.push_back(this->ArrayName);
    GroupPaths.push_back(this->GroupPath);
  }

  //
  // Make sure file is open
  //
  if (!this->OpenFile()) {
    vtkErrorMacro(<<"Couldn't open file " << this->FileName);
    return;
  }

  std::vector<int> ParallelOffsets(this->UpdateNumPieces+1);

  for (unsigned int i=0; i<ArrayTypes.size(); i++) {
    this->GroupPathInternal = GroupPaths[i];
    this->ArrayTypeInternal = ArrayTypes[i];
    this->ArrayNameInternal = ArrayNames[i];

    this->OpenGroup(GroupPathInternal.c_str());

    if (this->ArrayTypeInternal == 0 && input->IsA("vtkPointSet")) {
      //
      // Write coordinate data, because points on each process must be offset
      // by the ID of previous processes for connectivity, we store the offsets
      // store_offsets = true, apply_offsets=false
      //
      vtkSmartPointer<vtkPoints> points = vtkPointSet::SafeDownCast(input)->GetPoints();
      if (points && points->GetData()) {
        points->GetData()->SetName("Coords");
        this->WriteDataArray(this->ArrayNameInternal.c_str(), points->GetData(), true, ParallelOffsets);
      }
    }

    if (this->ArrayTypeInternal == 1) {
      //
      // Write connectivity data, because points on each process must be offset
      // by the ID of previous processes for connectivity, we apply the offsets
      // computed for the points
      // store_offsets = false, apply_offsets=true
      //
      vtkSmartPointer<vtkCellArray> cells = grid ? grid->GetCells() : NULL;
      if (input->IsA("vtkPolyData")) {
        cells = grid ? grid->GetCells() : NULL;
        this->WriteConnectivityTriangles(cells, ParallelOffsets);
      }
      else if (cells) {
        this->WriteConnectivityTriangles(cells, ParallelOffsets);
      }
    }

    if (this->ArrayTypeInternal == 2) {
      if (this->FieldType==0) {
        //
        // Write point data
        //
        vtkPointData *pd = input->GetPointData();
        vtkDataArray *data = pd->GetArray(this->ArrayName);
        this->WriteDataArray(this->ArrayNameInternal.c_str(), data, false, ParallelOffsets);
      }
      if (this->FieldType==1) {
        //
        // Write cell data
        //
        vtkCellData *cd = input->GetCellData();
        vtkDataArray *data = cd->GetArray(this->ArrayName);
        this->WriteDataArray(this->ArrayNameInternal.c_str(), data, false, ParallelOffsets);
      }
    }

    this->CloseGroup(this->H5GroupId);
  }

  this->CloseFile();
}
//----------------------------------------------------------------------------
void vtkSteeringWriter::WriteConnectivityTriangles(vtkCellArray *cells,
  std::vector<int> &parallelOffsets) 
{
  vtkSmartPointer<vtkIntArray> intarray = vtkSmartPointer<vtkIntArray>::New();
  intarray->SetNumberOfComponents(3);
  intarray->SetNumberOfTuples(cells->GetNumberOfCells());
  vtkIdType *cellptr = cells->GetPointer();
  int *triptr = intarray->GetPointer(0);
  //
  vtkIdType tindex=0, cindex = 0;
  for (vtkIdType i=0; i<cells->GetNumberOfCells(); i++) {
    vtkIdType N = cellptr[cindex++];
    for (vtkIdType p=0; p<N; p++) {
      triptr[tindex++] = 1+cellptr[cindex++]+parallelOffsets[this->UpdatePiece];
    }
  }

  this->WriteDataArray(this->ArrayNameInternal.c_str(), intarray, false, parallelOffsets);
}
//----------------------------------------------------------------------------
void vtkSteeringWriter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
