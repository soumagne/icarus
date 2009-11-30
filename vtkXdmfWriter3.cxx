/*=========================================================================

  Project                 : vtkCSCS
  Module                  : vtkXdmfWriter3.h
  Revision of last commit : $Rev: 598 $
  Author of last commit   : $Author: soumagne $
  Date of last commit     : $Date:: 2009-07-15 22:33:59 +0200 #$

  Copyright (C) CSCS - Swiss National Supercomputing Centre.
  You may use modify and and distribute this code freely providing
  1) This copyright notice appears on all copies of source code
  2) An acknowledgment appears with any substantial usage of the code
  3) If this code is contributed to any other open source project, it
  must not be reformatted such that the indentation, bracketing or
  overall style is modified significantly.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

=========================================================================*/
#include "vtkXdmfWriter3.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkPointSet.h"
#include "vtkDataArray.h"
//
#include "vtkCharArray.h"
#include "vtkUnsignedCharArray.h"
#include "vtkShortArray.h"
#include "vtkUnsignedShortArray.h"
#include "vtkLongArray.h"
#include "vtkUnsignedLongArray.h"
#include "vtkLongLongArray.h"
#include "vtkUnsignedLongLongArray.h"
#include "vtkIntArray.h"
#include "vtkUnsignedIntArray.h"
#include "vtkFloatArray.h"
#include "vtkDoubleArray.h"
#include "vtkUnstructuredGrid.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkCompositeDataIterator.h"
#include "vtkCellType.h"
#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkGenericCell.h"
//
#include "vtkDSMManager.h"
//
#include <stdlib.h>
#include <sstream>
#include <vtkstd/algorithm>
//
// vtksys : this must go before Xdmf includes
//
#include <vtksys/SystemTools.hxx>
//
#include "Xdmf.h"
#include "XdmfLightData.h"
#include <libxml/tree.h>
//
#ifdef VTK_USE_MPI
#include "vtkExtentTranslator.h"
#include "vtkMPI.h"
#include "vtkMultiProcessController.h"
#include "vtkMPICommunicator.h"
#endif
//
//----------------------------------------------------------------------------
vtkCxxRevisionMacro(vtkXdmfWriter3, "$Revision: 598 $");
vtkStandardNewMacro(vtkXdmfWriter3);
#ifdef VTK_USE_MPI
vtkCxxSetObjectMacro(vtkXdmfWriter3, Controller, vtkMultiProcessController);
vtkCxxSetObjectMacro(vtkXdmfWriter3, DSMManager, vtkDSMManager);
#endif
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
#define vtkXdmfCELLMAP1(a,b) if (celltype==a) { data[ctr++] = b; \
  for (int i=0; i<N; ++i) data[ctr++] = pts[i]; return b; }
#define vtkXdmfCELLINCN(a,b) if (celltype==a) { data[ctr++] = b; \
  data[ctr++] = N; for (int i=0; i<N; ++i) data[ctr++] = pts[i]; return b; }
#define vtkXdmfCELLNONE(a,b) if (celltype==a) { return b; }
const int voxel[8] = {0,1,3,2,4,5,7,6};
#define vtkXdmfVOXEL(a,b) if (celltype==a) { data[ctr++] = b; \
  for (int i=0; i<8; ++i) data[ctr++] = pts[voxel[i]]; return b; }
const int pixel[4] = {0,1,3,2};
#define vtkXdmfPIXEL(a,b) if (celltype==a) { data[ctr++] = b; \
  for (int i=0; i<4; ++i) data[ctr++] = pts[pixel[i]]; return b; }
//
int VTKToXdmfCellMap(vtkIdType *data, vtkIdType *pts, int celltype, int N, vtkIdType &ctr) {
  vtkXdmfCELLNONE(VTK_EMPTY_CELL             ,XDMF_NOTOPOLOGY);  // ?
  vtkXdmfCELLINCN(VTK_VERTEX                 ,XDMF_POLYVERTEX);
  vtkXdmfCELLINCN(VTK_POLY_VERTEX            ,XDMF_POLYVERTEX);
  vtkXdmfCELLINCN(VTK_LINE                   ,XDMF_POLYLINE);
  vtkXdmfCELLINCN(VTK_POLY_LINE              ,XDMF_POLYLINE);
  vtkXdmfCELLMAP1(VTK_TRIANGLE               ,XDMF_TRI);
  vtkXdmfCELLNONE(VTK_TRIANGLE_STRIP         ,XDMF_NOTOPOLOGY);  // ?
  vtkXdmfCELLINCN(VTK_POLYGON                ,XDMF_POLYGON);
  vtkXdmfPIXEL   (VTK_PIXEL                  ,XDMF_QUAD);
  vtkXdmfCELLMAP1(VTK_QUAD                   ,XDMF_QUAD);
  vtkXdmfCELLMAP1(VTK_TETRA                  ,XDMF_TET);
  vtkXdmfVOXEL   (VTK_VOXEL                  ,XDMF_HEX);
  vtkXdmfCELLMAP1(VTK_HEXAHEDRON             ,XDMF_HEX);
  vtkXdmfCELLMAP1(VTK_WEDGE                  ,XDMF_WEDGE);
  vtkXdmfCELLMAP1(VTK_PYRAMID                ,XDMF_PYRAMID);
  vtkXdmfCELLNONE(VTK_PENTAGONAL_PRISM       ,XDMF_NOTOPOLOGY);  // ?
  vtkXdmfCELLNONE(VTK_HEXAGONAL_PRISM        ,XDMF_NOTOPOLOGY);  // ?
  vtkXdmfCELLMAP1(VTK_QUADRATIC_TRIANGLE     ,XDMF_TRI_6);
  vtkXdmfCELLMAP1(VTK_QUADRATIC_QUAD         ,XDMF_QUAD_8);
  vtkXdmfCELLMAP1(VTK_QUADRATIC_TETRA        ,XDMF_TET_10);
  vtkXdmfCELLMAP1(VTK_QUADRATIC_PYRAMID      ,XDMF_PYRAMID_13);
  vtkXdmfCELLMAP1(VTK_QUADRATIC_WEDGE        ,XDMF_WEDGE_15);
  vtkXdmfCELLMAP1(VTK_QUADRATIC_HEXAHEDRON   ,XDMF_HEX_20);
  return XDMF_NOTOPOLOGY;
}
//----------------------------------------------------------------------------
#define vtk2XdmfTYPEMAP(a,b) if (datatype==a) return b;
//
int VTKToXdmfTypeMap(int datatype) {
  vtk2XdmfTYPEMAP(VTK_UNSIGNED_CHAR  ,XDMF_UINT8_TYPE);
  vtk2XdmfTYPEMAP(VTK_UNSIGNED_SHORT ,XDMF_UINT16_TYPE);
  vtk2XdmfTYPEMAP(VTK_UNSIGNED_INT   ,XDMF_UINT32_TYPE);
  vtk2XdmfTYPEMAP(VTK_CHAR           ,XDMF_INT8_TYPE);
  vtk2XdmfTYPEMAP(VTK_SHORT          ,XDMF_INT16_TYPE);
  vtk2XdmfTYPEMAP(VTK_INT            ,XDMF_INT32_TYPE);
  vtk2XdmfTYPEMAP(VTK_FLOAT          ,XDMF_FLOAT32_TYPE);
  vtk2XdmfTYPEMAP(VTK_DOUBLE         ,XDMF_FLOAT64_TYPE);
  if (sizeof(vtkIdType) == sizeof(int)) {
    vtk2XdmfTYPEMAP(VTK_ID_TYPE      ,XDMF_INT32_TYPE);
  }
  else {
    vtk2XdmfTYPEMAP(VTK_ID_TYPE      ,XDMF_INT64_TYPE);
  }
  return -1;
}
//----------------------------------------------------------------------------
vtkSmartPointer<vtkIdTypeArray> ConvertConnectivityArray(
    vtkCellArray *cells, vtkUnsignedCharArray *types, vtkIdType &NumberOfCells)
    {
      vtkIdType  npts, cellId = 0, Idcnt = 0;
      vtkIdType *pts;
      bool warning = false;
      //
      vtkSmartPointer<vtkIdTypeArray> newconnectivity = vtkSmartPointer<vtkIdTypeArray>::New();
      // we must allocate extra space because xdmf
      // connectivity stores xdmf {type,N,pts}, vtk {type, pts}
      //vtkIdType *Idptr = newconnectivity->WritePointer(0, cells->GetNumberOfConnectivityEntries()*2);
      vtkIdType *Idptr = new vtkIdType[cells->GetNumberOfConnectivityEntries()*2];
      newconnectivity->SetVoidArray(Idptr, cells->GetNumberOfConnectivityEntries()*2, 1);
      //
      cells->InitTraversal();
      NumberOfCells = 0;
      while (cells->GetNextCell(npts, pts)) {
        int xdmftype = VTKToXdmfCellMap(Idptr, pts, types->GetValue(cellId), npts, Idcnt);
        if (xdmftype == XDMF_NOTOPOLOGY) warning = true;
        else NumberOfCells++;
        cellId++;
      }

      newconnectivity->SetNumberOfValues(Idcnt);
      if (warning) {
        vtkGenericWarningMacro("A Cell type that is not yet supported by Xdmf was encountered."
            << " The Cell data array will not match the cells written (NumTuples!=NumCells)");
      }
      return newconnectivity;
    }
//----------------------------------------------------------------------------
vtkstd::string GenerateHDF5DataSetName(
    const char *Name, const char *group, const char *txt)
{
  vtkstd::string datasetname = vtkstd::string(Name);
  if (group) {
    datasetname += vtkstd::string(group) + "/";
  }
  datasetname += vtkstd::string(txt);

  return datasetname;
}
//----------------------------------------------------------------------------
void vtk2XdmfArray(XdmfArray *xdmfarray, vtkDataArray *vtkarray,
    const char *Name, const char *group, const char *txt)
{
  vtkstd::string datasetname = GenerateHDF5DataSetName(Name, group, txt ? txt : vtkarray->GetName());
  xdmfarray->SetNumberType(VTKToXdmfTypeMap(vtkarray->GetDataType()));

  xdmfarray->SetHeavyDataSetName(datasetname.c_str());
  xdmfarray->SetDataPointer(vtkarray->GetVoidPointer(0));
  XdmfInt32 Rank = 1;
  XdmfInt64 Dims[2] = {vtkarray->GetNumberOfTuples(), 1};
  if (vtkarray->GetNumberOfComponents()>1) {
    Rank = 2;
    Dims[0] = vtkarray->GetNumberOfTuples();
    Dims[1] = vtkarray->GetNumberOfComponents();
  }
  xdmfarray->SetShape(Rank, Dims);
}
//----------------------------------------------------------------------------
vtkXdmfWriter3::vtkXdmfWriter3()
{
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(0);
  //
  this->FileName                   = NULL;
  this->DomainName                 = NULL;
  this->UpdatePiece                = -1;
  this->UpdateNumPieces            = -1;
  this->CollectiveIO               = 0;
  this->AppendMode                 = VTK_XDMF_APPEND_TEMPORAL;
  this->TimeStep                   = 0;
  this->TopologyConstant           = 0;
  this->GeometryConstant           = 0;
  this->DSMManager                 = NULL;
  this->TemporalCollection         = 1;

  this->BuildMode                  = VTK_XDMF_BUILD_ALL;
  this->DummyBuild                 = 0;

#ifdef VTK_USE_MPI
  this->Controller = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());
#endif

}
//----------------------------------------------------------------------------
vtkXdmfWriter3::~vtkXdmfWriter3()
{
#ifdef VTK_USE_MPI
  this->SetController(NULL);
#endif
  if (this->FileName)
    {
      delete []this->FileName;
      this->FileName = NULL;
    }
  if (this->DomainName)
    {
      delete []this->DomainName;
      this->DomainName = NULL;
    }
}
//----------------------------------------------------------------------------
int vtkXdmfWriter3::FillInputPortInformation(int port, vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataObject");
  return 1;
}
//----------------------------------------------------------------------------
void vtkXdmfWriter3::SetWriteModeToCollective()
{
  this->SetCollectiveIO(1);
}
//----------------------------------------------------------------------------
void vtkXdmfWriter3::SetWriteModeToIndependent()
{
  this->SetCollectiveIO(0);
}
//----------------------------------------------------------------------------
void vtkXdmfWriter3::SetAppendModeToOverwrite()
{
  this->SetAppendMode(VTK_XDMF_APPEND_OVERWRITE);
}
//----------------------------------------------------------------------------
void vtkXdmfWriter3::SetAppendModeToAppendMultiBlock()
{
  this->SetAppendMode(VTK_XDMF_APPEND_MULTIBLOCK);
}
//----------------------------------------------------------------------------
void vtkXdmfWriter3::SetAppendModeToAppendTemporal()
{
  this->SetAppendMode(VTK_XDMF_APPEND_TEMPORAL);
}
//----------------------------------------------------------------------------
void vtkXdmfWriter3::SetBuildModeToLight()
{
  this->SetBuildMode(VTK_XDMF_BUILD_LIGHT);
}
//----------------------------------------------------------------------------
void vtkXdmfWriter3::SetBuildModeToHeavy()
{
  this->SetBuildMode(VTK_XDMF_BUILD_HEAVY);
}
//----------------------------------------------------------------------------
void vtkXdmfWriter3::SetBuildModeToAll()
{
  this->SetBuildMode(VTK_XDMF_BUILD_ALL);
}
//----------------------------------------------------------------------------
XdmfDOM *vtkXdmfWriter3::ParseExistingFile(const char* filename)
{
  vtkXDRDebug("ParseExistingFile");

  if (!vtksys::SystemTools::FileExists(filename)) {
    vtkErrorMacro("Error opening file " << filename);
    return NULL;
  }
  //
  XdmfDOM *dom = new XdmfDOM();
  dom->SetWorkingDirectory(this->WorkingDirectory.c_str());
  dom->SetInputFileName(filename);
  dom->Parse(filename);
  return dom;
}
//----------------------------------------------------------------------------
XdmfXmlNode vtkXdmfWriter3::GetStaticGridNode(XdmfDOM *DOM, const char *path)
{
  vtkXDRDebug("GetStaticGridNode");

  if (!DOM) return NULL;
  XdmfXmlNode grid = DOM->FindElementByPath(path);
  return grid;
  // XdmfConstString(grid->children[0].content);
}
//----------------------------------------------------------------------------
// template <typename T> void vtkXW2_delete_object(T *p) { delete p; }
//----------------------------------------------------------------------------
XdmfDOM *vtkXdmfWriter3::BuildXdmfGrid(
    vtkDataSet *dataset, const char *name, double time, vtkXW2NodeHelp *staticnode)
{
  XdmfDOM        *DOM = new XdmfDOM();
  XdmfRoot        root;
  XdmfDomain      domain;
  XdmfGrid        grid;
  XdmfTopology   *topology;
  XdmfGeometry   *geometry;
  XdmfArray      *xdmfarray;
  //
  vtkXDRDebug("BuildXdmfGrid");
  // Debug DSM info
#ifdef VTK_USE_MPI
  if (this->DSMManager && this->DSMManager->GetDSMHandle()) {    
    XdmfDsmBuffer *dsmbuffer = this->DSMManager->GetDSMHandle();
    long long tlength, length, start, end;
    tlength = dsmbuffer->GetTotalLength();
    vtkXDRDebug("Total length of DSM: " << tlength);
    length = dsmbuffer->GetLength();
    vtkXDRDebug("Length of DSM per node: " << length);
    dsmbuffer->GetAddressRangeForId(this->UpdatePiece, &start, &end);
    vtkXDRDebug("DSM address range: " << start << "-->" << end);
  }
#endif
  DOM->SetWorkingDirectory(this->WorkingDirectory.c_str());
  domain.SetName(this->DomainName);
  //
  // File/DataGroup name
  //
  vtkstd::stringstream temp2;
  if (this->DSMManager) temp2 << "DSM:";
  temp2 << this->BaseFileName.c_str() << ".h5:/" <<
  setw(5) << setfill('0') << this->TimeStep << "/" << this->BlockNum << "/" << ends;
  vtkstd::string hdf5String = temp2.str();

  //
  // Initialization
  //
  root.SetDOM(DOM);
  domain.SetDOM(DOM);
  root.Build();
  root.Insert(&domain);
  root.Build();
  grid.SetName(name);
  grid.SetDOM(DOM);
  domain.Insert(&grid);
  //
  vtkIdType NumberOfPoints = dataset->GetNumberOfPoints();
  vtkIdType NumberOfCells  = dataset->GetNumberOfCells();
  vtkXDRDebug("Number of Points: " << NumberOfPoints << ", Number of Cells: " << NumberOfCells);
  //
  vtkUnstructuredGrid *ug = vtkUnstructuredGrid::SafeDownCast(dataset);

  vtkSmartPointer<vtkUnsignedCharArray>  celltypes = ug->GetCellTypesArray();
  vtkSmartPointer<vtkIdTypeArray>         celllocs = ug->GetCellLocationsArray();
  vtkSmartPointer<vtkCellArray>       connectivity = ug->GetCells();

  vtkSmartPointer<vtkIdTypeArray> connectivityXdmf = ConvertConnectivityArray(connectivity, celltypes, NumberOfCells);
  vtkXDRDebug("ConvertConnectivityArray done");

  //
  // Topology
  //
  topology = grid.GetTopology();
  if (!staticnode->DOM || !staticnode->node) {
    topology->SetTopologyType(XDMF_MIXED);
    topology->SetNumberOfElements(NumberOfCells);
    // Fortran is 1 based while c++ is 0 based
    topology->SetBaseOffset(0);
    // GetConnectivity() will create an XdmfArray.
    xdmfarray = topology->GetConnectivity();
    if (this->DSMManager) xdmfarray->SetDsmBuffer(this->DSMManager->GetDSMHandle());
    if (this->BuildMode == VTK_XDMF_BUILD_HEAVY || this->BuildMode == VTK_XDMF_BUILD_ALL) {
//      xdmfarray->SetBuildHeavy(XDMF_TRUE);
//      if (this->DummyBuild == 1)
//        xdmfarray->SetDummyArray(XDMF_TRUE);
    } else {
//      xdmfarray->SetBuildHeavy(XDMF_FALSE);
    }
    vtk2XdmfArray(xdmfarray, connectivityXdmf, hdf5String.c_str(), "Topology", "Connectivity");

  }
  else {
    XdmfXmlNode staticTopo = staticnode->DOM->FindElement("Topology", 0, staticnode->node);
    XdmfConstString text = staticnode->DOM->Serialize(staticTopo->children);
    topology->SetTopologyType(XDMF_MIXED);
    topology->SetNumberOfElements(NumberOfCells);
    topology->SetBaseOffset(0);
    topology->SetDataXml(text);
  }
  if (staticnode->staticFlag) {
    grid.Set("TopologyConstant", "True");
  }
  vtkXDRDebug("Topology write done");

  //
  // Geometry - points
  //
  geometry = grid.GetGeometry();
  if (!staticnode->DOM || !staticnode->node) {
    geometry->SetGeometryType(XDMF_GEOMETRY_XYZ);
    geometry->SetNumberOfPoints(NumberOfPoints);
    //
    xdmfarray = geometry->GetPoints();
    if (this->DSMManager) xdmfarray->SetDsmBuffer(this->DSMManager->GetDSMHandle());
    if (this->BuildMode == VTK_XDMF_BUILD_HEAVY || this->BuildMode == VTK_XDMF_BUILD_ALL) {
//       xdmfarray->SetBuildHeavy(XDMF_TRUE);
//       if (this->DummyBuild == 1)
//         xdmfarray->SetDummyArray(XDMF_TRUE);
     } else {
//       xdmfarray->SetBuildHeavy(XDMF_FALSE);
     }
    vtkDataArray *points = ug->GetPoints()->GetData();
    vtk2XdmfArray(xdmfarray, points, hdf5String.c_str(), "Geometry", "Points");
  }
  else {
    XdmfXmlNode staticGeom = staticnode->DOM->FindElement("Geometry", 0, staticnode->node);
    XdmfConstString text = staticnode->DOM->Serialize(staticGeom->children);
    geometry->SetGeometryType(XDMF_GEOMETRY_XYZ);
    geometry->SetNumberOfPoints(NumberOfPoints);
    geometry->SetDataXml(text);
  }
  if (staticnode->staticFlag) {
    grid.Set("GeometryConstant", "True");
  }

  //
  // Point Data : sort alphabetically to avoid potential bad ordering problems
  //
  vtkstd::vector<vtkstd::string> PointAttributeNames;
  for (int i=0; i<dataset->GetPointData()->GetNumberOfArrays(); i++) {
    vtkDataArray *scalars = dataset->GetPointData()->GetArray(i);
    PointAttributeNames.push_back(scalars->GetName());
  }
  vtkstd::sort(PointAttributeNames.begin(), PointAttributeNames.end());

  for (int i=0; i<dataset->GetPointData()->GetNumberOfArrays(); i++) {
    vtkDataArray *scalars = dataset->GetPointData()->GetArray(PointAttributeNames[i].c_str());
    XdmfAttribute *nodedata = new XdmfAttribute();
    nodedata->SetName(scalars->GetName());
    nodedata->SetAttributeCenter(XDMF_ATTRIBUTE_CENTER_NODE);
    //
    xdmfarray = nodedata->GetValues();
    if (this->DSMManager) xdmfarray->SetDsmBuffer(this->DSMManager->GetDSMHandle());
    if (this->BuildMode == VTK_XDMF_BUILD_HEAVY || this->BuildMode == VTK_XDMF_BUILD_ALL) {
//       xdmfarray->SetBuildHeavy(XDMF_TRUE);
//       if (this->DummyBuild == 1)
//         xdmfarray->SetDummyArray(XDMF_TRUE);
     } else {
//       xdmfarray->SetBuildHeavy(XDMF_FALSE);
     }
    vtk2XdmfArray(xdmfarray, scalars, hdf5String.c_str(), "PointData", NULL);

    // scalar type
    if (scalars->GetNumberOfComponents()==1) {
      nodedata->SetAttributeType(XDMF_ATTRIBUTE_TYPE_SCALAR);
    }
    else if (scalars->GetNumberOfComponents()==3) {
      nodedata->SetAttributeType(XDMF_ATTRIBUTE_TYPE_VECTOR);
    }
    else if (scalars->GetNumberOfComponents()==9) {
      nodedata->SetAttributeType(XDMF_ATTRIBUTE_TYPE_TENSOR);
    }
    grid.Insert(nodedata);
    nodedata->SetDeleteOnGridDelete(1);
  }

  //
  // Cell Data : sort alphabetically to avoid potential bad ordering problems
  //
  vtkstd::vector<vtkstd::string> CellAttributeNames;
  for (int i=0; i<dataset->GetPointData()->GetNumberOfArrays(); i++) {
    vtkDataArray *scalars = dataset->GetPointData()->GetArray(i);
    CellAttributeNames.push_back(scalars->GetName());
  }
  vtkstd::sort(CellAttributeNames.begin(), CellAttributeNames.end());

  for (int i=0; i<dataset->GetCellData()->GetNumberOfArrays(); i++) {
    XdmfAttribute *celldata = new XdmfAttribute();
    celldata->SetAttributeCenter(XDMF_ATTRIBUTE_CENTER_CELL);
    //
    xdmfarray = celldata->GetValues();
    if (this->DSMManager) xdmfarray->SetDsmBuffer(this->DSMManager->GetDSMHandle());
    if (this->BuildMode == VTK_XDMF_BUILD_HEAVY || this->BuildMode == VTK_XDMF_BUILD_ALL) {
//       xdmfarray->SetBuildHeavy(XDMF_TRUE);
//       if (this->DummyBuild == 1)
//         xdmfarray->SetDummyArray(XDMF_TRUE);
     } else {
//       xdmfarray->SetBuildHeavy(XDMF_FALSE);
     }
    vtkDataArray *scalars = dataset->GetCellData()->GetArray(CellAttributeNames[i].c_str());
    vtk2XdmfArray(xdmfarray, scalars, hdf5String.c_str(), "CellData", NULL);
    celldata->SetName(scalars->GetName());
    // scalar type
    if (scalars->GetNumberOfComponents()==1) {
      celldata->SetAttributeType(XDMF_ATTRIBUTE_TYPE_SCALAR);
    }
    else if (scalars->GetNumberOfComponents()==3) {
      celldata->SetAttributeType(XDMF_ATTRIBUTE_TYPE_VECTOR);
    }
    else if (scalars->GetNumberOfComponents()==9) {
      celldata->SetAttributeType(XDMF_ATTRIBUTE_TYPE_TENSOR);
    }
    grid.Insert(celldata);
    celldata->SetDeleteOnGridDelete(1);
  }

  // Build is recursive ... it will be called on all of the child nodes.
  // This updates the DOM and writes the HDF5
  if (grid.Build()!=XDMF_SUCCESS) {
    vtkErrorMacro("Xdmf Grid Build failed");
    return NULL;
  }
  vtkXDRDebug("Xdmf Grid Build succeeded");
  //
  return DOM;
}
//----------------------------------------------------------------------------
void vtkXdmfWriter3::BuildHeavyXdmfGrid(vtkMultiBlockDataSet *mbdataset, int data_block, int nb_arrays)
{
  //
  vtkXDRDebug("BuildHeavyXdmfGrid");
  int nb_blocks = mbdataset->GetNumberOfBlocks();

  vtkSmartPointer<vtkCompositeDataIterator> iter;
  iter.TakeReference(mbdataset->NewIterator());
  iter->SkipEmptyNodesOff();
  this->BlockNum = 0;

  hid_t fd;
  XdmfArray *xdmfarray = new XdmfArray[nb_arrays*nb_blocks];
  XdmfHDF      *xdmfH5 = new XdmfHDF[nb_arrays*nb_blocks];

  for (iter->InitTraversal(); !iter->IsDoneWithTraversal(); iter->GoToNextItem(), this->BlockNum++)
  {
    vtkXDRDebug("Loop " << this->BlockNum);
    vtkDataSet *dataset = vtkUnstructuredGrid::SafeDownCast(iter->GetCurrentDataObject());
    if (dataset) {
      vtkXDRDebug("I got the block number: " << this->BlockNum);
      this->DummyBuildOff();
    } else {
      vtkXDRDebug("Dummy sync " << this->BlockNum);
      dataset = vtkDataSet::SafeDownCast(mbdataset->GetBlock(data_block));
      this->DummyBuildOn();
    }

    this->SetBuildModeToHeavy();

    // Debug DSM info
#ifdef VTK_USE_MPI
    if (this->DSMManager && this->DSMManager->GetDSMHandle()) {
      XdmfDsmBuffer *dsmbuffer = this->DSMManager->GetDSMHandle();
      long long tlength, length, start, end;
      tlength = dsmbuffer->GetTotalLength();
      vtkXDRDebug("Total length of DSM: " << tlength);
      length = dsmbuffer->GetLength();
      vtkXDRDebug("Length of DSM per node: " << length);
      dsmbuffer->GetAddressRangeForId(this->UpdatePiece, &start, &end);
      vtkXDRDebug("DSM address range: " << start << "-->" << end);
    }
#endif

    //
    // File/DataGroup name
    //
    vtkstd::stringstream temp2;
    if (this->DSMManager) temp2 << "DSM:";
    temp2 << this->BaseFileName.c_str() << ".h5:/" <<
    setw(5) << setfill('0') << this->TimeStep << "/" << this->BlockNum << "/" << ends;
    vtkstd::string hdf5String = temp2.str();

    //
    // Initialization
    //
    vtkIdType NumberOfPoints = dataset->GetNumberOfPoints();
    vtkIdType NumberOfCells  = dataset->GetNumberOfCells();
    vtkXDRDebug("Number of Points: " << NumberOfPoints << ", Number of Cells: " << NumberOfCells);
    //
    vtkUnstructuredGrid *ug = vtkUnstructuredGrid::SafeDownCast(dataset);

    vtkSmartPointer<vtkUnsignedCharArray>  celltypes = ug->GetCellTypesArray();
    vtkSmartPointer<vtkIdTypeArray>         celllocs = ug->GetCellLocationsArray();
    vtkSmartPointer<vtkCellArray>       connectivity = ug->GetCells();

    vtkSmartPointer<vtkIdTypeArray> connectivityXdmf = ConvertConnectivityArray(connectivity, celltypes, NumberOfCells);
    vtkXDRDebug("ConvertConnectivityArray done");

    //
    // Topology
    //
    int index_topo = this->BlockNum*nb_arrays;
    if (this->DSMManager) xdmfarray[index_topo].SetDsmBuffer(this->DSMManager->GetDSMHandle());
    if (this->DSMManager) xdmfH5[index_topo].SetDsmBuffer(this->DSMManager->GetDSMHandle());
    if (this->BuildMode == VTK_XDMF_BUILD_HEAVY || this->BuildMode == VTK_XDMF_BUILD_ALL) {
//      xdmfarray[index_topo].SetBuildHeavy(XDMF_TRUE);
      if (this->DummyBuild == 1) {
//        xdmfarray[index_topo].SetDummyArray(XDMF_TRUE);
        vtkXDRDebug("Dummy Array set");
      }
    }
    vtk2XdmfArray(&xdmfarray[index_topo], connectivityXdmf, hdf5String.c_str(), "Topology", "Connectivity");
    xdmfH5[index_topo].CopyType( &xdmfarray[index_topo] );
    xdmfH5[index_topo].CopyShape( &xdmfarray[index_topo] );
    vtkXDRDebug("Opening..." << xdmfarray[index_topo].GetHeavyDataSetName());
    
    if (index_topo == 0) {
//      xdmfH5[index_topo].OpenBlock( xdmfarray[index_topo].GetHeavyDataSetName(), "rw" );
//      fd = xdmfH5[index_topo].GetFile();
    } else {
//      xdmfH5[index_topo].SetFile(fd);
//      xdmfH5[index_topo].OpenBlock( xdmfarray[index_topo].GetHeavyDataSetName(), "rw" );
    }

    //
    // Geometry - points
    //
    int index_geom = this->BlockNum*nb_arrays + 1;
    if (this->DSMManager) xdmfarray[index_geom].SetDsmBuffer(this->DSMManager->GetDSMHandle());
    if (this->DSMManager) xdmfH5[index_geom].SetDsmBuffer(this->DSMManager->GetDSMHandle());
    if (this->BuildMode == VTK_XDMF_BUILD_HEAVY || this->BuildMode == VTK_XDMF_BUILD_ALL) {
//      xdmfarray[index_geom].SetBuildHeavy(XDMF_TRUE);
//      if (this->DummyBuild == 1)
//        xdmfarray[index_geom].SetDummyArray(XDMF_TRUE);
    }
    vtkDataArray *points = ug->GetPoints()->GetData();
    vtk2XdmfArray(&xdmfarray[index_geom], points, hdf5String.c_str(), "Geometry", "Points");
    xdmfH5[index_geom].CopyType( &xdmfarray[index_geom] );
    xdmfH5[index_geom].CopyShape( &xdmfarray[index_geom] );
    vtkXDRDebug("Opening..." << xdmfarray[index_geom].GetHeavyDataSetName());
//    xdmfH5[index_geom].SetFile(fd);
//    xdmfH5[index_geom].OpenBlock( xdmfarray[index_geom].GetHeavyDataSetName(), "rw" );

    //
    // Point Data : sort alphabetically to avoid potential bad ordering problems
    //
    vtkstd::vector<vtkstd::string> PointAttributeNames;
    for (int i=0; i<dataset->GetPointData()->GetNumberOfArrays(); i++) {
      vtkDataArray *scalars = dataset->GetPointData()->GetArray(i);
      PointAttributeNames.push_back(scalars->GetName());
    }
    vtkstd::sort(PointAttributeNames.begin(), PointAttributeNames.end());

    for (int i=0; i<dataset->GetPointData()->GetNumberOfArrays(); i++) {
      int index_point = this->BlockNum*nb_arrays + 2 + i;
      vtkDataArray *scalars = dataset->GetPointData()->GetArray(PointAttributeNames[i].c_str());
      if (this->DSMManager) xdmfarray[index_point].SetDsmBuffer(this->DSMManager->GetDSMHandle());
      if (this->DSMManager) xdmfH5[index_point].SetDsmBuffer(this->DSMManager->GetDSMHandle());
      if (this->BuildMode == VTK_XDMF_BUILD_HEAVY || this->BuildMode == VTK_XDMF_BUILD_ALL) {
//        xdmfarray[index_point].SetBuildHeavy(XDMF_TRUE);
//        if (this->DummyBuild == 1)
//          xdmfarray[index_point].SetDummyArray(XDMF_TRUE);
      }
      vtk2XdmfArray(&xdmfarray[index_point], scalars, hdf5String.c_str(), "PointData", NULL);
      xdmfH5[index_point].CopyType( &xdmfarray[index_point] );
      xdmfH5[index_point].CopyShape( &xdmfarray[index_point] );
      vtkXDRDebug("Opening..." << xdmfarray[index_point].GetHeavyDataSetName());
//      xdmfH5[index_point].SetFile(fd);
//      xdmfH5[index_point].OpenBlock( xdmfarray[index_point].GetHeavyDataSetName(), "rw" );
    }

    //
    // Cell Data : sort alphabetically to avoid potential bad ordering problems
    //
    vtkstd::vector<vtkstd::string> CellAttributeNames;
    for (int i=0; i<dataset->GetPointData()->GetNumberOfArrays(); i++) {
      vtkDataArray *scalars = dataset->GetPointData()->GetArray(i);
      CellAttributeNames.push_back(scalars->GetName());
    }
    vtkstd::sort(CellAttributeNames.begin(), CellAttributeNames.end());

    for (int i=0; i<dataset->GetCellData()->GetNumberOfArrays(); i++) {
      int index_cell = (this->BlockNum + 1)*nb_arrays - dataset->GetCellData()->GetNumberOfArrays() + i;
      if (this->DSMManager) xdmfarray[index_cell].SetDsmBuffer(this->DSMManager->GetDSMHandle());
      if (this->DSMManager) xdmfH5[index_cell].SetDsmBuffer(this->DSMManager->GetDSMHandle());
      if (this->BuildMode == VTK_XDMF_BUILD_HEAVY || this->BuildMode == VTK_XDMF_BUILD_ALL) {
//        xdmfarray[index_cell].SetBuildHeavy(XDMF_TRUE);
//        if (this->DummyBuild == 1)
//          xdmfarray[index_cell].SetDummyArray(XDMF_TRUE);
      }
      vtkDataArray *scalars = dataset->GetCellData()->GetArray(CellAttributeNames[i].c_str());
      vtk2XdmfArray(&xdmfarray[index_cell], scalars, hdf5String.c_str(), "CellData", NULL);
      xdmfH5[index_cell].CopyType( &xdmfarray[index_cell] );
      xdmfH5[index_cell].CopyShape( &xdmfarray[index_cell] );
//      xdmfH5[index_cell].SetFile(fd);
//      xdmfH5[index_cell].OpenBlock( xdmfarray[index_cell].GetHeavyDataSetName(), "rw" );
    }
  }

  this->Controller->Barrier();

  for (int block=0; block<nb_blocks; block++) {
    for (int i=0 ; i<nb_arrays ; i++) {
      int index = block*nb_arrays + i;
//      if (xdmfarray[index].GetDummyArray() == XDMF_FALSE)
        vtkXDRDebug("Writing..." << xdmfarray[index].GetHeavyDataSetName());
//      xdmfH5[index].WriteBlock( &xdmfarray[index] );
    }
  }

  this->Controller->Barrier();

  for (int block=0; block<nb_blocks; block++) {
    for (int i=0 ; i<nb_arrays ; i++) {
      int index = block*nb_arrays + i;
      vtkXDRDebug("Closing..." << xdmfarray[index].GetHeavyDataSetName());
//      xdmfH5[index].CloseBlock();
    }
  }

//  xdmfH5[0].CloseBlockFileOnly();

  delete[] xdmfarray;
  delete[] xdmfH5;
  vtkXDRDebug("Heavy Build succeeded");
}
//----------------------------------------------------------------------------
XdmfDOM *vtkXdmfWriter3::CreateEmptyCollection(const char *name, const char *ctype)
{
  vtkXDRDebug("CreateEmptyCollection");

  XdmfDOM *DOM = new XdmfDOM();
  DOM->SetWorkingDirectory(this->WorkingDirectory.c_str());
  //
  XdmfRoot root;
  root.SetDOM(DOM);
  root.Build();
  XdmfDomain domain;
  domain.SetName(this->DomainName);
  root.Insert(&domain);
  root.Build();
  //
  if (name != NULL && ctype != NULL) {
    XdmfXmlNode  domain1 = DOM->FindElement("Domain");
    XdmfXmlNode gridNode = DOM->InsertFromString(domain1, "<Grid />");
    //
    XdmfGrid    grid;
    grid.SetDOM(DOM);
    grid.SetElement(gridNode);
    grid.Set("Name", name);
    grid.Set("GridType", "Collection");
    grid.Set("CollectionType", ctype);
  }
  return DOM;
}
//----------------------------------------------------------------------------
XdmfDOM *vtkXdmfWriter3::AddGridToCollection(XdmfDOM *cDOM, XdmfDOM *block)
{
  vtkXDRDebug("AddGridToCollection");

  cDOM->SetWorkingDirectory(this->WorkingDirectory.c_str());
  //
  XdmfXmlNode     root = cDOM->GetRoot();
  XdmfXmlNode   domain = cDOM->FindElement("Domain");
  XdmfXmlNode gridNode = cDOM->FindElement("Grid", 0, domain);
  //
  XdmfXmlNode   domain2 = block->FindElement("Domain");
  XdmfXmlNode gridNode2 = block->FindElement("Grid", 0, domain2);
  //

  if (gridNode && gridNode2)// Grid collection
    cDOM->Insert(gridNode, gridNode2);
  else
    if (domain)// No collection
      cDOM->Insert(domain, gridNode2);
    else
      vtkErrorMacro("Could not add node to DOM as grid nodes were not found");

  return cDOM;
}
//----------------------------------------------------------------------------
vtkstd::string vtkXdmfWriter3::MakeGridName(vtkDataSet *dataset, const char *name)
{
  vtkXDRDebug("MakeGridName");

  vtkstd::stringstream temp1, temp2;
  if (!name)
    temp1 << dataset->GetClassName() << "_" << this->BlockNum << vtkstd::ends;

  vtkstd::string gridname = name ? name : temp1.str().c_str();

  return gridname;
}
//----------------------------------------------------------------------------
void vtkXdmfWriter3::WriteOutputXML(XdmfDOM *outputDOM, XdmfDOM *timestep, double time)
{
  vtkXDRDebug("WriteOutputXML");

  vtkstd::stringstream temp;
  temp << time;
  XdmfXmlNode   domain = timestep->FindElement("Domain");
  XdmfXmlNode     grid = timestep->FindElement("Grid", 0, domain);

  if (this->TemporalCollection != 0) {// Insert time info
    XdmfXmlNode timeNode = timestep->InsertFromString(grid, "<Time />");
    timestep->Set(timeNode, "TimeType", "Single");
    timestep->Set(timeNode, "Value", temp.str().c_str());
    if (!outputDOM)
      outputDOM = this->CreateEmptyCollection("TimeSeries", "Temporal");
  }
  else
    if (!outputDOM)
      outputDOM = this->CreateEmptyCollection(NULL, NULL);

  // Using parallel mode, get XML blocks from each process and gather on node 0
#ifdef VTK_USE_MPI
  if (this->Controller && (this->UpdateNumPieces > 1)) {
    vtkXDRDebug("Gathering XML blocks");
    if (this->UpdatePiece != 0) {// Slave

      int nb_blocks = timestep->GetNumberOfChildren(grid);// If more than one block per process
      if (this->TemporalCollection != 0) nb_blocks--;// Only need the time value from the master
      this->Controller->Send(&nb_blocks, 1, 0, 0);
      XdmfXmlNode sub_grid = grid->children;
      for (int i=0 ; i<nb_blocks ; i++) {// Can only insert block by block
        XdmfConstString text = timestep->Serialize(sub_grid);
        int text_len = strlen(text);
        this->Controller->Send(&text_len, 1, 0, 1);
        this->Controller->Send(text, text_len+1, 0, 2);
        sub_grid = sub_grid->next;
      }
    }
    else {// Master

      int recv_len, recv_nb_blocks;
      char *recv;
      XdmfConstString text = timestep->Serialize(domain->children);
      int text_len = strlen(text);
      XdmfXmlNode out_domain = outputDOM->FindElement("Domain");
      XdmfXmlNode out_space_grid;
      if (this->TemporalCollection != 0) {
        XdmfXmlNode out_time_grid = outputDOM->FindElement("Grid", 0, out_domain);
        outputDOM->InsertFromString(out_time_grid, text);
        out_space_grid = outputDOM->FindElement("Grid", 0, out_time_grid);
      }
      else {
        outputDOM->InsertFromString(out_domain, text);
        out_space_grid = outputDOM->FindElement("Grid", 0, out_domain);
      }
      for (int i = 1; i < this->UpdateNumPieces; i++) {
        this->Controller->Receive(&recv_nb_blocks, 1, i, 0);
        for (int j=0 ; j<recv_nb_blocks; j++) {
          this->Controller->Receive(&recv_len, 1, i, 1);
          recv = new char[recv_len+1];
          this->Controller->Receive(recv, recv_len+1, i, 2);
          recv[recv_len+0] = 0;
          outputDOM->InsertFromString(out_space_grid, recv);
          delete[] recv;
        }
      }
      outputDOM->Write(this->XdmfFileName.c_str());
    }
  }
  else {// If not parallel
#endif
    this->AddGridToCollection(outputDOM, timestep);
    outputDOM->Write(this->XdmfFileName.c_str());
#ifdef VTK_USE_MPI
  }
#endif
}
//----------------------------------------------------------------------------
void vtkXdmfWriter3::WriteData()
{
  vtkXDRDebug("WriteData");

#ifdef VTK_USE_MPI
  if (this->Controller) {
    this->UpdatePiece = this->Controller->GetLocalProcessId();
    this->UpdateNumPieces = this->Controller->GetNumberOfProcesses();
  }
  else {
    this->UpdatePiece = 0;
    this->UpdateNumPieces = 1;
  }
#else
  this->UpdatePiece = 0;
  this->UpdateNumPieces = 1;
#endif
  //
  vtkMultiBlockDataSet *mbinput = vtkMultiBlockDataSet::SafeDownCast(this->GetInput());
  vtkDataSet           *dsinput = vtkUnstructuredGrid::SafeDownCast(this->GetInput());
  //
  this->WorkingDirectory = vtksys::SystemTools::GetFilenamePath(this->FileName);
  this->BaseFileName     = vtksys::SystemTools::GetFilenameName(this->FileName);
  this->XdmfFileName     = this->WorkingDirectory + "/" + this->BaseFileName + ".xmf";

  //
  // Adding to an existing file?
  //
  const char *XpathPrefix = "/Xdmf/Domain/Grid[@CollectionType=\"Temporal\"]";
  XdmfDOM     *outputDOM = NULL;
  XdmfXmlNode staticnode = NULL;
  bool fileexists = vtksys::SystemTools::FileExists(this->XdmfFileName.c_str());
  if (fileexists && this->AppendMode != VTK_XDMF_APPEND_OVERWRITE) {
    outputDOM = this->ParseExistingFile(this->XdmfFileName.c_str());
  }
  //
  double current_time = this->TimeStep;
  XdmfDOM *timeStepDOM = NULL;
  int data_block;

  if (dsinput) {
    vtkXDRDebug("DataSet input")
    if (this->UpdateNumPieces > 1) {
      // create multiblock
      vtkXDRDebug("create mbdataset")
      mbinput = vtkMultiBlockDataSet::New();
      mbinput->SetNumberOfBlocks(this->UpdateNumPieces);
      for (int i = 0; i < this->UpdateNumPieces; i++) {
        if (i == this->UpdatePiece)
          mbinput->SetBlock(i, dsinput);
        else
          mbinput->SetBlock(i, NULL);
      }
    } else {
      if (dsinput->GetInformation()->Has(vtkDataObject::DATA_TIME_STEPS()))
        current_time = dsinput->GetInformation()->Get(vtkDataObject::DATA_TIME_STEPS())[0];
      vtkXDRDebug("Current Time: " << current_time);

      this->BlockNum = 0;
      bool StaticFlag = false;
      vtkstd::string name = this->MakeGridName(dsinput, NULL);
      if (dsinput->GetInformation()->Has(vtkDataObject::DATA_GEOMETRY_UNMODIFIED()) || this->TopologyConstant)
        {
        // if re-using static topology/geometry, then find the first dataset ...
        vtkstd::stringstream staticXPath;
        staticXPath << XpathPrefix << "/Grid[@Name=\"" << name.c_str() << "\"][1]";
        staticnode = this->GetStaticGridNode(outputDOM, staticXPath.str().c_str());
        StaticFlag = true;
        }
      vtkXW2NodeHelp helper(outputDOM, staticnode, StaticFlag);
      this->SetBuildModeToAll();
      timeStepDOM = this->BuildXdmfGrid(dsinput, name.c_str(), 0.0, &helper);
    }
  }
  if (mbinput) {
    vtkXDRDebug("MBDataSet input")
    if (mbinput->GetInformation()->Has(vtkDataObject::DATA_TIME_STEPS()))
      current_time = mbinput->GetInformation()->Get(vtkDataObject::DATA_TIME_STEPS())[0];
    vtkXDRDebug("Current Time: " << current_time);

    vtkSmartPointer<vtkCompositeDataIterator> iter;
    iter.TakeReference(mbinput->NewIterator());
    iter->SkipEmptyNodesOff();

    this->BlockNum = 0;

    timeStepDOM = this->CreateEmptyCollection("vtkMultiBlock", "Spatial");

#ifdef JB_DEBUG__
    //    OUTPUTTEXT("P(" << this->UpdatePiece << ") | ")
    for (int i=0 ; i<this->UpdateNumPieces ; i++) {
      if (mbinput->GetBlock(i) != NULL) {
        OUTPUTTEXT("BLOCK | ")
      }
      else {
        OUTPUTTEXT("NULL | ")
      }
    }
#endif

    for (iter->InitTraversal(); !iter->IsDoneWithTraversal(); iter->GoToNextItem(), this->BlockNum++)
      {
      vtkXDRDebug("Loop " << this->BlockNum);
      const char *dsname = mbinput->GetMetaData(iter)->Get(vtkCompositeDataSet::NAME());
      dsinput = vtkUnstructuredGrid::SafeDownCast(iter->GetCurrentDataObject());
      bool StaticFlag = false;
      if (dsinput) {
        vtkXDRDebug("I got the block number: " << this->BlockNum);
        data_block = this->BlockNum;
        vtkstd::string name = MakeGridName(dsinput, dsname);
        if (dsinput->GetInformation()->Has(vtkDataObject::DATA_GEOMETRY_UNMODIFIED())
            || this->TopologyConstant)
          {
          vtkstd::stringstream staticXPath;
          staticXPath << XpathPrefix << "/Grid[@Name=\"vtkMultiBlock\"][1]" << "/Grid[@Name=\"" << name.c_str() << "\"]";
          staticnode = this->GetStaticGridNode(outputDOM, staticXPath.str().c_str());
          XdmfXmlNode staticnode = this->GetStaticGridNode(outputDOM, staticXPath.str().c_str());
          StaticFlag = true;
          }
        vtkXW2NodeHelp helper(outputDOM, staticnode, StaticFlag);
        this->SetBuildModeToLight();
        XdmfDOM *block = this->BuildXdmfGrid(dsinput, name.c_str(), 0.0, &helper);
        this->AddGridToCollection(timeStepDOM, block);
        delete block;
      }
      }

  }
  if (timeStepDOM)
    this->WriteOutputXML(outputDOM, timeStepDOM, current_time);

#ifdef VTK_USE_MPI
  if (this->Controller)
    this->Controller->Barrier();
#endif
  if (mbinput) {
    vtkXDRDebug("**********Heavy Building************");
    XdmfXmlNode   domain = timeStepDOM->FindElement("Domain");
    XdmfXmlNode     grid = timeStepDOM->FindElement("Grid", 0, domain);
    XdmfXmlNode     grid_block = timeStepDOM->FindElement("Grid", 0, grid);
    int nb_arrays = timeStepDOM->GetNumberOfChildren(grid_block);
    this->BuildHeavyXdmfGrid(mbinput, data_block, nb_arrays);
  }

  if (timeStepDOM)
    delete timeStepDOM;
  delete outputDOM;
  this->TimeStep++;
}
//----------------------------------------------------------------------------
void vtkXdmfWriter3::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

