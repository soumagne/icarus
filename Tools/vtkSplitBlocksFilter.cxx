/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile: vtkSplitBlocksFilter.cxx,v $

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkSplitBlocksFilter.h"

#include "vtkModifiedBSPTree.h"
#include "vtkKdTree.h"
#include "vtkCompositeDataIterator.h"
#include "vtkIdList.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkToolkits.h"
#include "vtkUnstructuredGrid.h"
#include "vtkSmartPointer.h"
#include "vtkTimerLog.h"
#include "vtkDataObjectTypes.h"
#include "vtkSelectionSource.h"
#include "vtkSelectionNode.h"
#include "vtkExtractSelection.h"

// For PARAVIEW_USE_MPI
#include "vtkPVConfig.h"
#ifdef PARAVIEW_USE_MPI
#include "vtkMPIController.h"
#endif

#include <std/vector>
//----------------------------------------------------------------------------
vtkCxxRevisionMacro(vtkSplitBlocksFilter, "$Revision: 1.53 $")
vtkStandardNewMacro(vtkSplitBlocksFilter)
//----------------------------------------------------------------------------
vtkSplitBlocksFilter::vtkSplitBlocksFilter()
{
  this->BSPtree              = NULL;
  this->RetainBSPtree        = 1;
  this->UpdatePiece          = 0;
  this->UpdateNumPieces      = 0;
  this->Timing               = 0;
  this->MaximumCellsPerBlock = 4096;
  this->SubdivisionValid     = 0;
  this->OutputNumberOfPieces = 0;
  this->Timer                = vtkSmartPointer<vtkTimerLog>::New();
  this->Controller           = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());
}                        
//----------------------------------------------------------------------------
vtkSplitBlocksFilter::~vtkSplitBlocksFilter()
{ 
  if (this->BSPtree)
    {
    this->BSPtree->Delete();
    this->BSPtree = NULL;
    }  
  this->SetController(NULL);
}
//----------------------------------------------------------------------------
void vtkSplitBlocksFilter::SetController(vtkMultiProcessController *c)
{
  if ((c == NULL) || (c->GetNumberOfProcesses() == 0))
    {
    this->UpdateNumPieces = 1;    
    this->UpdatePiece = 0;
    }

  if (this->Controller == c)
    {
    return;
    }

  this->Modified();

  if (this->Controller != NULL)
    {
    this->Controller->UnRegister(this);
    this->Controller = NULL;
    }

  if (c == NULL)
    {
    return;
    }

  this->Controller = c;

  c->Register(this);
  this->UpdateNumPieces = c->GetNumberOfProcesses();
  this->UpdatePiece    = c->GetLocalProcessId();
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//-------------------------------------------------------------------------
int vtkSplitBlocksFilter::RequestUpdateExtent(
  vtkInformation *vtkNotUsed(request),
  vtkInformationVector **inputVector,
  vtkInformationVector *outputVector)
{
  // get the info objects
  vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation *outInfo = outputVector->GetInformationObject(0);

  int piece, numPieces, ghostLevels;
  // This is left in from D3 filter, we are experimenting
  //
  // We require preceding filters to refrain from creating ghost cells.
  //
  piece = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER());
  numPieces = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES());
  ghostLevels = 0;

  inInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER(), piece);
  inInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES(), numPieces);
  inInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_GHOST_LEVELS(), ghostLevels);
  inInfo->Set(vtkStreamingDemandDrivenPipeline::EXACT_EXTENT(), 1);
  //
  return 1;
}
//----------------------------------------------------------------------------
int vtkSplitBlocksFilter::FillInputPortInformation(int port, vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataObject");
  return 1;
}
//----------------------------------------------------------------------------
int vtkSplitBlocksFilter::FillOutputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkMultiBlockDataSet");
  return 1;
}
//----------------------------------------------------------------------------
int vtkSplitBlocksFilter::RequestInformation(
  vtkInformation *vtkNotUsed(request),
  vtkInformationVector **inputVector,
  vtkInformationVector *outputVector)
{
  // get the info objects
  vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation *outInfo = outputVector->GetInformationObject(0);

#ifdef VTK_USE_MPI
  if (this->Controller) {
    this->UpdatePiece = this->Controller->GetLocalProcessId();
    this->UpdateNumPieces = this->Controller->GetNumberOfProcesses();
  }
#else 
  this->UpdatePiece = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER());
#endif
  std::cout << 
    "vtkSplitBlocksFilter UpdateNumPieces " << this->UpdateNumPieces << "\n" <<
    "vtkSplitBlocksFilter UpdatePiece " << this->UpdatePiece << "\n";

  int inMaxPieces = inInfo->Get(vtkStreamingDemandDrivenPipeline::MAXIMUM_NUMBER_OF_PIECES());
  std::cout << "vtkSplitBlocksFilter Input Max Pieces " << inMaxPieces << "\n" << "\n";

  if (this->SubdivisionValid) {
    outInfo->Set(vtkStreamingDemandDrivenPipeline::MAXIMUM_NUMBER_OF_PIECES(), this->OutputNumberOfPieces);  
  }
  return 1;
}

//----------------------------------------------------------------------------
int vtkSplitBlocksFilter::RequestData(
  vtkInformation *vtkNotUsed(request),
  vtkInformationVector **inputVector,
  vtkInformationVector *outputVector)
{
  // get the info objects
  vtkInformation *dataInfo     = inputVector[0]->GetInformationObject(0);
  vtkInformation *outInfo      = outputVector->GetInformationObject(0);

  // get the input and output, check for multiblock probe/output geometry
  vtkDataSet *data = vtkDataSet::SafeDownCast(
    dataInfo->Get(vtkDataObject::DATA_OBJECT()));
  vtkMultiBlockDataSet *mbdata = vtkMultiBlockDataSet::SafeDownCast(
    dataInfo->Get(vtkDataObject::DATA_OBJECT()));
  vtkMultiBlockDataSet *mboutput = vtkMultiBlockDataSet::SafeDownCast(
    outInfo->Get(vtkDataObject::DATA_OBJECT()));

  //
  this->UpdateProgress(0.0);
  this->Timer->StartTimer();
  //
  int blocks = 0;
  if (data) {
    std::cout << "vtkSplitBlocksFilter input : " << 1 << std::endl;
    blocks += this->SubdivideDataSet(data, mboutput);
  }
  else if (mbdata) {
    vtkSmartPointer<vtkCompositeDataIterator> iter;
    iter.TakeReference(mbdata->NewIterator());
    // count total number so we can do a progress bar
    int numDataSets = 0, count = 0;
    while (!iter->IsDoneWithTraversal()) { numDataSets++; iter->GoToNextItem(); }
    std::cout << "vtkSplitBlocksFilter input : " << numDataSets << std::endl;
    //
    for (iter->InitTraversal(); !iter->IsDoneWithTraversal(); iter->GoToNextItem()) {
      vtkDataSet *data = vtkDataSet::SafeDownCast(iter->GetCurrentDataObject());
      vtkStdString name = mbdata->GetMetaData(iter)->Get(vtkCompositeDataSet::NAME());
      //
      if (data) {
        blocks += this->SubdivideDataSet(data, mboutput);
//        mboutput->SetBlock(block, newOutput);
//        mboutput->GetMetaData(block++)->Set(vtkCompositeDataSet::NAME(), name.c_str());
      }
      count++;
      this->UpdateProgress((double)count/numDataSets);
    }
  }
  this->Timer->StopTimer();
  // double time = this->Timer->GetElapsedTime();
  this->UpdateProgress(1.0);
  //
  std::cout << "vtkSplitBlocksFilter output : " << blocks << std::endl;
  this->SubdivisionValid = 1;
  outInfo->Set(vtkStreamingDemandDrivenPipeline::MAXIMUM_NUMBER_OF_PIECES(), this->OutputNumberOfPieces);  
  return 1;
}
//----------------------------------------------------------------------------
vtkSmartPointer<vtkDataSet> vtkSplitBlocksFilter_Copy(vtkDataSet *d) {
  vtkSmartPointer<vtkDataSet> result;
  result.TakeReference(d->NewInstance());
  result->ShallowCopy(d);
  return result;
}
//----------------------------------------------------------------------------
vtkSmartPointer<vtkDataSet> vtkSplitBlocksFilter_CopyOutput(vtkAlgorithm *a, int port) {
  vtkDataSet *dobj = vtkDataSet::SafeDownCast(a->GetOutputDataObject(port));
  return vtkSplitBlocksFilter_Copy(dobj);
}
//----------------------------------------------------------------------------
// use #define BSP to select a vtkModifiedBSPTree, otherwise a vtkKdTree
// #define BSP
//----------------------------------------------------------------------------
int vtkSplitBlocksFilter::SubdivideDataSet(
  vtkDataSet *data, vtkMultiBlockDataSet *mboutput)
{
  // don't want to use the actual dataset as a filter input
  vtkSmartPointer<vtkDataSet> datacopy = vtkSplitBlocksFilter_Copy(data);

  //
  // Build a locator which spatially divides the data
  //
#ifdef BSP
  vtkSmartPointer<vtkModifiedBSPTree> tree = vtkSmartPointer<vtkModifiedBSPTree>::New();
  tree->SetLazyEvaluation(0);
  tree->SetNumberOfCellsPerNode(this->MaximumCellsPerBlock);
  tree->SetNumberOfCellsPerNode(1024);
#else
  vtkSmartPointer<vtkKdTree> tree = vtkSmartPointer<vtkKdTree>::New();
  tree->SetMinCells(1024);
#endif
  tree->SetDataSet(datacopy);
  tree->BuildLocator();

  //
  // Now extract lists of IDs for each leaf node
  //
  std::vector< vtkSmartPointer<vtkIdList> > LeafCellsList;
  //
#ifdef BSP
  tree->GetLeafNodeCellInformation(LeafCellsList);
#else
  tree->CreateCellLists();
  int R = tree->GetNumberOfRegions();
  for (int r=0; r<R; r++) {
    vtkSmartPointer<vtkIdList> ids = tree->GetCellList(r);
    LeafCellsList.push_back(ids);
  }
#endif

  int NotModified = data->GetInformation()->Has(vtkDataObject::DATA_GEOMETRY_UNMODIFIED());
  //
  std::vector< vtkSmartPointer<vtkIdList> >::iterator it=LeafCellsList.begin();
  for (; it!=LeafCellsList.end(); ++it) {
    vtkSmartPointer<vtkSelectionSource> selection = vtkSmartPointer<vtkSelectionSource>::New();
    vtkSmartPointer<vtkExtractSelection>  extract = vtkSmartPointer<vtkExtractSelection>::New();
    selection->SetContentType(vtkSelectionNode::INDICES);
    selection->SetFieldType(vtkSelectionNode::CELL);
    for (int c=0; c<(*it)->GetNumberOfIds(); c++) {
      vtkIdType cellId = (*it)->GetId(c);
      selection->AddID(-1, cellId);
    }
    extract->SetInput(datacopy);
    extract->SetSelectionConnection(selection->GetOutputPort());
    extract->Update();
    vtkSmartPointer<vtkDataSet> newBlock = vtkSplitBlocksFilter_CopyOutput(extract,0);
    if (NotModified) {
      newBlock->GetInformation()->Set(vtkDataObject::DATA_GEOMETRY_UNMODIFIED(),1);
      std::cout << "Set DATA_GEOMETRY_UNMODIFIED after split " << std::endl; 
    }
    mboutput->SetBlock(mboutput->GetNumberOfBlocks(), newBlock);
  }

  return LeafCellsList.size();
}
//-------------------------------------------------------------------------
vtkSmartPointer<vtkDataSet> vtkSplitBlocksFilter::NewOutput(vtkDataSet *data) 
{
  int outputType = data->GetDataObjectType();
  vtkSmartPointer<vtkDataSet> newoutput = vtkDataSet::SafeDownCast(vtkDataObjectTypes::NewDataObject(outputType));
  newoutput->Delete(); // dec ref count
  return newoutput;
}
//----------------------------------------------------------------------------
