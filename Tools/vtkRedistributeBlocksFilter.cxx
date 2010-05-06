/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile: vtkRedistributeBlocksFilter.cxx,v $

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkRedistributeBlocksFilter.h"

#include "vtkCompositeDataIterator.h"
#include "vtkIdList.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkDataSet.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkToolkits.h"
#include "vtkSmartPointer.h"
#include "vtkTimerLog.h"
#include "vtkExtentTranslator.h"

#ifdef VTK_USE_MPI
#include "vtkMPIController.h"
#endif

#include "vtkStdString.h"
#include <vtkstd/vector>
#include <vtkstd/numeric>

//----------------------------------------------------------------------------
vtkCxxRevisionMacro(vtkRedistributeBlocksFilter, "$Revision: 1.53 $")
vtkStandardNewMacro(vtkRedistributeBlocksFilter)
//----------------------------------------------------------------------------
vtkRedistributeBlocksFilter::vtkRedistributeBlocksFilter()
{
  this->UpdatePiece          = 0;
  this->UpdateNumPieces      = 0;
  this->Timing               = 0;
  this->OutputNumberOfPieces = 0;
  this->Timer                = vtkSmartPointer<vtkTimerLog>::New();
  this->Controller           = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());
}                        
//----------------------------------------------------------------------------
vtkRedistributeBlocksFilter::~vtkRedistributeBlocksFilter()
{ 
  this->SetController(NULL);
}
//----------------------------------------------------------------------------
void vtkRedistributeBlocksFilter::SetController(vtkMultiProcessController *c)
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
int vtkRedistributeBlocksFilter::RequestUpdateExtent(
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
int vtkRedistributeBlocksFilter::FillInputPortInformation(int port, vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkMultiBlockDataSet");
  return 1;
}
//----------------------------------------------------------------------------
int vtkRedistributeBlocksFilter::FillOutputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkMultiBlockDataSet");
  return 1;
}
//----------------------------------------------------------------------------
int vtkRedistributeBlocksFilter::RequestInformation(
  vtkInformation *vtkNotUsed(request),
  vtkInformationVector **inputVector,
  vtkInformationVector *outputVector)
{
  // get the info objects
  vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);
  // vtkInformation *outInfo = outputVector->GetInformationObject(0);

#ifdef VTK_USE_MPI
  if (this->Controller) {
    this->UpdatePiece = this->Controller->GetLocalProcessId();
    this->UpdateNumPieces = this->Controller->GetNumberOfProcesses();
  }
#else 
  this->UpdatePiece = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER());
#endif
  if (this->UpdatePiece==0) {
  }
  std::cout << 
    "vtkRedistributeBlocksFilter UpdateNumPieces " << this->UpdateNumPieces << "\n" <<
    "vtkRedistributeBlocksFilter UpdatePiece " << this->UpdatePiece << "\n";

  int inMaxPieces = inInfo->Get(vtkStreamingDemandDrivenPipeline::MAXIMUM_NUMBER_OF_PIECES());
  std::cout << "Input Max Pieces " << inMaxPieces << "\n" << "\n";

  return 1;
}

//----------------------------------------------------------------------------
int vtkRedistributeBlocksFilter::RequestData(
  vtkInformation *vtkNotUsed(request),
  vtkInformationVector **inputVector,
  vtkInformationVector *outputVector)
{
  // get the info objects
  vtkInformation *dataInfo     = inputVector[0]->GetInformationObject(0);
  vtkInformation *outInfo      = outputVector->GetInformationObject(0);

  // get the input and output, check for multiblock probe/output geometry
  vtkMultiBlockDataSet *mbdata = vtkMultiBlockDataSet::SafeDownCast(
    dataInfo->Get(vtkDataObject::DATA_OBJECT()));
  vtkMultiBlockDataSet *mboutput = vtkMultiBlockDataSet::SafeDownCast(
    outInfo->Get(vtkDataObject::DATA_OBJECT()));

  if (!mbdata) {
    vtkErrorMacro(<<"This filter requires Multiblock input");
    return 0;
  }
  //
  this->UpdateProgress(0.0);
  this->Timer->StartTimer();

  //
  // How many blocks do we have on our input
  //
  vtkSmartPointer<vtkCompositeDataIterator> iter;
  iter.TakeReference(mbdata->NewIterator());
  int numDataSets = 0;
  while (!iter->IsDoneWithTraversal()) { numDataSets++; iter->GoToNextItem(); }

  //
  // How many blocks do all the other processes have
  //
  vtkMPICommunicator* com = vtkMPICommunicator::SafeDownCast(this->Controller->GetCommunicator()); 
  if (com == 0) {
    vtkErrorMacro("MPICommunicator needed for this filter");
    return 0;
  }
  // 
  vtkstd::vector<int> BlocksPerProcess(this->UpdateNumPieces);
  com->AllGather(&numDataSets, &BlocksPerProcess[0], 1);
  int totalblocks = vtkstd::accumulate(BlocksPerProcess.begin(), BlocksPerProcess.end(), 0);
  vtkstd::vector<int> PartialSum(this->UpdateNumPieces+1);
  vtkstd::partial_sum(BlocksPerProcess.begin(), BlocksPerProcess.end(), PartialSum.begin());

  //
  // Allocate blocks to each process, build a list of who will get what
  //
  vtkstd::vector<int> Destinations(this->UpdateNumPieces);
  vtkSmartPointer<vtkExtentTranslator> extTran = vtkSmartPointer<vtkExtentTranslator>::New();
    extTran->SetSplitModeToBlock();
    int WholeExtent[6] = { 0, totalblocks, 0, 0, 0, 0 };
    extTran->SetNumberOfPieces(this->UpdateNumPieces);
    extTran->SetWholeExtent(WholeExtent);
    for (int i=0; i<this->UpdateNumPieces; i++) {
      extTran->SetPiece(i);
      extTran->PieceToExtent();
      int PartitionExtents[6];
      extTran->GetExtent(PartitionExtents);
      Destinations[i] = PartitionExtents[1];
    }

//  std::cout << "vtkRedistributeBlocksFilter Blocks " << this->UpdatePiece << " " << PartitionExtents[0] << " " << PartitionExtents[1] << std::endl;
  //
  //
  //
  int src=0, dest=0;
  for (int i=0; i<totalblocks; i++) {
    while (i>=PartialSum[src]) src++;
    while (i>=Destinations[dest]) dest++;
    //
    if (src!=this->UpdatePiece && dest!=this->UpdatePiece) {
      // no action, just do nothing
    }
    else if (src==dest) {
      int srcindex = (src>0) ? (i-PartialSum[src-1]) : i;
      vtkDataObject *localblock = mbdata->GetBlock(srcindex);
      mboutput->SetBlock(mboutput->GetNumberOfBlocks(), localblock);
      int NotModified = localblock->GetInformation()->Has(vtkDataObject::DATA_GEOMETRY_UNMODIFIED());
      if (NotModified) {
        std::cout << "DATA_GEOMETRY_UNMODIFIED after redistribute (localblock)" << std::endl; 
      }
    }
    else if (src==this->UpdatePiece) {
      std::cout << "vtkRedistributeBlocksFilter " << this->UpdatePiece << " Sending block " 
        << i << " to " << dest << std::endl;
     int srcindex = (src>0) ? (i-PartialSum[src-1]) : i;
     vtkDataSet *localblock = vtkDataSet::SafeDownCast(
        mbdata->GetBlock(srcindex));
      com->Send(localblock, dest, vtkRedistributeBlocksFilter::SEND_DATASET);
      int NotModified = localblock->GetInformation()->Has(vtkDataObject::DATA_GEOMETRY_UNMODIFIED());
      com->Send(&NotModified, 1, dest, SEND_STATIC);
      std::cout << "vtkRedistributeBlocksFilter " << this->UpdatePiece << " Sent " 
        << localblock->GetNumberOfCells() << std::endl;
    }
    else if (dest==this->UpdatePiece) {
      std::cout << "vtkRedistributeBlocksFilter " << this->UpdatePiece << " Receiving block " 
        << i << " from " << src << std::endl;
      vtkDataSet *newblock = vtkDataSet::SafeDownCast(
        com->ReceiveDataObject(src, vtkRedistributeBlocksFilter::SEND_DATASET));
      std::cout << "vtkRedistributeBlocksFilter " << this->UpdatePiece << " Received " 
        << newblock->GetNumberOfCells() << std::endl;
      mboutput->SetBlock(mboutput->GetNumberOfBlocks(), newblock);
      int NotModified;
      com->Receive(&NotModified, 1, src, SEND_STATIC);
      if (NotModified) {
        newblock->GetInformation()->Set(vtkDataObject::DATA_GEOMETRY_UNMODIFIED(),1);
        std::cout << "DATA_GEOMETRY_UNMODIFIED after redistribute (receive)" << std::endl; 
      }
    }
    com->Barrier();
    this->UpdateProgress((double)i/numDataSets);
  }

  this->Timer->StopTimer();
  // double time = this->Timer->GetElapsedTime();
  this->UpdateProgress(1.0);
  //
  outInfo->Set(vtkStreamingDemandDrivenPipeline::MAXIMUM_NUMBER_OF_PIECES(), this->OutputNumberOfPieces);  
  std::cout << "vtkRedistributeBlocksFilter output : " << mboutput->GetNumberOfBlocks() 
    << std::endl << std::endl;
  com->Barrier();
  return 1;
}
//----------------------------------------------------------------------------
vtkSmartPointer<vtkDataSet> vtkRedistributeBlocksFilter_Copy(vtkDataSet *d) {
  vtkSmartPointer<vtkDataSet> result;
  result.TakeReference(d->NewInstance());
  result->ShallowCopy(d);
  return result;
}
//----------------------------------------------------------------------------
vtkSmartPointer<vtkDataSet> vtkRedistributeBlocksFilter_CopyOutput(vtkAlgorithm *a, int port) {
  vtkDataSet *dobj = vtkDataSet::SafeDownCast(a->GetOutputDataObject(port));
  return vtkRedistributeBlocksFilter_Copy(dobj);
}
//----------------------------------------------------------------------------
