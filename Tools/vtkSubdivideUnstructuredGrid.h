/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile: vtkSubdivideUnstructuredGrid.h,v $

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkSubdivideUnstructuredGrid - subdivide one tetrahedron into twelve for every tetra
// .SECTION Description
// This filter subdivides tetrahedra in an unstructured grid into twelve tetrahedra.


#ifndef __vtkSubdivideTetra_h
#define __vtkSubdivideTetra_h

#include "vtkUnstructuredGridAlgorithm.h"
#include "vtkSmartPointer.h"

class vtkGenericCell;
class vtkMergePoints;
class vtkPointData;

class VTK_EXPORT vtkSubdivideUnstructuredGrid : public vtkUnstructuredGridAlgorithm
{
public:
  static vtkSubdivideUnstructuredGrid *New();
  vtkTypeRevisionMacro(vtkSubdivideUnstructuredGrid,vtkUnstructuredGridAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent);

protected:
  vtkSubdivideUnstructuredGrid();
  virtual ~vtkSubdivideUnstructuredGrid() {};

  // Description:
  virtual int FillInputPortInformation(int port, vtkInformation* info);

  int RequestData(vtkInformation *, vtkInformationVector **, vtkInformationVector *);

  // Description:
  // Subdivides one tetrahedron into into twelve tetrahedra.
  void SubdivideTetra(vtkGenericCell *cell, vtkPointData *pd);

  // Description:
  // Subdivides one hexahedron into into eight hexahedra.
  void SubdivideHexahedron(vtkGenericCell *cell, vtkPointData *pd);

  //
  vtkSmartPointer<vtkMergePoints>      Locator;
  vtkSmartPointer<vtkPointData>        OutputPD;
  vtkSmartPointer<vtkUnstructuredGrid> input;
  vtkSmartPointer<vtkUnstructuredGrid> output;

private:
  vtkSubdivideUnstructuredGrid(const vtkSubdivideUnstructuredGrid&);  // Not implemented.
  void operator=(const vtkSubdivideUnstructuredGrid&);  // Not implemented.
};

#endif


