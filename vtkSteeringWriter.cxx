/*=========================================================================

  Project                 : vtkCSCS
  Module                  : vtkSteeringWriter.h
  Revision of last commit : $Rev: 153 $
  Author of last commit   : $Author: biddisco $
  Date of last commit     : $Date:: 2006-07-12 10:09:37 +0200 #$

  Copyright (C) CSCS - Swiss National Supercomputing Centre.
  You may use modify and and distribute this code freely providing 
  1) This copyright notice appears on all copies of source code 
  2) An acknowledgment appears with any substantial usage of the code
  3) If this code is contributed to any other open source project, it 
  must not be reformatted such that the indentation, bracketing or 
  overall style is modified significantly. 

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

=========================================================================*/
#include "vtkSteeringWriter.h"
#include "vtkDSMManager.h"
#include "H5FDdsm.h"
#include "H5MButil.h"

#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkPointData.h"
#include "vtkCellData.h"
#include "vtkPoints.h"
#include "vtkPointSet.h"
#include "vtkDataArray.h"
#include "vtkUnstructuredGrid.h"
//
#include "vtkCharArray.h"
#include "vtkUnsignedCharArray.h"
#include "vtkShortArray.h"
#include "vtkUnsignedShortArray.h"
#include "vtkLongArray.h"
#include "vtkUnsignedLongArray.h"
#include "vtkLongLongArray.h"
#include "vtkUnsignedLongLongArray.h"
#include "vtkIntArray.h"
#include "vtkUnsignedIntArray.h"
#include "vtkFloatArray.h"
#include "vtkDoubleArray.h"
#include "vtkCellArray.h"
#include "vtkCleanUnstructuredGrid.h"
#include "vtkTriangleFilter.h"
//
#ifdef VTK_USE_MPI
#include "vtkMPI.h"
#include "vtkMultiProcessController.h"
#include "vtkMPICommunicator.h"
#endif
//
#include <cstdlib>
#include <algorithm>
//
// vtksys
//
#include <vtksys/SystemTools.hxx>
#include <vtksys/RegularExpression.hxx>
#include <vtksys/ios/iostream>
#include <vtksys/ios/sstream>
#include <vtksys/stl/stdexcept>
//----------------------------------------------------------------------------
vtkCxxRevisionMacro(vtkSteeringWriter, "$Revision$");
vtkStandardNewMacro(vtkSteeringWriter);
#ifdef VTK_USE_MPI
vtkCxxSetObjectMacro(vtkSteeringWriter, DSMManager, vtkDSMManager);
vtkCxxSetObjectMacro(vtkSteeringWriter, Controller, vtkMultiProcessController);
#endif
//----------------------------------------------------------------------------
vtkSteeringWriter::vtkSteeringWriter()
{
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(0);
  //
  this->DSMManager = NULL;
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
  this->SetDSMManager(NULL);
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
  H5Pset_fapl_dsm(dsmFapl, MPI_COMM_WORLD, this->DSMManager->GetDSMHandle());
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
void vtkSteeringWriter::H5WriteDataArray(hid_t mem_space, hid_t file_space, hsize_t mem_type, hid_t group_id, const char *array_name, vtkDataArray *dataarray)
{
  hid_t H5DataSetID = H5Dcreate(this->H5GroupId, array_name, mem_type, file_space, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  if (H5DataSetID<0) {
    vtkErrorMacro(<<"Dataset open failed for " << array_name);
  } else {
    void *dataptr = dataarray->GetVoidPointer(0);
    H5Dwrite(H5DataSetID, mem_type, mem_space, file_space, H5P_DEFAULT, dataptr);
    H5Dclose(H5DataSetID);
  }
}
//----------------------------------------------------------------------------
void vtkSteeringWriter::WriteDataArray(const char *name, vtkDataArray *indata)
{
#define SPLIT_ARRAYS 0

  vtkSmartPointer<vtkDataArray> data = indata;
  //

  hid_t mem_space = H5S_ALL;
  hid_t file_space = -1;

  herr_t r=0;
  int Nt = data->GetNumberOfTuples();
  int Nc = data->GetNumberOfComponents();
  hsize_t     count_mem[2] = { Nt, Nc };
  int rank = 1;
//  hsize_t     offset_mem[] = { 0 };
  //
  vtkSmartPointer<vtkDataArray> component;

  // we copy from original to a single component array
  count_mem[0] = Nt;
#if SPLIT_ARRAYS==1
    if (Nc>1) {
      component.TakeReference(data->NewInstance());
      component->SetNumberOfComponents(1);
      component->SetNumberOfTuples(Nt);
      component->WriteVoidPointer(0, Nt);
    }
#else
  if (Nc>1) rank = 2;
#endif 

  char typestring[128];

#if SPLIT_ARRAYS==1
  for (int c=0; c<Nc; c++) {
#endif 
    // shape
    mem_space = H5Screate_simple(rank, count_mem, NULL);

    // single vector write or component by component
    vtkSmartPointer<vtkDataArray> finalData = data;
#if SPLIT_ARRAYS==1
    if (Nc>1) {
      sprintf(buffer,"_%i", c);
      name = name.append(buffer);

      this->CopyFromVector(c, data, component);
      finalData = component;
    }
    else {
      // we don't need a hyperslab here because we're writing 
      // a contiguous block from mem to disk with the same flat shape
    }
#else
    file_space = H5Screate_simple(rank, count_mem, NULL);
#endif

    switch (finalData->GetDataType()) {
    case VTK_FLOAT:
      if (0 && this->WriteFloatAsDouble) {
        H5WriteDataArray(mem_space, file_space, H5T_NATIVE_DOUBLE, this->H5FileId, name, finalData);
      }
      else {
        H5WriteDataArray(mem_space, file_space, H5T_NATIVE_FLOAT, this->H5FileId, name, finalData);
      }
      break;
    case VTK_DOUBLE:
      H5WriteDataArray(mem_space, file_space, H5T_NATIVE_DOUBLE, this->H5FileId, name, finalData);
      break;
    case VTK_CHAR:
      if (VTK_TYPE_CHAR_IS_SIGNED) {
        H5WriteDataArray(mem_space, file_space, H5T_NATIVE_SCHAR, this->H5FileId, name, finalData);
      }
      else {
        H5WriteDataArray(mem_space, file_space, H5T_NATIVE_UCHAR, this->H5FileId, name, finalData);
      }
      break;
    case VTK_SIGNED_CHAR:
      H5WriteDataArray(mem_space, file_space, H5T_NATIVE_SCHAR, this->H5FileId, name, finalData);
      break;
    case VTK_UNSIGNED_CHAR:
      H5WriteDataArray(mem_space, file_space, H5T_NATIVE_UCHAR, this->H5FileId, name, finalData);
      break;
    case VTK_SHORT:
      H5WriteDataArray(mem_space, file_space, H5T_NATIVE_SHORT, this->H5FileId, name, finalData);
      break;
    case VTK_UNSIGNED_SHORT:
      H5WriteDataArray(mem_space, file_space, H5T_NATIVE_USHORT, this->H5FileId, name, finalData);
      break;
    case VTK_INT:
      H5WriteDataArray(mem_space, file_space, H5T_NATIVE_INT, this->H5FileId, name, finalData);
      break;
    case VTK_UNSIGNED_INT:
      H5WriteDataArray(mem_space, file_space, H5T_NATIVE_UINT, this->H5FileId, name, finalData);
      break;
    case VTK_LONG:
      H5WriteDataArray(mem_space, file_space, H5T_NATIVE_LONG, this->H5FileId, name, finalData);
      break;
    case VTK_UNSIGNED_LONG:
      H5WriteDataArray(mem_space, file_space, H5T_NATIVE_ULONG, this->H5FileId, name, finalData);
      break;
    case VTK_LONG_LONG:
      H5WriteDataArray(mem_space, file_space, H5T_NATIVE_LLONG, this->H5FileId, name, finalData);
      break;
    case VTK_UNSIGNED_LONG_LONG:
      H5WriteDataArray(mem_space, file_space, H5T_NATIVE_ULLONG, this->H5FileId, name, finalData);
      break;
    case VTK_ID_TYPE:
      if (VTK_SIZEOF_ID_TYPE==8) {
        H5WriteDataArray(mem_space, file_space, H5T_NATIVE_LLONG, this->H5FileId, name, finalData);
      }
      else if (VTK_SIZEOF_ID_TYPE==4) {
        H5WriteDataArray(mem_space, file_space, H5T_NATIVE_LONG, this->H5FileId, name, finalData);
      }
      break;
    default:
      vtkErrorMacro(<<"Unexpected data type");
    }
    if (mem_space!=H5S_ALL) {
      if (H5Sclose(mem_space)<0) vtkErrorMacro(<<"memshape : HANDLE_H5S_CLOSE_ERR");
      mem_space = H5S_ALL;
    }
    if (r<0) {
      vtkErrorMacro(<<"Array write failed for name " << name);
    } else {
      vtkDebugMacro(<<"Wrote " << name << " " << Nt << " " << Nc << " " << typestring); 
    }
#if SPLIT_ARRAYS==1
  }
#endif 
  if (file_space!=-1) {
    if (H5Sclose(file_space)<0) vtkErrorMacro(<<"file_space : HANDLE_H5S_CLOSE_ERR");
    file_space = H5S_ALL;
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
  vtkDataSet *input = this->GetInput();
  if (!input) {
    vtkWarningMacro(<<"No input to select arrays from ");
    return;
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

  for (int i=0; i<ArrayTypes.size(); i++) {
    this->GroupPathInternal = GroupPaths[i];
    this->ArrayTypeInternal = ArrayTypes[i];
    this->ArrayNameInternal = ArrayNames[i];

    this->OpenGroup(GroupPathInternal.c_str());

    if (this->ArrayTypeInternal == 0 && input->IsA("vtkPointSet")) {
      //
      // Write coordinate data
      //
      vtkSmartPointer<vtkPoints> points = vtkPointSet::SafeDownCast(input)->GetPoints();
      if (points && points->GetData()) {
        points->GetData()->SetName("Coords");
        this->WriteDataArray(this->ArrayNameInternal.c_str(), points->GetData());
      }
    }

    if (this->ArrayTypeInternal == 1) {
      //
      // Write connectivity data
      //
      vtkSmartPointer<vtkUnstructuredGrid> grid = vtkUnstructuredGrid::SafeDownCast(input);
      vtkSmartPointer<vtkCellArray> cells = grid ? grid->GetCells() : NULL;
      if (!grid && input->IsA("vtkPolyData")) {
        vtkSmartPointer<vtkTriangleFilter> triF = vtkSmartPointer<vtkTriangleFilter>::New();
        vtkSmartPointer<vtkCleanUnstructuredGrid> CtoG = vtkSmartPointer<vtkCleanUnstructuredGrid>::New();
        triF->SetInput(input);
        CtoG->SetInputConnection(triF->GetOutputPort(0));
        CtoG->Update();
        grid = vtkUnstructuredGrid::SafeDownCast(CtoG->GetOutput());
        CtoG->SetInput(NULL);
        cells = grid ? grid->GetCells() : NULL;
        this->WriteConnectivityTriangles(cells);
      }
      else if (cells) {
        this->WriteConnectivityTriangles(cells);
      }
    }

    if (this->ArrayTypeInternal == 2) {
      if (this->FieldType==0) {
        //
        // Write point data
        //
        vtkPointData *pd = input->GetPointData();
        vtkDataArray *data = pd->GetArray(this->ArrayName);
        this->WriteDataArray(this->ArrayNameInternal.c_str(), data);
      }
      if (this->FieldType==1) {
        //
        // Write cell data
        //
        vtkCellData *cd = input->GetCellData();
        vtkDataArray *data = cd->GetArray(this->ArrayName);
        this->WriteDataArray(this->ArrayNameInternal.c_str(), data);
      }
    }

    this->CloseGroup(this->H5GroupId);
  }

  this->CloseFile();
}
//----------------------------------------------------------------------------
void vtkSteeringWriter::WriteConnectivityTriangles(vtkCellArray *cells) {
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
      triptr[tindex++] = 1+cellptr[cindex++];
    }
  }
  this->WriteDataArray(this->ArrayNameInternal.c_str(), intarray);
}
//----------------------------------------------------------------------------
void vtkSteeringWriter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
