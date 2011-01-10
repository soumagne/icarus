/*=========================================================================

  Project                 : Icarus
  Module                  : vtkXdmfReader4.cxx

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
#include "vtkXdmfReader4.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkMultiProcessController.h"
#include "vtkStreamingDemandDrivenPipeline.h"
//
#include "vtkDSMManager.h"
//
//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkXdmfReader4);
vtkCxxSetObjectMacro(vtkXdmfReader4, Controller, vtkMultiProcessController);
//----------------------------------------------------------------------------
vtkXdmfReader4::vtkXdmfReader4()
{
  this->Controller = NULL;
  this->DSMManager = 0;
  this->TimeRange[0] = 0.0;
  this->TimeRange[1] = 0.0;
}
//----------------------------------------------------------------------------
vtkXdmfReader4::~vtkXdmfReader4()
{
  this->SetController(NULL);
}
//----------------------------------------------------------------------------
void vtkXdmfReader4::SetDSMManager(vtkDSMManager* dsmmanager)
{
  this->DSMManager = dsmmanager;
  this->SetDsmBuffer(dsmmanager->GetDSMHandle());
}
//----------------------------------------------------------------------------
bool vtkXdmfReader4::PrepareDsmBufferDocument()
{
  if (this->DSMManager && this->DSMManager->GetXMLStringReceive()) {
    this->SetReadFromInputString(1);
    this->SetInputString(this->DSMManager->GetXMLStringReceive());
  }
  return true;
}
//----------------------------------------------------------------------------
int vtkXdmfReader4::RequestInformation(
  vtkInformation *request,
  vtkInformationVector **inputVector,
  vtkInformationVector *outputVector)
{
  int result = vtkXdmfReader::RequestInformation(request, inputVector, outputVector);
  //
  if (this->DSMManager) {
    vtkInformation *outInfo = outputVector->GetInformationObject(0);
    outInfo->Set(vtkStreamingDemandDrivenPipeline::TIME_RANGE(), this->TimeRange, 2);
  }
  //
  return result;
}
//----------------------------------------------------------------------------
void vtkXdmfReader4::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

