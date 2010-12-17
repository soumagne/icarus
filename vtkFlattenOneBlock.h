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

  // Description:
  // Set the block that is to be extracted. Note that the block index
  // is based on the flat index as used by vtkExtractBlock and so it is generally
  // 0 for the root, then 1,2,3,4 for each leaf. More complex heirarchies are not yet
  // supported and may cause failure.
  vtkSetMacro(BlockIndex, int)
  vtkGetMacro(BlockIndex, int)

  // Description:
  // If the block to be extracted is NULL, then we are probably running
  // in parallel, and one process is exporting data, whilst the others have NULL.
  // Unfortunately, because we are flattening the pipeline from Multiblock to simple
  // we must put a dataset on the output, but we need to know what datatype to set.
  // When using the CSCS steering environment, we can parse the template file to
  // get the output types of each block and set them here.
  // The Default DataType shold be set immediately after setting block index
  // as it may change per block Id.
  // The default value is VTK_UNSTRUCTURED_GRID (=4, see vtkType.h for the full list)
  vtkSetMacro(DefaultNullDataType, int)
  vtkGetMacro(DefaultNullDataType, int)

protected:
   vtkFlattenOneBlock();
  ~vtkFlattenOneBlock();

  int BlockIndex;
  int DefaultNullDataType;

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


