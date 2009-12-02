/*=========================================================================

  Project                 : vtkCSCS
  Module                  : vtkXdmfWriter4.h
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
#include "vtkXdmfWriter4.h"
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
#include "XdmfH5MBCallback.h"
//
//----------------------------------------------------------------------------
vtkCxxRevisionMacro(vtkXdmfWriter4, "$Revision: 598 $");
vtkStandardNewMacro(vtkXdmfWriter4);
#ifdef VTK_USE_MPI
vtkCxxSetObjectMacro(vtkXdmfWriter4, Controller, vtkMultiProcessController);
vtkCxxSetObjectMacro(vtkXdmfWriter4, DSMManager, vtkDSMManager);
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
vtkXdmfWriter4::vtkXdmfWriter4()
{
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(0);
  //
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
  this->Callback                   = NULL;

#ifdef VTK_USE_MPI
  this->Controller = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());
#endif
  this->LightDataLimit = 0;
}
//----------------------------------------------------------------------------
vtkXdmfWriter4::~vtkXdmfWriter4()
{
#ifdef VTK_USE_MPI
  this->SetController(NULL);
#endif
  if (this->DomainName)
    {
      delete []this->DomainName;
      this->DomainName = NULL;
    }

  if (this->Callback) {
    this->Callback->CloseTree();
  }

}
//----------------------------------------------------------------------------
int vtkXdmfWriter4::FillInputPortInformation(int port, vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataObject");
  return 1;
}
//----------------------------------------------------------------------------
void vtkXdmfWriter4::SetWriteModeToCollective()
{
  this->SetCollectiveIO(1);
}
//----------------------------------------------------------------------------
void vtkXdmfWriter4::SetWriteModeToIndependent()
{
  this->SetCollectiveIO(0);
}
//----------------------------------------------------------------------------
void vtkXdmfWriter4::SetAppendModeToOverwrite()
{
  this->SetAppendMode(VTK_XDMF_APPEND_OVERWRITE);
}
//----------------------------------------------------------------------------
void vtkXdmfWriter4::SetAppendModeToAppendMultiBlock()
{
  this->SetAppendMode(VTK_XDMF_APPEND_MULTIBLOCK);
}
//----------------------------------------------------------------------------
void vtkXdmfWriter4::SetAppendModeToAppendTemporal()
{
  this->SetAppendMode(VTK_XDMF_APPEND_TEMPORAL);
}
//----------------------------------------------------------------------------
void vtkXdmfWriter4::SetBuildModeToLight()
{
  this->SetBuildMode(VTK_XDMF_BUILD_LIGHT);
}
//----------------------------------------------------------------------------
void vtkXdmfWriter4::SetBuildModeToHeavy()
{
  this->SetBuildMode(VTK_XDMF_BUILD_HEAVY);
}
//----------------------------------------------------------------------------
void vtkXdmfWriter4::SetBuildModeToAll()
{
  this->SetBuildMode(VTK_XDMF_BUILD_ALL);
}
//----------------------------------------------------------------------------
XdmfDOM *vtkXdmfWriter4::ParseExistingFile(const char* filename)
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
XdmfXmlNode vtkXdmfWriter4::GetStaticGridNode(vtkDataSet *dsinput, bool multiblock, XdmfDOM *DOM, const char *name, bool &staticFlag)
{
  vtkXDRDebug("GetStaticGridNode");
  staticFlag = false;
  if (!DOM) return NULL;
  XdmfXmlNode grid = NULL;
  //
  if (dsinput->GetInformation()->Has(vtkDataObject::DATA_GEOMETRY_UNMODIFIED()) || this->TopologyConstant)
  {
    staticFlag = true;
    // if re-using static topology/geometry, then find the first dataset ...
    vtkstd::stringstream staticXPath;
    const char *XpathPrefix = "/Xdmf/Domain/Grid[@CollectionType=\"Temporal\"]";
    if (multiblock) {
      staticXPath << XpathPrefix << "/Grid[@Name=\"vtkMultiBlock\"][1]" << "/Grid[@Name=\"" << name << "\"]";
    }
    else {
      staticXPath << XpathPrefix << "/Grid[@Name=\"" << name << "\"][1]";
    }
    staticFlag = true;
    grid = DOM->FindElementByPath(staticXPath.str().c_str());
  }
  return grid;
}
//----------------------------------------------------------------------------
XdmfDOM *vtkXdmfWriter4::CreateXdmfGrid(
    vtkDataSet *dataset, const char *name, int index, double time, vtkXW2NodeHelp *staticnode)
{
  XdmfDOM        *DOM = new XdmfDOM();
  XdmfRoot        root;
  XdmfDomain      domain;
  XdmfGrid        grid;
  //
  vtkXDRDebug("CreateXdmfGrid");
  // Debug DSM info
  DOM->SetWorkingDirectory(this->WorkingDirectory.c_str());
  domain.SetName(this->DomainName);

  //
  // Initialization
  //
  root.SetDOM(DOM);
  root.Build();
  root.Insert(&domain);
  grid.SetName(name);
  domain.Insert(&grid);

  //
  // File/DataGroup name
  //
  vtkstd::string hdf5name = this->BaseFileName + ".h5";
  vtkstd::stringstream hdf5group;
  hdf5group << "/" << setw(5) << setfill('0') << this->TimeStep << "/Process_" << this->UpdatePiece;
  if (index>=0) {
    hdf5group << "/Block_" << index;
  }
  hdf5group << ends;
  if (this->DSMManager) {
    hdf5name = "DSM:" + hdf5name;
  }
  this->SetHeavyDataFileName(hdf5name.c_str());
  this->SetHeavyDataGroupName(hdf5group.str().c_str());

  //
  // Attributes
  //
  vtkIdType FRank = 1;
  vtkIdType FDims[1] = {dataset->GetFieldData()->GetNumberOfTuples() };
  vtkIdType CRank = 3;
  vtkIdType CDims[3];
  vtkIdType PRank = 3;
  vtkIdType PDims[3];

  this->vtkXdmfWriter2::CreateTopology(dataset, &grid, PDims, CDims, PRank, CRank, staticnode);
  this->vtkXdmfWriter2::CreateGeometry(dataset, &grid, staticnode);

  this->WriteArrays(dataset->GetFieldData(), &grid,XDMF_ATTRIBUTE_CENTER_GRID, FRank, FDims, "Field");
  this->WriteArrays(dataset->GetCellData(),  &grid,XDMF_ATTRIBUTE_CENTER_CELL, CRank, CDims, "Cell");
  this->WriteArrays(dataset->GetPointData(), &grid,XDMF_ATTRIBUTE_CENTER_NODE, PRank, PDims, "Node");

  // Build is recursive ... it will be called on all of the child nodes.
  // This updates the DOM and writes the HDF5
  if (grid.Build()!=XDMF_SUCCESS) {
    vtkErrorMacro("Xdmf Grid Build failed");
    return NULL;
  }
  vtkXDRDebug("Xdmf Grid Create succeeded");

  //
  return DOM;
}
//----------------------------------------------------------------------------
XdmfDOM *vtkXdmfWriter4::CreateEmptyCollection(const char *name, const char *ctype)
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
XdmfDOM *vtkXdmfWriter4::AddGridToCollection(XdmfDOM *cDOM, XdmfDOM *block)
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

  if (gridNode && gridNode2) { // Grid collection
    cDOM->Insert(gridNode, gridNode2);
  }
  else {
    if (domain) { // No collection
      cDOM->Insert(domain, gridNode2);
    }
    else {
      vtkErrorMacro("Could not add node to DOM as grid nodes were not found");
    }
  }

  return cDOM;
}
//----------------------------------------------------------------------------
vtkstd::string vtkXdmfWriter4::MakeGridName(vtkDataSet *dataset, const char *name, int index)
{
  vtkXDRDebug("MakeGridName");
  vtkstd::stringstream temp1, temp2;
  if (!name) {
    temp1 << dataset->GetClassName();
  }
  if (this->UpdateNumPieces>1) {
    temp1 << "_P_" << this->UpdatePiece;
  }
  if (index>=0) {
    temp1 << "_B_" << index;
  }
  temp1 << vtkstd::ends;
  vtkstd::string gridname = name ? name : temp1.str().c_str();
  return gridname;
}
//----------------------------------------------------------------------------
void vtkXdmfWriter4::WriteOutputXML(XdmfDOM *outputDOM, XdmfDOM *timestep, double time)
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
    if (!outputDOM) {
      outputDOM = this->CreateEmptyCollection("TimeSeries", "Temporal");
    }
  }
  else {
    if (!outputDOM) {
      outputDOM = this->CreateEmptyCollection(NULL, NULL);
    }
  }

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
        int text_len = (int)strlen(text);
        this->Controller->Send(&text_len, 1, 0, 1);
        this->Controller->Send(text, text_len+1, 0, 2);
        sub_grid = sub_grid->next;
      }
    }
    else {// Master

      int recv_len, recv_nb_blocks;
      char *recv;
      XdmfConstString text = timestep->Serialize(domain->children);
      int text_len = (int)strlen(text);
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
void vtkXdmfWriter4::SetXdmfArrayCallbacks(XdmfArray *data)
{
  data->setOpenCallback( this->Callback );
//  data->setReadCallback( this->Callback );
  data->setWriteCallback( this->Callback );
  data->setCloseCallback( this->Callback );
}
//----------------------------------------------------------------------------
int vtkXdmfWriter4::RequestData(
  vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* vtkNotUsed(outputVector))
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
  vtkDataObject        *doinput = this->GetInput();
  vtkMultiBlockDataSet *mbinput = vtkMultiBlockDataSet::SafeDownCast(this->GetInput());
  vtkDataSet           *dsinput = vtkDataSet::SafeDownCast(this->GetInput());
  //
  this->WorkingDirectory = vtksys::SystemTools::GetFilenamePath(this->FileName);
  this->BaseFileName     = vtksys::SystemTools::GetFilenameName(this->FileName);
  this->XdmfFileName     = this->WorkingDirectory + "/" + this->BaseFileName + ".xmf";

  //
  // Get the raw MPI_Comm handle
  //
  vtkMPICommunicator *communicator = vtkMPICommunicator::SafeDownCast(this->Controller->GetCommunicator());
  MPI_Comm mpiComm = *communicator->GetMPIComm()->GetHandle();
  //

  if (!this->Callback) {
    this->Callback = new H5MBCallback(mpiComm);
    this->Callback->SetDSMManager(this->DSMManager);
  }

  //
  // Adding to an existing file?
  //
  XdmfDOM     *outputDOM = NULL;
  XdmfXmlNode staticnode = NULL;
  bool fileexists = vtksys::SystemTools::FileExists(this->XdmfFileName.c_str());
  if (fileexists && this->AppendMode != VTK_XDMF_APPEND_OVERWRITE) {
    outputDOM = this->ParseExistingFile(this->XdmfFileName.c_str());
  }
  //
  double current_time = this->TimeStep;
  XdmfDOM *timeStepDOM = NULL;
  //  
  if (doinput->GetInformation()->Has(vtkDataObject::DATA_TIME_STEPS())) {
    current_time = doinput->GetInformation()->Get(vtkDataObject::DATA_TIME_STEPS())[0];
    vtkXDRDebug("Current Time: " << current_time);
  }
                  
  if (dsinput) {
    bool StaticFlag = false;
    vtkstd::string name = this->MakeGridName(dsinput, NULL, -1);
    staticnode = this->GetStaticGridNode(dsinput, false, outputDOM, name.c_str(), StaticFlag);
    vtkXW2NodeHelp helper(outputDOM, staticnode, StaticFlag);
    timeStepDOM = this->CreateXdmfGrid(dsinput, name.c_str(), -1, 0.0, &helper);    
  }
  if (mbinput) {
    vtkSmartPointer<vtkCompositeDataIterator> iter;
    iter.TakeReference(mbinput->NewIterator());
    int index = 0;
    timeStepDOM = this->CreateEmptyCollection("vtkMultiBlock", "Spatial");
    for (iter->InitTraversal(); !iter->IsDoneWithTraversal(); iter->GoToNextItem()) {
      const char *dsname = mbinput->GetMetaData(iter)->Get(vtkCompositeDataSet::NAME());
      dsinput = vtkDataSet::SafeDownCast(iter->GetCurrentDataObject());
      bool StaticFlag = false;
      if (dsinput) {
        vtkstd::string name = this->MakeGridName(dsinput, NULL, index);
        staticnode = this->GetStaticGridNode(dsinput, true, outputDOM, name.c_str(), StaticFlag);
        vtkXW2NodeHelp helper(outputDOM, staticnode, StaticFlag);
        std::cout << "Adding block by index " << index << std::endl;
        XdmfDOM *block = this->CreateXdmfGrid(dsinput, name.c_str(), index++, 0.0, &helper);    
        this->AddGridToCollection(timeStepDOM, block);
        delete block;
      }
    }
  }

  this->Callback->Synchronize();

  if (timeStepDOM) {
    XdmfXmlNode domain = timeStepDOM->FindElement("Domain");

    this->WriteOutputXML(outputDOM, timeStepDOM, current_time);
  }

#ifdef VTK_USE_MPI
  if (this->Controller) {
    this->Controller->Barrier();
  }
#endif

  if (timeStepDOM) {
    delete timeStepDOM;
  }

  delete outputDOM;
  this->TimeStep++;

  return 1;
}
//----------------------------------------------------------------------------
void vtkXdmfWriter4::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

