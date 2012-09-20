/*=========================================================================

  Project                 : Icarus
  Module                  : vtkDsmTableReader.cxx

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
#include "vtkDsmTableReader.h"
//
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkTable.h"
#include "vtkCellArray.h"
#include "vtkPointData.h"
#include "vtkDoubleArray.h"
#include "vtkFloatArray.h"
#include "vtkMath.h"
//
#include "vtkDsmManager.h"
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
#include "H5FDdsm.h"

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
vtkCxxSetObjectMacro(vtkDsmTableReader, DsmManager,  vtkDsmManager);
vtkCxxSetObjectMacro(vtkDsmTableReader, NameStrings, vtkStringList);
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkDsmTableReader);
//----------------------------------------------------------------------------
vtkDsmTableReader::vtkDsmTableReader()
{
  // we override the input and instead generate values directly
  this->SetNumberOfInputPorts(0);
  this->DsmManager           = NULL;
  this->NameStrings          = NULL;
  this->NumberOfTimeSteps    = 0;
  this->LatestTime           = 01E10;
  this->Values               = vtkSmartPointer<vtkPointData>::New();
  this->TimeData             = vtkSmartPointer<vtkDoubleArray>::New();
  this->TimeData->SetName("TimeData");
}
//----------------------------------------------------------------------------
vtkDsmTableReader::~vtkDsmTableReader()
{
  this->SetDsmManager(NULL);
}
//----------------------------------------------------------------------------
void vtkDsmTableReader::SetFileModified()
{
  this->FileModifiedTime.Modified();
  this->Modified();
}
//----------------------------------------------------------------------------
void vtkDsmTableReader::CloseFile()
{
  if (this->DsmManager) {
    this->DsmManager->GetDsmManager()->CloseDSM();
  }
}
//----------------------------------------------------------------------------
int vtkDsmTableReader::OpenFile()
{
  if (!this->DsmManager) {
    vtkWarningMacro("vtkDsmTableReader OpenFile aborted, no DSM manager");
  }

  this->DsmManager->GetDsmManager()->OpenDSM(H5F_ACC_RDONLY);
  this->FileOpenedTime.Modified();

  return 1;
}
//----------------------------------------------------------------------------
int vtkDsmTableReader::ProcessRequest(vtkInformation *request,
    vtkInformationVector **inputVector,
    vtkInformationVector *outputVector)
{
  int result = 0;
  if (this->DsmManager) { 
    if (!this->DsmManager->GetDsmManager()->IsOpenDSM()) {
      this->DsmManager->GetDsmManager()->OpenDSM(H5F_ACC_RDONLY);
      result = this->Superclass::ProcessRequest(request, inputVector, outputVector);
      this->DsmManager->GetDsmManager()->CloseDSM();
    } else {
      result = this->Superclass::ProcessRequest(request, inputVector, outputVector);
    }
  } else {
    std::cout << "No DsmManager in DsmTableReader" << std::endl;
  }

  return result;
}
//----------------------------------------------------------------------------
int vtkDsmTableReader::RequestInformation(
  vtkInformation *request,
  vtkInformationVector **inputVector,
  vtkInformationVector *outputVector)
{
  int result = vtkTableAlgorithm::RequestInformation(request, inputVector, outputVector);
  //
  vtkInformation *outInfo = outputVector->GetInformationObject(0);
  //
  int numT = this->TimeData->GetNumberOfTuples();
  if (numT>0) {
    double *tvals = this->TimeData->GetPointer(0);
    outInfo->Set(vtkStreamingDemandDrivenPipeline::TIME_STEPS(), tvals, numT);
  }

  return result;
}
//---------------------------------------------------------------------------
int vtkDsmTableReader::RequestData(
  vtkInformation *vtkNotUsed(information),
  vtkInformationVector **inputVector,
  vtkInformationVector *outputVector)
{
  vtkTable* output = vtkTable::GetData(outputVector);
  vtkInformation* outputInfo = outputVector->GetInformationObject(0);

  // open DSM
  if (this->DsmManager->GetDsmManager()->OpenDSM(H5F_ACC_RDONLY)!=H5FD_DSM_SUCCESS) {
    vtkErrorMacro("Failed to open DSM in vtkDsmTableReader");
    return 0;
  }   
  
  // did simulation write time into steering values?
  double currenttime = this->LatestTime;
  if (this->DsmManager->GetSteeringValues("TimeValue", 1, &currenttime)==H5FD_DSM_SUCCESS) {
    // value set in currenttime
  }
  else if (outputInfo && outputInfo->Has(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEPS()))
  {
    currenttime = outputInfo->Get(
      vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEPS())[0];
  }

  std::vector<double> datavalues;
  datavalues.assign(this->NameStrings->GetNumberOfStrings(),0.0);
  double value;
  for (int i=0; i<this->NameStrings->GetNumberOfStrings(); i++) {
    if (this->DsmManager->GetSteeringValues(this->NameStrings->GetString(i), 1, &value)==H5FD_DSM_SUCCESS) {
      datavalues[i] = value;
    }
  }

  // close DSM
  this->DsmManager->GetDsmManager()->CloseDSM();

  //
  // if the user rewound the animation, the time will be wrong, reset
  //
  if (currenttime<this->LatestTime) {
    this->Flush();
  }
  this->LatestTime = currenttime;

  //
  // Add the latest time to the time values array
  //
  this->TimeData->InsertNextTuple1(currenttime);
  vtkIdType numT = this->TimeData->GetNumberOfTuples();
  output->AddColumn(this->TimeData);

  //
  // At start we must initialize the field arrays
  //
  if (numT==1) {
    this->DataArrays.clear();
    for (int i=0; i<this->NameStrings->GetNumberOfStrings(); i++) {
      this->DataArrays.push_back(vtkSmartPointer<vtkDoubleArray>::New());
      this->DataArrays[i]->SetName(this->NameStrings->GetString(i));
    }
  }

  //
  // copy data values into point field data 
  //
  for (int i=0; i<this->DataArrays.size(); i++) {
    this->DataArrays[i]->InsertNextTuple(&datavalues[i]);
    output->AddColumn(this->DataArrays[i]);
  }

  // to stop annoying "can't plot with one point" error messages
  // we will always do the first point twice
  if (numT==1) {
    for (int i=0; i<this->NameStrings->GetNumberOfStrings(); i++) {
      this->DataArrays[i]->InsertNextTuple(&datavalues[i]);
    }
    this->TimeData->InsertNextTuple(&currenttime);
  }

  return 1;
}
//---------------------------------------------------------------------------
void vtkDsmTableReader::Flush()
{
  this->Values->Initialize();
  this->TimeData->Initialize();
}
//---------------------------------------------------------------------------
void vtkDsmTableReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
//-----------------------------------------------------------------------------
