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

#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkPointSet.h"
#include "vtkDataArray.h"
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
//----------------------------------------------------------------------------
vtkCxxRevisionMacro(vtkSteeringWriter, "$Revision$");
vtkStandardNewMacro(vtkSteeringWriter);
#ifdef VTK_USE_MPI
vtkCxxSetObjectMacro(vtkSteeringWriter, Controller, vtkMultiProcessController);
#endif
//----------------------------------------------------------------------------
vtkSteeringWriter::vtkSteeringWriter()
{
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
  //
  this->TimeValue                 = 0.0;
  this->NumberOfParticles         = 0;
  this->H5FileId                  = H5I_BADID;
  this->GroupPath                 = NULL;
  this->UpdatePiece               = -1;
  this->UpdateNumPieces           = -1;
#ifdef VTK_USE_MPI
  this->Controller = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());
#endif
}
//----------------------------------------------------------------------------
vtkSteeringWriter::~vtkSteeringWriter()
{ 
  this->CloseFile();
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
  if (this->H5FileId != H5I_BADID) {
    if (H5Fclose(this->H5FileId) < 0) {
      vtkErrorMacro(<<"CloseFile failed");
    }
    this->H5FileId = NULL;
  }
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

  this->H5GroupId = H5Gopen(this->H5FileId, (const char *)this->GroupPath, H5P_DEFAULT);

  if (!this->H5GroupId) {
    vtkErrorMacro(<< "Initialize: Could not open group " << this->GroupPath);
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
//----------------------------------------------------------------------------
void vtkSteeringWriter::WriteDataArray(int i, vtkDataArray *indata)
{
  /*
  // if a process has zero points/scalars, then this routine is entered with
  // a null pointer, we must find out what the other processes are sending 
  // and do an empty send with the same type etc.
  vtkSmartPointer<vtkDataArray> data;
  if (this->UpdateNumPieces>1 && !this->DisableInformationGather) {
    int correctType = -1, numComponents = -1;
    std::string correctName;
    GatherDataArrayInfo(indata, correctType, correctName, numComponents);
    if (!indata) {
      vtkDebugMacro(<<"NULL data found, used MPI_Gather to find :" 
          << " DataType " << correctType
          << " Name " << correctName.c_str()
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

  hid_t dataset;
  hid_t &diskshape = H5FileId->diskshape;
  hid_t &memshape  = H5FileId->memshape;
  if (memshape!=H5S_ALL) {
    if (H5Sclose(memshape)<0) vtkErrorMacro(<<"memshape : HANDLE_H5S_CLOSE_ERR");
    memshape = H5S_ALL;
  }
  herr_t r=0;
  int Nt = data->GetNumberOfTuples();
  int Nc = data->GetNumberOfComponents();
  hsize_t     count1_mem[] = { Nt*Nc };
  hsize_t     count2_mem[] = { Nt };
  hsize_t     offset_mem[] = { 0 };
  hsize_t     stride_mem[] = { Nc };
  //
  vtkSmartPointer<vtkDataArray> component;
  if (!this->VectorsWithStridedWrite) {
    // we copy from original to a single component array
    count1_mem[0] = Nt;
    if (Nc>1) {
      component.TakeReference(data->NewInstance());
      component->SetNumberOfComponents(1);
      component->SetNumberOfTuples(Nt);
      component->WriteVoidPointer(0, Nt);
    }
  }
  char buffer[8];
  char BadChars[] = "/\\:*?\"<> ";
  char typestring[128];
  for (int c=0; c<Nc; c++) {
    // set the array name
    sprintf(buffer,"%i", i);
    vtkstd::string name = vtkstd::string("Scalars_").append(buffer);
    if (data->GetName()) name = data->GetName();
    char *tempname = const_cast<char *>(name.c_str());
    name = vtksys::SystemTools::ReplaceChars(tempname, BadChars, '_');
    // shape
    memshape = H5Screate_simple(1, count1_mem, NULL);   
    // single vector write or component by component
    vtkSmartPointer<vtkDataArray> finalData = data;
    if (Nc>1) {
      sprintf(buffer,"_%i", c);
      name = name.append(buffer);
      if (!this->VectorsWithStridedWrite) {
        this->CopyFromVector(c, data, component);
        finalData = component;
      }
      else {
        offset_mem[0] = c;
        r = H5Sselect_hyperslab(
            memshape, H5S_SELECT_SET, offset_mem, stride_mem, count2_mem, NULL);
      }
    }
    else {
      // we don't need a hyperslab here because we're writing 
      // a contiguous block from mem to disk with the same flat shape
    }
    //
    switch (finalData->GetDataType()) {
    case VTK_FLOAT:
      H5PartWriteDataArray(,H5T_NATIVE_FLOAT, this->H5FileId, name, finalData);
      break;
    case VTK_DOUBLE:
      H5PartWriteDataArray(,H5T_NATIVE_DOUBLE, this->H5FileId, name, finalData);
      break;
    case VTK_CHAR:
              if (VTK_TYPE_CHAR_IS_SIGNED) {
                H5PartWriteDataArray(,H5T_NATIVE_SCHAR, this->H5FileId, name, finalData);
              }
              else {
                H5PartWriteDataArray(,H5T_NATIVE_UCHAR, this->H5FileId, name, finalData);
              }
              break;
            case VTK_SIGNED_CHAR:
              H5PartWriteDataArray(,H5T_NATIVE_SCHAR, this->H5FileId, name, finalData);
              break;
            case VTK_UNSIGNED_CHAR:
              H5PartWriteDataArray(,H5T_NATIVE_UCHAR, this->H5FileId, name, finalData);
              break;
            case VTK_SHORT:
              H5PartWriteDataArray(,H5T_NATIVE_SHORT, this->H5FileId, name, finalData);
              break;
            case VTK_UNSIGNED_SHORT:
              H5PartWriteDataArray(,H5T_NATIVE_USHORT, this->H5FileId, name, finalData);
              break;
            case VTK_INT:
              H5PartWriteDataArray(,H5T_NATIVE_INT, this->H5FileId, name, finalData);
              break;
            case VTK_UNSIGNED_INT:
              H5PartWriteDataArray(,H5T_NATIVE_UINT, this->H5FileId, name, finalData);
              break;
            case VTK_LONG:
              H5PartWriteDataArray(,H5T_NATIVE_LONG, this->H5FileId, name, finalData);
              break;
            case VTK_UNSIGNED_LONG:
              H5PartWriteDataArray(,H5T_NATIVE_ULONG, this->H5FileId, name, finalData);
              break;
            case VTK_LONG_LONG:
              H5PartWriteDataArray(,H5T_NATIVE_LLONG, this->H5FileId, name, finalData);
              break;
            case VTK_UNSIGNED_LONG_LONG:
              H5PartWriteDataArray(,H5T_NATIVE_ULLONG, this->H5FileId, name, finalData);
              break;
            case VTK_ID_TYPE:
              if (VTK_SIZEOF_ID_TYPE==8) {
                H5PartWriteDataArray(,H5T_NATIVE_LLONG, this->H5FileId, name, finalData);
              }
              else if (VTK_SIZEOF_ID_TYPE==4) {
                H5PartWriteDataArray(,H5T_NATIVE_LONG, this->H5FileId, name, finalData);
              }
      break;
    default:
      vtkErrorMacro(<<"Unexpected data type");
    }
    if (memshape!=H5S_ALL) {
      if (H5Sclose(memshape)<0) vtkErrorMacro(<<"memshape : HANDLE_H5S_CLOSE_ERR");
      memshape = H5S_ALL;
    }
    if (dataset>=0 && r<0) {
      vtkErrorMacro(<<"Array write failed for name "
          << name.c_str() << " Timestep " << this->H5FileId->timestep);
    } 
    else if (dataset>=0 && r>=0) {
      vtkDebugMacro(<<"Wrote " << name.c_str() << " " << Nt << " " << Nc << " " << typestring); 
    }
  }
  */
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
  // Make sure file is open
  //
  if (!this->OpenFile()) {
    vtkErrorMacro(<<"Couldn't create file " << this->FileName);
    return;
  }

  //
  // Get the input to write and Set Num-Particles
  // H5PartSetNumParticles does an MPI_All_Gather so we must do it
  // even if we are not writing anything out on this process
  // (ie if this process has zero particles we still do it)
  //
  vtkPointSet *input = this->GetInput();
  this->NumberOfParticles = input->GetNumberOfPoints();
//  H5PartSetNumParticles(this->H5FileId, this->NumberOfParticles);
  //
  // Write coordinate data
  //
  vtkSmartPointer<vtkPoints> points = input->GetPoints();
  if (points && points->GetData()) {
    points->GetData()->SetName("Coords");
    this->WriteDataArray(0, points->GetData());
  }
  else {
    this->WriteDataArray(0, NULL);
  }
  //
  // Write point data
  //
  int numScalars;
  vtkPointData *pd = input->GetPointData();
  numScalars = pd->GetNumberOfArrays();

  for (int i=0; i<numScalars; i++) {
    vtkDataArray *data = pd->GetArray(i);
    this->WriteDataArray(i, data);
  }

}
//----------------------------------------------------------------------------
void vtkSteeringWriter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
