/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile: vtkRedistributeBlocksFilter.h,v $

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

// .NAME vtkRedistributeBlocksFilter - Split blocks into smaller pieces
//
// .SECTION Description
// Takes a multiblock dataset and breaks it into a new dataset containing
// more blocks of smaller sizes
//
// .SECTION See Also
// vtkModifiedBSPTree

#ifndef __vtkRedistributeBlocksFilter_h
#define __vtkRedistributeBlocksFilter_h

#include "vtkDataObjectAlgorithm.h"
#include "vtkSmartPointer.h"

class vtkModifiedBSPTree;
class vtkMultiProcessController;
class vtkDataSet;
class vtkMultiBlockDataSet;
class vtkDataSet;
class vtkTimerLog;

class VTK_EXPORT vtkRedistributeBlocksFilter: public vtkDataObjectAlgorithm
{
  vtkTypeRevisionMacro(vtkRedistributeBlocksFilter, 
    vtkDataObjectAlgorithm);

public:
  static vtkRedistributeBlocksFilter *New();

  // Description:
  //   Set/Get the communicator object
  void SetController(vtkMultiProcessController *c);
  vtkGetObjectMacro(Controller, vtkMultiProcessController);

  // Description:
  // Ensure previous filters don't send up ghost cells
  virtual int RequestUpdateExtent(vtkInformation *, vtkInformationVector **, vtkInformationVector *);
 
  enum Tags
  {
    SEND_DATASET = 9000,
    SEND_STATIC,
  };


protected:
  vtkRedistributeBlocksFilter();
  virtual ~vtkRedistributeBlocksFilter();

  // Description:
  // We accept any old data as input, preferably multi-block
  virtual int FillInputPortInformation(int port, vtkInformation* info);
  virtual int FillOutputPortInformation(int port, vtkInformation* info);

  // Description:
  //   Build a vtkUnstructuredGrid for a spatial region from the 
  //   data distributed across processes.  Execute() must be called
  //   by all processes, or it will hang.

  virtual int RequestData(vtkInformation *, vtkInformationVector **,
    vtkInformationVector *);

  virtual int RequestInformation(vtkInformation *, vtkInformationVector **,
    vtkInformationVector *);
    
private:

  vtkMultiProcessController *Controller;
//BTX
  vtkSmartPointer<vtkTimerLog>       Timer;
//ETX
  int UpdatePiece;
  int UpdateNumPieces;
  int Timing;
  int OutputNumberOfPieces;

  vtkRedistributeBlocksFilter(const vtkRedistributeBlocksFilter&); // Not implemented
  void operator=(const vtkRedistributeBlocksFilter&); // Not implemented
};
#endif
