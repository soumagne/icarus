/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile: vtkSubdivideBlocksFilter.h,v $

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

// .NAME vtkSubdivideBlocksFilter - Split blocks into smaller pieces
//
// .SECTION Description
// Takes a multiblock dataset and breaks it into a new dataset containing
// more blocks of smaller sizes
//
// .SECTION See Also
// vtkModifiedBSPTree

#ifndef __vtkSubdivideBlocksFilter_h
#define __vtkSubdivideBlocksFilter_h

#include "vtkDataObjectAlgorithm.h"
#include "vtkSmartPointer.h"

class vtkModifiedBSPTree;
class vtkMultiProcessController;
class vtkDataSet;
class vtkMultiBlockDataSet;
class vtkDataSet;
class vtkTimerLog;

class VTK_EXPORT vtkSubdivideBlocksFilter: public vtkDataObjectAlgorithm
{
  vtkTypeRevisionMacro(vtkSubdivideBlocksFilter, 
    vtkDataObjectAlgorithm);

public:
  static vtkSubdivideBlocksFilter *New();

  // Description:
  //   Set/Get the communicator object
  void SetController(vtkMultiProcessController *c);
  vtkGetObjectMacro(Controller, vtkMultiProcessController);

  // Description:
  // When this filter executes, it creates a BSPtree 
  // data structure.  
  vtkBooleanMacro(RetainBSPtree, int);
  vtkGetMacro(RetainBSPtree, int);
  vtkSetMacro(RetainBSPtree, int);

  // Description:
  // Ensure previous filters don't send up ghost cells
  virtual int RequestUpdateExtent(vtkInformation *, vtkInformationVector **, vtkInformationVector *);

  // Description:
  // Turn on collection of timing data
  vtkBooleanMacro(Timing, int);
  vtkSetMacro(Timing, int);
  vtkGetMacro(Timing, int);

  // Description:
  // When subdividing blocks, we set a limit on the number of cells per block
  // that are desired in the output
  vtkSetMacro(MaximumCellsPerBlock, int);
  vtkGetMacro(MaximumCellsPerBlock, int);
  
protected:
  vtkSubdivideBlocksFilter();
  virtual ~vtkSubdivideBlocksFilter();

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
  
  int SubdivideDataSet(vtkDataSet *data, vtkMultiBlockDataSet *mboutput);

  vtkSmartPointer<vtkDataSet> NewOutput(vtkDataSet *data);

  // Description:
  // Overridden to create the correct type of data output. If input is dataset,
  // output is vtkUnstructuredGrid. If input is composite dataset, output is
  // vtkMultiBlockDataSet.
//  virtual int RequestDataObject(vtkInformation*,
//                                vtkInformationVector**,
//                                vtkInformationVector*);
  
private:

  vtkModifiedBSPTree        *BSPtree;
  vtkMultiProcessController *Controller;
//BTX
  vtkSmartPointer<vtkTimerLog>       Timer;
//ETX
  int RetainBSPtree;
  int UpdatePiece;
  int UpdateNumPieces;
  int Timing;
  int MaximumCellsPerBlock;
  int SubdivisionValid;
  int OutputNumberOfPieces;

  vtkSubdivideBlocksFilter(const vtkSubdivideBlocksFilter&); // Not implemented
  void operator=(const vtkSubdivideBlocksFilter&); // Not implemented
};
#endif
