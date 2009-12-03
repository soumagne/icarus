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
#include "XdmfRoot.h"
#include "XdmfDomain.h"
#include "XdmfGrid.h"
#include "XdmfTopology.h"
#include "XdmfGeometry.h"
#include "XdmfTime.h"
#include "XdmfAttribute.h"
#include "XdmfDataItem.h"

#include <cstdlib>
//----------------------------------------------------------------------------
XdmfGenerator::XdmfGenerator()
{
  this->GeneratedDOM  = new XdmfDOM();
  this->GeneratedFile = new std::string();
  this->LXdmfDOM      = new XdmfDOM();
  this->HdfDOM        = new XdmfHDFDOM();
}
//----------------------------------------------------------------------------
XdmfGenerator::~XdmfGenerator()
{
  if (this->GeneratedDOM) delete this->GeneratedDOM;
  if (this->GeneratedFile) delete this->GeneratedFile;
  if (this->LXdmfDOM) delete LXdmfDOM;
  if (this->HdfDOM) delete HdfDOM;
}
//----------------------------------------------------------------------------
void XdmfGenerator::SetHdfFileName(XdmfConstString fileName)
{
  // To fit XDMF dataset path convention
  this->HdfFileName = std::string(fileName) + std::string(":");
}
//----------------------------------------------------------------------------
XdmfDOM *XdmfGenerator::GetGeneratedDOM()
{
  return this->GeneratedDOM;
}
//----------------------------------------------------------------------------
XdmfConstString XdmfGenerator::GetGeneratedFile()
{
  *this->GeneratedFile = this->GeneratedFileStream.str();
  return this->GeneratedFile->c_str();
}
//----------------------------------------------------------------------------
XdmfInt32 XdmfGenerator::GenerateHead()
{
 this->GeneratedFileStream << "<?xml version=\"1.0\" ?>" << endl
        << "<!DOCTYPE Xdmf SYSTEM \"Xdmf.dtd\" []>" << endl ;
}
//----------------------------------------------------------------------------
XdmfInt32 XdmfGenerator::Generate(XdmfConstString lXdmfFile, XdmfConstString dumpHdfDescription)
{
  XdmfRoot           root;
  XdmfDomain         domain;
  XdmfXmlNode        domainNode;

  if (this->HdfFileName == "") {
    XdmfErrorMessage("HdfFileName must be set before generating");
    return(XDMF_FAIL);
  }

  // Start Building new DOM
  root.SetDOM(this->GeneratedDOM);
  domain.SetDOM(this->GeneratedDOM);
  root.Build();
  root.Insert(&domain);
  root.Build();

  // Fill LXdmfDOM
  if (this->LXdmfDOM->Parse(lXdmfFile) != XDMF_SUCCESS) {
    XdmfErrorMessage("Unable to parse light XDMF xml file");
    return(XDMF_FAIL);
  }

  // Fill HDF DOM
  if (this->HdfDOM->Parse(dumpHdfDescription) != XDMF_SUCCESS) {
    XdmfErrorMessage("Unable to parse HDF xml file");
    return(XDMF_FAIL);
  }
  XdmfDebug(this->HdfDOM->Serialize());

  //Find domain element
  domainNode = this->LXdmfDOM->FindElement("Domain");

  // Fill the GeneratedDOM
  int numberOfGrids = this->LXdmfDOM->FindNumberOfElements("Grid", domainNode);
  for (int currentGridIndex=0; currentGridIndex<numberOfGrids; currentGridIndex++) {
    XdmfGrid      grid;
    XdmfTopology *topology;
    XdmfGeometry *geometry;
    XdmfTime     *timeInfo;
    XdmfXmlNode   gridNode       = this->LXdmfDOM->FindElement("Grid", currentGridIndex, domainNode);
    XdmfXmlNode   topologyNode   = this->LXdmfDOM->FindElement("Topology", 0, gridNode);
    XdmfXmlNode   geometryNode   = this->LXdmfDOM->FindElement("Geometry", 0, gridNode);
    XdmfXmlNode   timeNode       = this->LXdmfDOM->FindElement("Time", 0, gridNode);

    grid.SetDOM(this->GeneratedDOM);
    domain.Insert(&grid);

    // Look for Topology
    topology = grid.GetTopology();
    topology->SetTopologyTypeFromString(this->LXdmfDOM->GetAttribute(topologyNode, "TopologyType"));
    XdmfXmlNode topologyDINode = this->LXdmfDOM->FindElement("DataItem", 0, topologyNode);
    if(topologyDINode != NULL) {
      XdmfConstString topologyPath = this->LXdmfDOM->GetCData(topologyDINode);
      XdmfXmlNode hdfTopologyNode = this->FindConvertHDFPath(topologyPath);
      topology->SetNumberOfElements(this->FindNumberOfCells(hdfTopologyNode,
          this->LXdmfDOM->GetAttribute(topologyNode, "TopologyType")));
      topology->SetDataXml(this->FindDataItemInfo(hdfTopologyNode, topologyPath));
    }

    // Look for Geometry
    geometry = grid.GetGeometry();
    geometry->SetGeometryTypeFromString(this->LXdmfDOM->GetAttribute(geometryNode, "GeometryType"));
    XdmfXmlNode geometryDINode = this->LXdmfDOM->FindElement("DataItem", 0, geometryNode);
    XdmfConstString geometryPath = this->LXdmfDOM->GetCData(geometryDINode);
    XdmfXmlNode hdfGeometryNode = this->FindConvertHDFPath(geometryPath);
    if(topologyDINode == NULL) {
      topology->SetNumberOfElements(this->FindNumberOfCells(hdfGeometryNode,
          this->LXdmfDOM->GetAttribute(topologyNode, "TopologyType")));
    }
    geometry->SetDataXml(this->FindDataItemInfo(hdfGeometryNode, geometryPath));

    // Look for Attributes
    int numberOfAttributes = LXdmfDOM->FindNumberOfElements("Attribute", gridNode);
    for(int currentAttributeIndex=0; currentAttributeIndex<numberOfAttributes; currentAttributeIndex++) {
      XdmfAttribute  *attribute        = new XdmfAttribute();
      XdmfXmlNode     attributeNode    = LXdmfDOM->FindElement("Attribute", currentAttributeIndex, gridNode);
      XdmfXmlNode     attributeDINode  = this->LXdmfDOM->FindElement("DataItem", 0, attributeNode);
      XdmfConstString attributePath    = this->LXdmfDOM->GetCData(attributeDINode);
      XdmfXmlNode     hdfAttributeNode = this->FindConvertHDFPath(attributePath);

      // Set Attribute Name
      vtksys::RegularExpression reName("/([^/]*)$");
      reName.find(attributePath);
      attribute->SetName(reName.match(1).c_str());
      // Set node center by default at the moment
      attribute->SetAttributeCenter(XDMF_ATTRIBUTE_CENTER_NODE);
      // Set Atrribute Type
      attribute->SetAttributeType(this->FindAttributeType(hdfAttributeNode));

      attribute->SetDataXml(this->FindDataItemInfo(hdfAttributeNode, attributePath));
      grid.Insert(attribute);
      attribute->SetDeleteOnGridDelete(1);
    }

    // Look for Time
    // TODO Add Time Node / enhancements??
    timeInfo = new XdmfTime();
    timeInfo->SetTimeType(XDMF_TIME_SINGLE);
    timeInfo->SetValue(0);
    grid.Insert(timeInfo);
    timeInfo->SetDeleteOnGridDelete(1);

    grid.Build();
  }

  this->GeneratedFileStream << this->GeneratedDOM->Serialize();
  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfXmlNode XdmfGenerator::FindConvertHDFPath(XdmfConstString path)
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
  node = this->HdfDOM->FindElementByPath(newPath.c_str());
  return node;
}
//----------------------------------------------------------------------------
XdmfInt32 XdmfGenerator::FindNumberOfCells(XdmfXmlNode hdfTopologyNode, XdmfConstString topologyTypeStr)
{
  XdmfInt32   numberOfCells;
  std::string topologyType = topologyTypeStr;
  for (int i=0; i<topologyType.length(); i++) {
    topologyType[i] = toupper(topologyType[i]);
  }

  XdmfXmlNode hdfDataspaceNode = this->HdfDOM->FindElement("Dataspace", 0, hdfTopologyNode);
  XdmfInt32   dimSize = atoi(this->HdfDOM->GetAttribute(
      this->HdfDOM->GetChild(0, this->HdfDOM->GetChild(0, hdfDataspaceNode)),
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
XdmfConstString XdmfGenerator::FindDataItemInfo(XdmfXmlNode hdfDatasetNode, XdmfConstString dataPath)
{
  XdmfXmlNode hdfDataspaceNode = this->HdfDOM->FindElement("Dataspace", 0, hdfDatasetNode);
  XdmfInt32 nDims = atoi(this->HdfDOM->GetAttribute(this->HdfDOM->GetChild(0, hdfDataspaceNode), "Ndims"));
  std::string dimSize;
  for (int i=0; i<nDims; i++) {
    dimSize += this->HdfDOM->GetAttribute(
        this->HdfDOM->GetChild(i, this->HdfDOM->GetChild(0, hdfDataspaceNode)),
        "DimSize"
    );
    if (i != (nDims-1)) dimSize += " ";
  }

  XdmfXmlNode hdfDatatypeNode = this->HdfDOM->FindElement("DataType", 0, hdfDatasetNode);
  std::string hdfDataType = this->HdfDOM->GetElementName(
      this->HdfDOM->GetChild(0, this->HdfDOM->GetChild(0, hdfDatatypeNode)));

  std::string dataType;
  // Float | Int | UInt | Char | UChar
  if (hdfDataType == "IntegerType")
    dataType = "Int";
  else if (hdfDataType == "FloatType")
    dataType = "Float";

  std::string dataPrecision = this->HdfDOM->GetAttribute(
      this->HdfDOM->GetChild(0, this->HdfDOM->GetChild(0, hdfDatatypeNode)),
      "Size");

  // TODO Instead of using a string, maybe replace this by using XdmfDataItem
  std::string dataItem =
      "<DataItem Dimensions=\"" + dimSize + "\" " +
      "NumberType=\"" + dataType + "\" " +
      "Precision=\"" + dataPrecision + "\" " +
      "Format=\"HDF\">" +
      this->HdfFileName + std::string(dataPath) +
      "</DataItem>";
  XdmfString dataItemStr = new char[dataItem.length()];
  strcpy(dataItemStr, dataItem.c_str());
  return (XdmfConstString)dataItemStr;
}
//----------------------------------------------------------------------------
XdmfInt32 XdmfGenerator::FindAttributeType(XdmfXmlNode hdfDatasetNode)
{
  XdmfInt32 type;
  XdmfXmlNode hdfDataspaceNode = this->HdfDOM->FindElement("Dataspace", 0, hdfDatasetNode);
  XdmfInt32 nDims = atoi(this->HdfDOM->GetAttribute(this->HdfDOM->GetChild(0, hdfDataspaceNode), "Ndims"));

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

