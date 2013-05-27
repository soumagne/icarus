/*=========================================================================

  Project                 : Icarus
  Module                  : vtkFlattenOneBlock.h

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
  vtkTypeMacro(vtkFlattenOneBlock,vtkDataSetAlgorithm);
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
  virtual ~vtkFlattenOneBlock();

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


