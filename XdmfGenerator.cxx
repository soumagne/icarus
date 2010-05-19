/*=========================================================================

  Project                 : vtkCSCS
  Module                  : XdmfGenerator.h
  Revision of last commit : $Rev$
  Author of last commit   : $Author$
  Date of last commit     : $Date::                            $

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
#include "XdmfGenerator.h"
//
#include <vtksys/RegularExpression.hxx>
//
#include "XdmfDOM.h"
#include "XdmfHDFDOM.h"
#include "XdmfRoot.h"
#include "XdmfDomain.h"
#include "XdmfGrid.h"
#include "XdmfTopology.h"
#include "XdmfGeometry.h"
#include "XdmfTime.h"
#include "XdmfAttribute.h"
#include "XdmfDataItem.h"

#include "XdmfDsmDump.h"
#include "XdmfDsmBuffer.h"

#include <cstdlib>
//----------------------------------------------------------------------------
XdmfGenerator::XdmfGenerator()
{
  this->GeneratedDOM  = new XdmfDOM();
  this->GeneratedFile = new std::string();

  // Start Building new DOM
  this->GeneratedRoot = new XdmfRoot();
  this->GeneratedDomain = new XdmfDomain();
  this->GeneratedRoot->SetDOM(this->GeneratedDOM);
  this->GeneratedRoot->Build();
  this->GeneratedRoot->Insert(this->GeneratedDomain);
  this->GeneratedRoot->Build();
}
//----------------------------------------------------------------------------
XdmfGenerator::~XdmfGenerator()
{
  if (this->GeneratedDOM) delete this->GeneratedDOM;
  if (this->GeneratedFile) delete this->GeneratedFile;
  if (this->GeneratedRoot) delete this->GeneratedRoot;
  if (this->GeneratedDomain) delete this->GeneratedDomain;
}
//----------------------------------------------------------------------------
XdmfDOM *XdmfGenerator::GetGeneratedDOM()
{
  return this->GeneratedDOM;
}
//----------------------------------------------------------------------------
XdmfConstString XdmfGenerator::GetGeneratedFile()
{
  std::ostringstream  generatedFileStream;
  generatedFileStream << "<?xml version=\"1.0\" ?>" << endl
      << "<!DOCTYPE Xdmf SYSTEM \"Xdmf.dtd\" []>" << endl
      << this->GeneratedDOM->Serialize();
  *this->GeneratedFile = generatedFileStream.str();
  return this->GeneratedFile->c_str();
}
//----------------------------------------------------------------------------
void XdmfGenerator::SetDsmBuffer(XdmfDsmBuffer *dsmBuffer)
{
    this->DsmBuffer = dsmBuffer;
}
//----------------------------------------------------------------------------
XdmfInt32 XdmfGenerator::GenerateTemporalCollection(XdmfConstString lXdmfFile,
    XdmfConstString anHdfFile)
{
  XdmfGrid *temporalGrid = new XdmfGrid();
  temporalGrid->SetGridType(XDMF_GRID_COLLECTION);
  temporalGrid->SetCollectionType(XDMF_GRID_COLLECTION_TEMPORAL);
  this->GeneratedDomain->Insert(temporalGrid);
  // Build the temporal grid
  temporalGrid->Build();

  std::string currentHdfFile = anHdfFile;

  for (int i=0; i<3; i++) {
    this->Generate(lXdmfFile, currentHdfFile.c_str(), temporalGrid);
  }
  delete temporalGrid;
  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfInt32 XdmfGenerator::Generate(XdmfConstString lXdmfFile, XdmfConstString hdfFileName,
    XdmfGrid *temporalGrid)
{
  XdmfXmlNode         domainNode;
  XdmfDOM            *lXdmfDOM = new XdmfDOM();
  XdmfHDFDOM         *hdfDOM = new XdmfHDFDOM();
  XdmfDsmDump        *hdfFileDump = new XdmfDsmDump();
  XdmfGrid           *spatialGrid = new XdmfGrid();
  XdmfTime           *timeInfo;
  std::ostringstream  hdfFileDumpStream;
  static int          timeValue = 0;

  if (!hdfFileName) {
    XdmfErrorMessage("HDF file name must be set before generating");
    return(XDMF_FAIL);
  }

  if (temporalGrid) {
    // Use a spatial grid collection to store the subgrids
    spatialGrid->SetGridType(XDMF_GRID_COLLECTION);
    spatialGrid->SetCollectionType(XDMF_GRID_COLLECTION_SPATIAL);
    temporalGrid->Insert(spatialGrid);
  }

  // Fill LXdmfDOM
  if (lXdmfDOM->Parse(lXdmfFile) != XDMF_SUCCESS) {
    XdmfErrorMessage("Unable to parse light XDMF xml file");
    return(XDMF_FAIL);
  }

  // Dump HDF file
  hdfFileDump->SetFileName(hdfFileName);
  if (this->DsmBuffer) {
    hdfFileDump->SetDsmBuffer(this->DsmBuffer);
  }
  hdfFileDump->DumpXML(hdfFileDumpStream);

  // Fill HDF DOM
  if (hdfDOM->Parse(hdfFileDumpStream.str().c_str()) != XDMF_SUCCESS) {
    XdmfErrorMessage("Unable to parse HDF xml file");
    return(XDMF_FAIL);
  }
  XdmfDebug(hdfDOM->Serialize());

  //Find domain element
  domainNode = lXdmfDOM->FindElement("Domain");

  // Fill the GeneratedDOM
  int numberOfGrids = lXdmfDOM->FindNumberOfElements("Grid", domainNode);
  for (int currentGridIndex=0; currentGridIndex<numberOfGrids; currentGridIndex++) {
    XdmfGrid      grid;
    XdmfTopology *topology;
    XdmfGeometry *geometry;
    XdmfXmlNode   gridNode       = lXdmfDOM->FindElement("Grid", currentGridIndex, domainNode);
    XdmfXmlNode   topologyNode   = lXdmfDOM->FindElement("Topology", 0, gridNode);
    XdmfXmlNode   geometryNode   = lXdmfDOM->FindElement("Geometry", 0, gridNode);
    // TODO Use timeNode
    //XdmfXmlNode   timeNode       = lXdmfDOM->FindElement("Time", 0, gridNode);

    //grid.SetDOM(this->GeneratedDOM);
    // Set grid name
    XdmfXmlNode gridInfoDINode = lXdmfDOM->FindElement("DataItem", 0, topologyNode);
    if (gridInfoDINode == NULL) {
      gridInfoDINode = lXdmfDOM->FindElement("DataItem", 0, geometryNode);
    }
    if (gridInfoDINode != NULL) {
      XdmfConstString gridInfoPath = lXdmfDOM->GetCData(gridInfoDINode);
      vtksys::RegularExpression gridName("^/([^/]*)/*");
      gridName.find(gridInfoPath);
      XdmfDebug("Grid name: " << gridName.match(1).c_str());
      grid.SetName(gridName.match(1).c_str());
    }

    if (temporalGrid) {
      spatialGrid->Insert(&grid);
      // Build the spatial grid
      spatialGrid->Build();
    } else {
      this->GeneratedDomain->Insert(&grid);
    }

    // Look for Topology
    topology = grid.GetTopology();
    topology->SetTopologyTypeFromString(lXdmfDOM->GetAttribute(topologyNode, "TopologyType"));
    XdmfXmlNode topologyDINode = lXdmfDOM->FindElement("DataItem", 0, topologyNode);
    if(topologyDINode != NULL) {
      XdmfConstString topologyPath = lXdmfDOM->GetCData(topologyDINode);
      XdmfXmlNode hdfTopologyNode = this->FindConvertHDFPath(hdfDOM, topologyPath);
      topology->SetNumberOfElements(this->FindNumberOfCells(hdfDOM, hdfTopologyNode,
          lXdmfDOM->GetAttribute(topologyNode, "TopologyType")));
      topology->SetDataXml(this->FindDataItemInfo(hdfDOM, hdfTopologyNode, hdfFileName, topologyPath));
    }

    // Look for Geometry
    geometry = grid.GetGeometry();
    geometry->SetGeometryTypeFromString(lXdmfDOM->GetAttribute(geometryNode, "GeometryType"));
    XdmfXmlNode geometryDINode = lXdmfDOM->FindElement("DataItem", 0, geometryNode);
    XdmfConstString geometryPath = lXdmfDOM->GetCData(geometryDINode);
    XdmfXmlNode hdfGeometryNode = this->FindConvertHDFPath(hdfDOM, geometryPath);
    if(topologyDINode == NULL) {
      topology->SetNumberOfElements(this->FindNumberOfCells(hdfDOM, hdfGeometryNode,
          lXdmfDOM->GetAttribute(topologyNode, "TopologyType")));
    }
    geometry->SetDataXml(this->FindDataItemInfo(hdfDOM, hdfGeometryNode, hdfFileName, geometryPath));

    // Look for Attributes
    int numberOfAttributes = lXdmfDOM->FindNumberOfElements("Attribute", gridNode);
    for(int currentAttributeIndex=0; currentAttributeIndex<numberOfAttributes; currentAttributeIndex++) {
      XdmfAttribute  *attribute        = new XdmfAttribute();
      XdmfXmlNode     attributeNode    = lXdmfDOM->FindElement("Attribute", currentAttributeIndex, gridNode);
      XdmfXmlNode     attributeDINode  = lXdmfDOM->FindElement("DataItem", 0, attributeNode);
      XdmfConstString attributePath    = lXdmfDOM->GetCData(attributeDINode);
      XdmfXmlNode     hdfAttributeNode = this->FindConvertHDFPath(hdfDOM, attributePath);

      // Set Attribute Name
      vtksys::RegularExpression reName("/([^/]*)$");
      reName.find(attributePath);
      std::string tmpName = std::string("attribute_") + reName.match(1).c_str();
      XdmfDebug("Attribute name: " << tmpName.c_str());
      attribute->SetName(tmpName.c_str());
      // Set node center by default at the moment
      attribute->SetAttributeCenter(XDMF_ATTRIBUTE_CENTER_NODE);
      // Set Atrribute Type
      attribute->SetAttributeType(this->FindAttributeType(hdfDOM, hdfAttributeNode));

      attribute->SetDataXml(this->FindDataItemInfo(hdfDOM, hdfAttributeNode, hdfFileName, attributePath));
      grid.Insert(attribute);
      attribute->SetDeleteOnGridDelete(1);
    }
    grid.Build();
  }
  if (temporalGrid) {
    // Look for Time
    // TODO Add Time Node / enhancements??
    timeInfo = new XdmfTime();
    timeInfo->SetTimeType(XDMF_TIME_SINGLE);
    timeInfo->SetValue(timeValue);
    timeValue++; // Blindly increment the time value for now
    spatialGrid->Insert(timeInfo);
    timeInfo->SetDeleteOnGridDelete(1);
    timeInfo->Build();
  }
  delete hdfDOM;
  delete lXdmfDOM;
  delete hdfFileDump;
  delete spatialGrid;

  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfXmlNode XdmfGenerator::FindConvertHDFPath(XdmfHDFDOM *hdfDOM, XdmfConstString path)
{
  std::string newPath = "/HDF5-File/RootGroup/";
  std::string currentBlockName = "";
  XdmfXmlNode node;

  // skip leading "/"
  int cursor = 1;

  while (path[cursor] != '\0') {
    if (path[cursor] == '/') {
      newPath += "Group[@Name=\"" + currentBlockName + "\"]/";
      currentBlockName.clear();
      currentBlockName = "";
    } else {
      currentBlockName += path[cursor];
    }
    cursor++;
  }

  newPath += "Dataset[@Name=\"" + currentBlockName + "\"]";
  node = hdfDOM->FindElementByPath(newPath.c_str());
  return node;
}
//----------------------------------------------------------------------------
XdmfInt32 XdmfGenerator::FindNumberOfCells(XdmfHDFDOM *hdfDOM,
    XdmfXmlNode hdfTopologyNode, XdmfConstString topologyTypeStr)
{
  XdmfInt32   numberOfCells = 0;
  std::string topologyType = topologyTypeStr;
  for (int i=0; i<(int)topologyType.length(); i++) {
    topologyType[i] = toupper(topologyType[i]);
  }

  XdmfXmlNode hdfDataspaceNode = hdfDOM->FindElement("Dataspace", 0, hdfTopologyNode);
  XdmfInt32   dimSize = atoi(hdfDOM->GetAttribute(
      hdfDOM->GetChild(0, hdfDOM->GetChild(0, hdfDataspaceNode)),
      "DimSize"));

  // TODO Do other topology types
  if (topologyType == "MIXED") {
    numberOfCells = dimSize - 1;
  }
  else if (topologyType == "POLYVERTEX") {
    numberOfCells = dimSize;
  }

  return numberOfCells;
}
//----------------------------------------------------------------------------
XdmfConstString XdmfGenerator::FindDataItemInfo(XdmfHDFDOM *hdfDOM, XdmfXmlNode hdfDatasetNode,
    XdmfConstString hdfFileName, XdmfConstString dataPath)
{
  XdmfXmlNode hdfDataspaceNode = hdfDOM->FindElement("Dataspace", 0, hdfDatasetNode);
  XdmfInt32 nDims = atoi(hdfDOM->GetAttribute(hdfDOM->GetChild(0, hdfDataspaceNode), "Ndims"));
  std::string dimSize;
  for (int i=0; i<nDims; i++) {
    dimSize += hdfDOM->GetAttribute(
        hdfDOM->GetChild(i, hdfDOM->GetChild(0, hdfDataspaceNode)),
        "DimSize"
    );
    if (i != (nDims-1)) dimSize += " ";
  }

  XdmfXmlNode hdfDatatypeNode = hdfDOM->FindElement("DataType", 0, hdfDatasetNode);
  std::string hdfDataType = hdfDOM->GetElementName(
      hdfDOM->GetChild(0, hdfDOM->GetChild(0, hdfDatatypeNode)));

  std::string dataType;
  // Float | Int | UInt | Char | UChar
  if (hdfDataType == "IntegerType")
    dataType = "Int";
  else if (hdfDataType == "FloatType")
    dataType = "Float";

  std::string dataPrecision = hdfDOM->GetAttribute(
      hdfDOM->GetChild(0, hdfDOM->GetChild(0, hdfDatatypeNode)),
      "Size");

  // TODO Instead of using a string, maybe replace this by using XdmfDataItem
  std::string dataItem =
      "<DataItem Dimensions=\"" + dimSize + "\" " +
      "NumberType=\"" + dataType + "\" " +
      "Precision=\"" + dataPrecision + "\" " +
      "Format=\"HDF\">" +
      std::string(hdfFileName) + ":" + std::string(dataPath) +
      "</DataItem>";
  XdmfString dataItemStr = new char[dataItem.length()+1];
  strcpy(dataItemStr, dataItem.c_str());
  return (XdmfConstString)dataItemStr;
}
//----------------------------------------------------------------------------
XdmfInt32 XdmfGenerator::FindAttributeType(XdmfHDFDOM *hdfDOM, XdmfXmlNode hdfDatasetNode)
{
  XdmfInt32 type;
  XdmfXmlNode hdfDataspaceNode = hdfDOM->FindElement("Dataspace", 0, hdfDatasetNode);
  XdmfInt32 nDims = atoi(hdfDOM->GetAttribute(hdfDOM->GetChild(0, hdfDataspaceNode), "Ndims"));

  // TODO Do other Xdmf Attribute types
  switch(nDims) {
  case 1:
    type = XDMF_ATTRIBUTE_TYPE_SCALAR;
    break;
  case 2:
    type = XDMF_ATTRIBUTE_TYPE_VECTOR;
    break;
  default:
    type = XDMF_ATTRIBUTE_TYPE_NONE;
    break;
  }
  return type;
}
//----------------------------------------------------------------------------

