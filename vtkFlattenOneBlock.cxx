/*=========================================================================

  Project                 : vtkCSCS
  Module                  : $RCSfile: vtkFlattenOneBlock.cpp,v $
  Revision of last commit : $Rev: 155 $
  Author of last commit   : $Author: biddisco $
  Date of last commit     : $Date:: 2006-07-13 10:23:31 +0200 #$

  Copyright (c) CSCS - Swiss National Supercomputing Centre.
  You may use modify and and distribute this code freely providing this
  copyright notice appears on all copies of source code and an
  acknowledgment appears with any substantial usage of the code.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

=========================================================================*/
#include "vtkFlattenOneBlock.h"

#include "vtkCompositeDataIterator.h"
#include "vtkCompositeDataPipeline.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkInformationVector.h"
#include "vtkDataSet.h"
#include "vtkFieldData.h"
#include "vtkInformation.h"
#include "vtkSmartPointer.h"
#include "vtkDataObjectTypes.h"
#include "vtkObjectFactory.h"
#include "vtkInstantiator.h"
#include "vtkImageData.h"
#include <vtkstd/vector>
//----------------------------------------------------------------------------
vtkCxxRevisionMacro(vtkFlattenOneBlock, "$Revision: 1.2 $");
vtkStandardNewMacro(vtkFlattenOneBlock);
//----------------------------------------------------------------------------
vtkFlattenOneBlock::vtkFlattenOneBlock()
{
  this->BlockIndex = 1;
  this->DefaultNullDataType = VTK_UNSTRUCTURED_GRID;
}
//----------------------------------------------------------------------------
vtkFlattenOneBlock::~vtkFlattenOneBlock()
{
}
//----------------------------------------------------------------------------
int vtkFlattenOneBlock::FillInputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  // we'll take anything, composite or normal
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataObject");
  return 1;
}

//----------------------------------------------------------------------------
int vtkFlattenOneBlock::FillOutputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkDataObject");
  return 1;
}
//----------------------------------------------------------------------------
int vtkFlattenOneBlock::RequestDataObject(
  vtkInformation *, 
  vtkInformationVector  **inputVector, 
  vtkInformationVector *outputVector)
{
  vtkInformation* outInfo = outputVector->GetInformationObject(0);
  vtkDataObject *output = vtkDataObject::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));
  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
  if (!inInfo) {
    return 0;
  }
  int output_type = output ? output->GetDataObjectType() : -1;
  
  vtkMultiBlockDataSet *inputCD = vtkMultiBlockDataSet::GetData(inInfo);
  vtkDataSet           *inputDS = vtkDataSet::GetData(inInfo);
  vtkDataSet           *newOutput = 0;

  if (inputCD) {
    if (inputCD->GetBlock(this->BlockIndex-1)) {
      int in_type = inputCD->GetBlock(this->BlockIndex-1)->GetDataObjectType();
      if (in_type==output_type) return 1;
      newOutput = vtkDataSet::SafeDownCast(vtkDataObjectTypes::NewDataObject(in_type));
    }
    else {
      // we do not know what the output type will be, use default
      std::cout << vtkDataObjectTypes::GetClassNameFromTypeId(this->DefaultNullDataType) << " inserted by default" << std::endl;
      newOutput = vtkDataSet::SafeDownCast(vtkDataObjectTypes::NewDataObject(this->DefaultNullDataType));
    }
  }
  else if (inputDS) {
    if (inputDS->GetDataObjectType()==output_type) {
      return 1;
    }
    newOutput = vtkDataSet::SafeDownCast(vtkDataObjectTypes::NewDataObject(inputDS->GetDataObjectType()));
  }
  else {
    // we do not know what the output type will be, use default
    std::cout << vtkDataObjectTypes::GetClassNameFromTypeId(this->DefaultNullDataType) << " inserted by default" << std::endl;
    newOutput = vtkDataSet::SafeDownCast(vtkDataObjectTypes::NewDataObject(this->DefaultNullDataType));
  }
  if (newOutput) {
    newOutput->SetPipelineInformation(outInfo);
    newOutput->Delete();
    this->GetOutputPortInformation(0)->Set(
      vtkDataObject::DATA_EXTENT_TYPE(), newOutput->GetExtentType());
    return 1;
  }

  return 0;
}
//----------------------------------------------------------------------------
int vtkFlattenOneBlock::RequestData(
  vtkInformation*, 
  vtkInformationVector** inputVector, 
  vtkInformationVector*  outputVector)
{
  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
  vtkMultiBlockDataSet *inputC = vtkMultiBlockDataSet::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));
  vtkDataSet           *inputD = vtkDataSet::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));
  if (!inputC && !inputD) {return 0;}
  //
  vtkInformation* outInfo = outputVector->GetInformationObject(0);
  vtkDataSet    *dsout = vtkDataSet::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));
  if (!dsout) {return 0;}
  //
  if (inputD) {
    dsout->ShallowCopy(inputD);
    return 1;
  }
  else {
    inputD = vtkDataSet::SafeDownCast(inputC->GetBlock(this->BlockIndex-1));
    if (inputD) dsout->ShallowCopy(inputD);
    return 1;
  }
  return 0;
}
//----------------------------------------------------------------------------
vtkExecutive* vtkFlattenOneBlock::CreateDefaultExecutive()
{
  return vtkCompositeDataPipeline::New();
}
//----------------------------------------------------------------------------
void vtkFlattenOneBlock::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
