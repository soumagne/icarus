/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile: vtkSubdivideUnstructuredGrid.cxx,v $

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkSubdivideUnstructuredGrid.h"

#include "vtkCellType.h"
#include "vtkGenericCell.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMergePoints.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkUnstructuredGrid.h"
#include "vtkUnsignedCharArray.h"

vtkCxxRevisionMacro(vtkSubdivideUnstructuredGrid, "$Revision: 1.29 $");
vtkStandardNewMacro(vtkSubdivideUnstructuredGrid);
//----------------------------------------------------------------------------
#define JB_DEBUG__
#ifdef JB_DEBUG__
  #ifdef ___WIN32
    #define OUTPUTTEXT(a) vtkOutputWindowDisplayText(a);
  #else
    #define OUTPUTTEXT(a) std::cout << a; std::cout.flush();
  #endif

  #undef vtkDebugMacro
  #define vtkDebugMacro(p,a)  \
  { \
  vtkOStreamWrapper::EndlType endl; \
  vtkOStreamWrapper::UseEndl(endl); \
  vtkOStrStreamWrapper vtkmsg; \
  vtkmsg << "P(" << p << ") " << a << "\n"; \
  OUTPUTTEXT(vtkmsg.str()); \
  vtkmsg.rdbuf()->freeze(0); \
  }
  #define vtkXDRDebug(a) vtkDebugMacro(this->UpdatePiece,a)
  #define vtkXDRError(a) vtkDebugMacro(this->UpdatePiece,a)
  //
  #undef  vtkGenericWarningMacro
  #define vtkGenericWarningMacro(a) vtkDebugMacro(-1,a)
#else
  #define vtkXDRDebug(a) vtkDebugMacro(a)
  #define vtkXDRError(a) vtkDebugMacro(a)
#endif
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
vtkSubdivideUnstructuredGrid::vtkSubdivideUnstructuredGrid()
{
}
//----------------------------------------------------------------------------
int vtkSubdivideUnstructuredGrid::FillInputPortInformation(int port, vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
  return 1;
}
//----------------------------------------------------------------------------
int vtkSubdivideUnstructuredGrid::RequestData(
  vtkInformation *vtkNotUsed(request),
  vtkInformationVector **inputVector,
  vtkInformationVector *outputVector)
{
  // get the info objects
  vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation *outInfo = outputVector->GetInformationObject(0);

  // get the input and output
   
  this->input = vtkUnstructuredGrid::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));
  this->output = vtkUnstructuredGrid::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));

  vtkIdType numPts = input->GetNumberOfPoints();
  vtkIdType numCells = input->GetNumberOfCells();
  vtkPoints *inPts = input->GetPoints();
  vtkGenericCell *cell;
  vtkPointData *pd = input->GetPointData();
  this->OutputPD = output->GetPointData();
  vtkPoints *newPts;

  vtkUnsignedCharArray *Types = input->GetCellTypesArray();
  unsigned char *types = Types->GetPointer(0);

  vtkGenericWarningMacro("Executing mesh subdivide");

  int NotModified = this->input->GetInformation()->Has(vtkDataObject::DATA_GEOMETRY_UNMODIFIED());

  // Copy original points and point data
  newPts = vtkPoints::New();
  newPts->Allocate(5*numPts,numPts);
  this->OutputPD->InterpolateAllocate(pd,5*numPts,numPts);
  
  output->Allocate(numCells);
  output->SetPoints(newPts);

  this->Locator = vtkSmartPointer<vtkMergePoints>::New();
  this->Locator->InitPointInsertion (newPts, input->GetBounds());

  for (vtkIdType ptId=0; ptId < numPts; ptId++)
    {
    this->Locator->InsertNextPoint(inPts->GetPoint(ptId));
    this->OutputPD->CopyData(pd,ptId,ptId);
    }

  cell = vtkGenericCell::New();

  vtkIdType unchanged = 0;
  vtkIdType tetschanged = 0;
  vtkIdType hexschanged = 0;

  // loop over tetrahedra, generating sixteen new ones for each. This is
  // done by introducing mid-edge nodes and a single mid-tetra node.
  for(vtkIdType cellId=0; cellId < numCells; cellId++)
    {
    input->GetCell(cellId, cell);
    if (types[cellId]==VTK_TETRA) 
      {
      this->SubdivideTetra(cell, pd);
      tetschanged++;
      }
    else if (types[cellId]==VTK_HEXAHEDRON) 
      {
      this->SubdivideHexahedron(cell, pd);
      hexschanged++;
      }
    else 
      {
      unchanged++;
      }
    } //for all cells
  cell->Delete();

  vtkGenericWarningMacro("Subdivided TetraHedra " << tetschanged << " Hexahedra " << hexschanged << " Unchanged " << unchanged);

  newPts->Delete();
  output->Squeeze();
  if (NotModified) {
    output->GetInformation()->Set(vtkDataObject::DATA_GEOMETRY_UNMODIFIED(),1);
    std::cout << "DATA_GEOMETRY_UNMODIFIED after SubdivideUnstructuredGrid " << std::endl; 
  }

  return 1;
}
//----------------------------------------------------------------------------
void vtkSubdivideUnstructuredGrid::SubdivideTetra(vtkGenericCell *cell, vtkPointData *pd)
{
  vtkIdType pts[4];
  double weights[4], x0[3], x1[3], x2[3], x3[3], x[3];
  int p0, p1, p2, p3;
  vtkIdType center, e01, e02, e03, e12, e13, e23;

  // get tetra points
  cell->Points->GetPoint(0,x0);
  cell->Points->GetPoint(1,x1);
  cell->Points->GetPoint(2,x2);
  cell->Points->GetPoint(3,x3);

  p0 = cell->PointIds->GetId(0);
  p1 = cell->PointIds->GetId(1);
  p2 = cell->PointIds->GetId(2);
  p3 = cell->PointIds->GetId(3);

  // compute center point
  weights[0] = weights[1] = weights[2] = weights[3] = 0.25;
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = 0.25*(x0[i] + x1[i] + x2[i] + x3[i]);
    }
  center = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolatePoint(pd, center, cell->PointIds, weights);
  
  // compute edge points
  // edge 0-1
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = 0.5 * (x1[i] + x0[i]);
    }
  e01 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolateEdge(pd, e01, p0, p1, 0.5);
  
  // edge 1-2
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = 0.5 * (x2[i] + x1[i]);
    }
  e12 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolateEdge(pd, e12, p1, p2, 0.5);
  
  // edge 2-0
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = 0.5 * (x2[i] + x0[i]);
    }
  e02 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolateEdge(pd, e02, p2, p0, 0.5);
  
  // edge 0-3
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = 0.5 * (x3[i] + x0[i]);
    }
  e03 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolateEdge(pd, e03, p0, p3, 0.5);
  
  // edge 1-3
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = 0.5 * (x3[i] + x1[i]);
    }
  e13 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolateEdge(pd, e13, p1, p3, 0.5);
  
  // edge 2-3
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = 0.5 * (x3[i] + x2[i]);
    }
  e23 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolateEdge(pd, e23, p2, p3, 0.5);

  // Now create tetrahedra
  // First, four tetra from each vertex
  pts[0] = p0;
  pts[1] = e01;
  pts[2] = e02;
  pts[3] = e03;
  output->InsertNextCell(VTK_TETRA, 4, pts);
  pts[0] = p1;
  pts[1] = e01;
  pts[2] = e12;
  pts[3] = e13;
  output->InsertNextCell(VTK_TETRA, 4, pts);
  pts[0] = p2;
  pts[1] = e02;
  pts[2] = e12;
  pts[3] = e23;
  output->InsertNextCell(VTK_TETRA, 4, pts);
  pts[0] = p3;
  pts[1] = e03;
  pts[2] = e13;
  pts[3] = e23;
  output->InsertNextCell(VTK_TETRA, 4, pts);

  // Now four tetra from cut-off tetra corners
  pts[0] = center;
  pts[1] = e01;
  pts[2] = e02;
  pts[3] = e03;
  output->InsertNextCell(VTK_TETRA, 4, pts);
  pts[1] = e01;
  pts[2] = e12;
  pts[3] = e13;
  output->InsertNextCell(VTK_TETRA, 4, pts);
  pts[1] = e02;
  pts[2] = e12;
  pts[3] = e23;
  output->InsertNextCell(VTK_TETRA, 4, pts);
  pts[1] = e03;
  pts[2] = e13;
  pts[3] = e23;
  output->InsertNextCell(VTK_TETRA, 4, pts);

  // Now four tetra from triangles on tetra faces
  pts[0] = center;
  pts[1] = e01;
  pts[2] = e12;
  pts[3] = e02;
  output->InsertNextCell(VTK_TETRA, 4, pts);
  pts[1] = e01;
  pts[2] = e13;
  pts[3] = e03;
  output->InsertNextCell(VTK_TETRA, 4, pts);
  pts[1] = e12;
  pts[2] = e23;
  pts[3] = e13;
  output->InsertNextCell(VTK_TETRA, 4, pts);
  pts[1] = e02;
  pts[2] = e23;
  pts[3] = e03;
  output->InsertNextCell(VTK_TETRA, 4, pts);
}
//----------------------------------------------------------------------------
void vtkSubdivideUnstructuredGrid::SubdivideHexahedron(vtkGenericCell *cell, vtkPointData *pd)
{
  double weights[8], x0[3], x1[3], x2[3], x3[3], x4[3], x5[3], x6[3], x7[3], x[3];
  int p0, p1, p2, p3, p4, p5, p6, p7;
  vtkIdType ctr, e01, e12, e23, e30, e45, e56, e67, e74, e04, e15, e26, e37;
  vtkIdType fc0154, fc1265, fc2376, fc3047, fc0123, fc4567; 
  vtkSmartPointer<vtkIdList> FaceIdlist = vtkSmartPointer<vtkIdList>::New();
  FaceIdlist->SetNumberOfIds(4);
  vtkIdType *face = FaceIdlist->GetPointer(0);

  // get hex points
  cell->Points->GetPoint(0,x0);
  cell->Points->GetPoint(1,x1);
  cell->Points->GetPoint(2,x2);
  cell->Points->GetPoint(3,x3);
  cell->Points->GetPoint(4,x4);
  cell->Points->GetPoint(5,x5);
  cell->Points->GetPoint(6,x6);
  cell->Points->GetPoint(7,x7);

  p0 = cell->PointIds->GetId(0);
  p1 = cell->PointIds->GetId(1);
  p2 = cell->PointIds->GetId(2);
  p3 = cell->PointIds->GetId(3);
  p4 = cell->PointIds->GetId(4);
  p5 = cell->PointIds->GetId(5);
  p6 = cell->PointIds->GetId(6);
  p7 = cell->PointIds->GetId(7);

  // -------------------------------------------
  // compute center of hexahedron 
  // -------------------------------------------
  weights[0] = weights[1] = weights[2] = weights[3] = 1.0/8.0;
  weights[4] = weights[5] = weights[6] = weights[7] = 1.0/8.0;
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = (1.0/8.0)*(x0[i] + x1[i] + x2[i] + x3[i] + x4[i] + x5[i] + x6[i] + x7[i]);
    }
  ctr = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolatePoint(pd, ctr, cell->PointIds, weights);

  // -------------------------------------------
  // compute centres of each of 6 faces
  // -------------------------------------------
  weights[0] = weights[1] = weights[2] = weights[3] = 1.0/4.0;
  //
  face[0] = p0;   face[1] = p1;   face[2] = p5;   face[3] = p4; 
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = (1.0/4.0)*(x0[i] + x1[i] + x5[i] + x4[i]);
    }
  fc0154 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolatePoint(pd, fc0154, FaceIdlist, weights);

  face[0] = p1;   face[1] = p2;   face[2] = p6;   face[3] = p5; 
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = (1.0/4.0)*(x1[i] + x2[i] + x6[i] + x5[i]);
    }
  fc1265 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolatePoint(pd, fc1265, FaceIdlist, weights);

  face[0] = p2;   face[1] = p3;   face[2] = p7;   face[3] = p6; 
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = (1.0/4.0)*(x2[i] + x3[i] + x7[i] + x6[i]);
    }
  fc2376 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolatePoint(pd, fc2376, FaceIdlist, weights);

  face[0] = p3;   face[1] = p0;   face[2] = p4;   face[3] = p7; 
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = (1.0/4.0)*(x3[i] + x0[i] + x4[i] + x7[i]);
    }
  fc3047 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolatePoint(pd, fc3047, FaceIdlist, weights);

  face[0] = p0;   face[1] = p1;   face[2] = p2;   face[3] = p3; 
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = (1.0/4.0)*(x0[i] + x1[i] + x2[i] + x3[i]);
    }
  fc0123 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolatePoint(pd, fc0123, FaceIdlist, weights);

  face[0] = p4;   face[1] = p5;   face[2] = p6;   face[3] = p7; 
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = (1.0/4.0)*(x4[i] + x5[i] + x6[i] + x7[i]);
    }
  fc4567 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolatePoint(pd, fc4567, FaceIdlist, weights);

  // -------------------------------------------
  // compute centres of each edge (12 edges)
  // -------------------------------------------
  // edge 0-1
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = 0.5 * (x0[i] + x1[i]);
    }
  e01 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolateEdge(pd, e01, p0, p1, 0.5);
  
  // edge 1-2
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = 0.5 * (x1[i] + x2[i]);
    }
  e12 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolateEdge(pd, e12, p1, p2, 0.5);
  
  // edge 2-3
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = 0.5 * (x2[i] + x3[i]);
    }
  e23 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolateEdge(pd, e23, p2, p3, 0.5);
  
  // edge 3-0
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = 0.5 * (x3[i] + x0[i]);
    }
  e30 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolateEdge(pd, e30, p3, p0, 0.5);
  
  // edge 4-5
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = 0.5 * (x4[i] + x5[i]);
    }
  e45 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolateEdge(pd, e45, p4, p5, 0.5);
  
  // edge 5-6
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = 0.5 * (x5[i] + x6[i]);
    }
  e56 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolateEdge(pd, e56, p5, p6, 0.5);
  
  // edge 6-7
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = 0.5 * (x6[i] + x7[i]);
    }
  e67 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolateEdge(pd, e67, p6, p7, 0.5);
 
  // edge 7-4
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = 0.5 * (x7[i] + x4[i]);
    }
  e74 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolateEdge(pd, e74, p7, p4, 0.5);

  // edge 0-4
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = 0.5 * (x0[i] + x4[i]);
    }
  e04 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolateEdge(pd, e04, p0, p4, 0.5);

  // edge 1-5
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = 0.5 * (x1[i] + x5[i]);
    }
  e15 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolateEdge(pd, e15, p1, p5, 0.5);

  // edge 2-6
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = 0.5 * (x2[i] + x6[i]);
    }
  e26 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolateEdge(pd, e26, p2, p6, 0.5);

  // edge 3-7
  for (vtkIdType i=0; i<3; i++)
    {
    x[i] = 0.5 * (x3[i] + x7[i]);
    }
  e37 = this->Locator->InsertNextPoint(x);
  this->OutputPD->InterpolateEdge(pd, e37, p3, p7, 0.5);

  // -------------------------------------------
  // create 8 new hexahedra
  // -------------------------------------------
  vtkIdType pts[8];
//   vtkIdType fc0154, fc1265, fc2376, fc3047, fc0123, fc4567; 

  pts[0] = p0;     pts[1] = e01;    pts[2] = fc0123; pts[3] = e30;
  pts[4] = e04;    pts[5] = fc0154; pts[6] = ctr;    pts[7] = fc3047;
  output->InsertNextCell(VTK_HEXAHEDRON, 8, pts);

  pts[0] = e01;    pts[1] = p1;     pts[2] = e12;    pts[3] = fc0123;
  pts[4] = fc0154; pts[5] = e15;    pts[6] = fc1265; pts[7] = ctr;
  output->InsertNextCell(VTK_HEXAHEDRON, 8, pts);

  pts[0] = e30;    pts[1] = fc0123; pts[2] = e23;    pts[3] = p3;
  pts[4] = fc3047; pts[5] = ctr;    pts[6] = fc2376; pts[7] = e37;
  output->InsertNextCell(VTK_HEXAHEDRON, 8, pts);

  pts[0] = fc0123; pts[1] = e12;    pts[2] = p2;     pts[3] = e23;
  pts[4] = ctr;    pts[5] = fc1265; pts[6] = e26;    pts[7] = fc2376;
  output->InsertNextCell(VTK_HEXAHEDRON, 8, pts);

  pts[0] = e04;    pts[1] = fc0154; pts[2] = ctr;    pts[3] = fc3047;
  pts[4] = p4;     pts[5] = e45;    pts[6] = fc4567; pts[7] = e74;
  output->InsertNextCell(VTK_HEXAHEDRON, 8, pts);
//
  //
  //
  pts[0] = fc0154; pts[1] = e15;    pts[2] = fc1265; pts[3] = ctr;
  pts[4] = e45;    pts[5] = p5;     pts[6] = e56;    pts[7] = fc4567;
  output->InsertNextCell(VTK_HEXAHEDRON, 8, pts);

  pts[0] = fc3047; pts[1] = ctr;    pts[2] = fc2376; pts[3] = e37;
  pts[4] = e74;    pts[5] = fc4567; pts[6] = e67;    pts[7] = p7;
  output->InsertNextCell(VTK_HEXAHEDRON, 8, pts);

  pts[0] = ctr;    pts[1] = fc1265; pts[2] = e26;    pts[3] = fc2376;
  pts[4] = fc4567; pts[5] = e56;    pts[6] = p6;     pts[7] = e67;
  output->InsertNextCell(VTK_HEXAHEDRON, 8, pts);


}
//----------------------------------------------------------------------------
void vtkSubdivideUnstructuredGrid::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
