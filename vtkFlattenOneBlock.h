/*=========================================================================

  Project                 : vtkCSCS
  Module                  : $RCSfile: vtkFlattenOneBlock.h,v $
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

// .NAME vtkFlattenOneBlock
// .SECTION Description
// vtkFlattenOneBlock applies vtkStructuredGridGeometryFilter to all
// blocks in vtkHierarchicalDataSet. Place this filter at the end of a
// pipeline before a polydata consumer such as a polydata mapper to extract
// geometry from all blocks and append them to one polydata object.

#ifndef __vtkFlattenOneBlock_h
#define __vtkFlattenOneBlock_h


#include "vtkDataSetAlgorithm.h"

class VTK_EXPORT vtkFlattenOneBlock : public vtkDataSetAlgorithm
{
public:
  static vtkFlattenOneBlock *New();
  vtkTypeRevisionMacro(vtkFlattenOneBlock,vtkDataSetAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent);

  vtkSetMacro(BlockIndex, int)
  vtkGetMacro(BlockIndex, int)

protected:
   vtkFlattenOneBlock();
  ~vtkFlattenOneBlock();

  int BlockIndex;

  virtual int FillInputPortInformation(int port, vtkInformation* info);
  virtual int FillOutputPortInformation(int port, vtkInformation* info);

  // Create a default executive.
  virtual vtkExecutive* CreateDefaultExecutive();

  virtual int RequestDataObject(vtkInformation*, 
                                vtkInformationVector**, 
                                vtkInformationVector*);

  virtual int RequestData(vtkInformation*, 
                          vtkInformationVector**, 
                          vtkInformationVector*);

private:
  vtkFlattenOneBlock(const vtkFlattenOneBlock&);  // Not implemented.
  void operator=(const vtkFlattenOneBlock&);  // Not implemented.
};

#endif


