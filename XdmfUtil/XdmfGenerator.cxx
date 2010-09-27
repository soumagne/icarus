/*=========================================================================

  Project                 : XdmfUtil
  Module                  : XdmfGenerator.cxx

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
#ifdef USE_VLD_MEMORY_LEAK_DETECTION
 #include "vld.h"
#endif
#include <libxml/tree.h>
#include "XdmfGenerator.h"
#include "XdmfArray.h"
//
#include <vtksys/RegularExpression.hxx>
//
#include "XdmfHDFDOM.h"
#include "XdmfGrid.h"
#include "XdmfTopology.h"
#include "XdmfGeometry.h"
#include "XdmfTime.h"
#include "XdmfAttribute.h"
#include "XdmfDataItem.h"
#include "XdmfDataDesc.h"

#ifdef USE_H5FD_DSM
#include "H5FDdsmBuffer.h"
#endif
#include "XdmfDump.h"

#include "FileSeriesFinder.h"

#include <cstdlib>
#include <stack>
//----------------------------------------------------------------------------
XdmfGenerator::XdmfGenerator()
{
  this->DsmBuffer = NULL;

  // Set the generated DOM
  this->GeneratedRoot.SetDOM(&this->GeneratedDOM);
  this->GeneratedRoot.Build();
  this->GeneratedRoot.Insert(&this->GeneratedDomain);
  this->GeneratedRoot.Build();
}
//----------------------------------------------------------------------------
XdmfGenerator::~XdmfGenerator()
{
}
//----------------------------------------------------------------------------
XdmfDOM *XdmfGenerator::GetGeneratedDOM()
{
  return &this->GeneratedDOM;
}
//----------------------------------------------------------------------------
XdmfConstString XdmfGenerator::GetGeneratedFile()
{
  std::ostringstream  generatedFileStream;
  generatedFileStream << "<?xml version=\"1.0\" ?>" << endl
      << "<!DOCTYPE Xdmf SYSTEM \"Xdmf.dtd\" []>" << endl
      << this->GeneratedDOM.Serialize();
  this->GeneratedFile = generatedFileStream.str();
  return this->GeneratedFile.c_str();
}
//----------------------------------------------------------------------------
void XdmfGenerator::SetDsmBuffer(H5FDdsmBuffer *dsmBuffer)
{
    this->DsmBuffer = dsmBuffer;
}
//----------------------------------------------------------------------------
XdmfInt32 XdmfGenerator::GenerateTemporalCollection(XdmfConstString lXdmfFile,
    XdmfConstString anHdfFile, XdmfConstString fileNamePattern)
{
  vtkstd::vector<double> timeStepValues;
  XdmfGrid temporalGrid;
  FileSeriesFinder *fileFinder;

  if (fileNamePattern) {
    fileFinder = new FileSeriesFinder(fileNamePattern);
  } else {
    fileFinder = new FileSeriesFinder();
  }
  temporalGrid.SetGridType(XDMF_GRID_COLLECTION);
  temporalGrid.SetCollectionType(XDMF_GRID_COLLECTION_TEMPORAL);
  this->GeneratedDomain.Insert(&temporalGrid);
  // Build the temporal grid
  temporalGrid.Build();

  // default file type: output.0000.h5
  fileFinder->SetPrefixRegEx("(.*)?");
  fileFinder->SetTimeRegEx("([0-9]+)");
  fileFinder->Scan(anHdfFile);
  // TODO use time values of files eventually
  fileFinder->GetTimeValues(timeStepValues);

//  std::cerr << "Number of TimeSteps: " <<fileFinder->GetNumberOfTimeSteps() << std::endl;
  for (int i=0; i<fileFinder->GetNumberOfTimeSteps(); i++) {
//    std::cerr << "Generate file name for time step: " << timeStepValues[i] << std::endl;
    std::string currentHdfFile = fileFinder->GenerateFileName(i);
//    std::cerr << "File name generated: " << currentHdfFile << std::endl;
    this->Generate(lXdmfFile, currentHdfFile.c_str(), &temporalGrid, timeStepValues[i]);
  }
  delete fileFinder;
  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfInt32 XdmfGenerator::Generate(XdmfConstString lXdmfFile, XdmfConstString hdfFileName,
    XdmfGrid *temporalGrid, XdmfInt32 timeValue)
{
  XdmfXmlNode         domainNode;
  XdmfDOM            *lXdmfDOM = new XdmfDOM();
  XdmfHDFDOM         *hdfDOM = new XdmfHDFDOM();
  XdmfDump           *hdfFileDump = new XdmfDump();
  XdmfGrid           *spatialGrid = NULL;
  XdmfTime           *timeInfo = NULL;
  std::ostringstream  hdfFileDumpStream;

  if (!hdfFileName) {
    XdmfErrorMessage("HDF file name must be set before generating");
    return(XDMF_FAIL);
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

//  std::cout << hdfFileDumpStream.str().c_str() << std::endl;

  // Fill HDF DOM
  if (hdfDOM->Parse(hdfFileDumpStream.str().c_str()) != XDMF_SUCCESS) {
    XdmfErrorMessage("Unable to parse HDF xml file");
    return(XDMF_FAIL);
  }
//  hdfDOM->SetGlobalDebug(1);
//  std::cout << (hdfDOM->Serialize()) << std::endl;

  //Find domain element
  domainNode = lXdmfDOM->FindElement("Domain");

  //
  // If the template has multiple grids, create a spatial grid 
  // to hold the multi-block structure
  //
  int numberOfGrids = lXdmfDOM->FindNumberOfElements("Grid", domainNode);
  if (numberOfGrids>1) {
    // Use a spatial grid collection to store the subgrids
    spatialGrid = new XdmfGrid();
    spatialGrid->SetGridType(XDMF_GRID_COLLECTION);
    spatialGrid->SetCollectionType(XDMF_GRID_COLLECTION_SPATIAL);
    if (temporalGrid) {
      temporalGrid->Insert(spatialGrid);
    }
    else {
      this->GeneratedDomain.Insert(spatialGrid);
    }
    spatialGrid->SetDeleteOnGridDelete(1);
    spatialGrid->Build();
  }

  // Fill the GeneratedDOM
  XdmfGrid *grid = NULL;
  for (int currentGridIndex=0; currentGridIndex<numberOfGrids; currentGridIndex++) {
    grid = new XdmfGrid();
    XdmfTopology *topology;
    XdmfGeometry *geometry;
    XdmfXmlNode   gridNode       = lXdmfDOM->FindElement("Grid", currentGridIndex, domainNode);
    XdmfXmlNode   gridInfoDINode = NULL;
    XdmfXmlNode   topologyNode   = lXdmfDOM->FindElement("Topology", 0, gridNode);
    XdmfString    topologyTypeStr = NULL;
    XdmfXmlNode   topologyDINode  = NULL;
    XdmfXmlNode   geometryNode   = lXdmfDOM->FindElement("Geometry", 0, gridNode);
    XdmfString    geometryTypeStr = NULL;
    XdmfXmlNode   geometryDINode = NULL;
    // TODO Use timeNode
    //XdmfXmlNode   timeNode       = lXdmfDOM->FindElement("Time", 0, gridNode);

    // Set grid name
    XdmfString gridName = (XdmfString) lXdmfDOM->GetAttribute(gridNode, "Name");
    if (gridName) {
      grid->SetName(gridName);
      free(gridName);
    }
    else {
      gridInfoDINode = lXdmfDOM->FindElement("DataItem", 0, topologyNode);
      if (gridInfoDINode == NULL) {
        gridInfoDINode = lXdmfDOM->FindElement("DataItem", 0, geometryNode);
      }
      if (gridInfoDINode != NULL) {
        XdmfConstString gridInfoPath = lXdmfDOM->GetCData(gridInfoDINode);
        vtksys::RegularExpression gridName("^/([^/]*)/*");
        gridName.find(gridInfoPath);
        XdmfDebug("Grid name: " << gridName.match(1).c_str());
        grid->SetName(gridName.match(1).c_str());
      }
    }

    // Look for Topology
    topology = grid->GetTopology();
    topologyTypeStr = (XdmfString) lXdmfDOM->GetAttribute(topologyNode, "TopologyType");
    topology->SetTopologyTypeFromString(topologyTypeStr);
    topologyDINode = lXdmfDOM->FindElement("DataItem", 0, topologyNode);
    if(topologyDINode != NULL) {
      XdmfConstString topologyPath = lXdmfDOM->GetCData(topologyDINode);
      XdmfXmlNode hdfTopologyNode = this->FindConvertHDFPath(hdfDOM, topologyPath);
      if (!hdfTopologyNode) {
        XdmfDebug("Skipping node of path " << topologyPath);
        continue;
      }
      XdmfConstString topologyData = this->FindDataItemInfo(hdfDOM, hdfTopologyNode, hdfFileName, topologyPath, lXdmfDOM, topologyDINode);
      topology->SetNumberOfElements(this->FindNumberOfCells(hdfDOM, hdfTopologyNode, topologyTypeStr));
      topology->SetDataXml(topologyData);
      if (topologyData) delete []topologyData;
    }

    // Look for Geometry
    geometry = grid->GetGeometry();
    geometryTypeStr = (XdmfString) lXdmfDOM->GetAttribute(geometryNode, "GeometryType");
    geometry->SetGeometryTypeFromString(geometryTypeStr);
    if (geometryTypeStr) free(geometryTypeStr);
    geometryDINode = lXdmfDOM->FindElement("DataItem", 0, geometryNode);

    // we might need to read multiple items for x/y/z sizes (2-3 or more dimensions)
    std::vector<XdmfInt64> numcells;
    std::string geomXML = "<Geometry>";
    while (geometryDINode != NULL) {
      XdmfConstString geometryPath = lXdmfDOM->GetCData(geometryDINode);
      XdmfXmlNode hdfGeometryNode = this->FindConvertHDFPath(hdfDOM, geometryPath);
      if (!hdfGeometryNode) {
        XdmfDebug("Skipping node of path " << geometryPath);
        geometryDINode = geometryDINode->next;
        continue;
      }
      XdmfConstString geometryData = this->FindDataItemInfo(hdfDOM, hdfGeometryNode, hdfFileName, geometryPath, lXdmfDOM, geometryDINode);
      if (geometryData) {
        geomXML += geometryData;
        delete []geometryData;
      }
      // Xdmf dimensions are in reverse order, so add each dimension to start of array, not end
      XdmfInt64 N = this->FindNumberOfCells(hdfDOM, hdfGeometryNode, topologyTypeStr);
      numcells.insert(numcells.begin(),N);
      //
      geometryDINode = geometryDINode->next;
    }
    geomXML += "</Geometry>";
    geometry->SetDataXml(geomXML.c_str());
    //
    if (topologyDINode == NULL) {
      topology->GetShapeDesc()->SetShape(numcells.size(), &numcells[0]);
    }
    if (topologyTypeStr) free(topologyTypeStr);

    if (spatialGrid) {
      spatialGrid->Insert(grid);
      grid->SetDeleteOnGridDelete(1);
    }
    else if (temporalGrid) {
      temporalGrid->Insert(grid);
      grid->SetDeleteOnGridDelete(1);
    }
    else {
      this->GeneratedDomain.Insert(grid);
    }

    // Look for Attributes
    int numberOfAttributes = lXdmfDOM->FindNumberOfElements("Attribute", gridNode);
    for(int currentAttributeIndex=0; currentAttributeIndex<numberOfAttributes; currentAttributeIndex++) {
      XdmfAttribute  *attribute        = new XdmfAttribute();
      XdmfXmlNode     attributeNode    = lXdmfDOM->FindElement("Attribute", currentAttributeIndex, gridNode);
      XdmfXmlNode     attributeDINode  = lXdmfDOM->FindElement("DataItem", 0, attributeNode);
      XdmfConstString attributePath    = lXdmfDOM->GetCData(attributeDINode);
      XdmfXmlNode     hdfAttributeNode = this->FindConvertHDFPath(hdfDOM, attributePath);
      XdmfConstString attributeData    = NULL;

      if (!hdfAttributeNode) {
        // The node does not exist in the HDF DOM so do not generate it
        XdmfDebug("Skipping node of path " << attributePath);
        continue;
      }

      // Set Attribute Name, use one from template if it exists
      XdmfString attributeName = (XdmfString) lXdmfDOM->GetAttribute(attributeNode, "Name");
      if (attributeName) {
        attribute->SetName(attributeName);
        free(attributeName);
      }
      else {
        vtksys::RegularExpression reName("/([^/]*)$");
        reName.find(attributePath);
        std::string tmpName = std::string("attribute_") + reName.match(1).c_str();
        XdmfDebug("Attribute name: " << tmpName.c_str());
        attribute->SetName(tmpName.c_str());
      }

      // Set node center by default at the moment
      attribute->SetAttributeCenter(XDMF_ATTRIBUTE_CENTER_NODE);

      // Get Attribute info
      attributeData = this->FindDataItemInfo(hdfDOM, hdfAttributeNode, hdfFileName, attributePath, lXdmfDOM, attributeDINode);
      attribute->SetDataXml(attributeData);
      attribute->SetAttributeType(this->FindAttributeType(hdfDOM, hdfAttributeNode, lXdmfDOM, attributeNode));

      if (attributeData) delete []attributeData;
      grid->Insert(attribute);
      attribute->SetDeleteOnGridDelete(1);
    }
    
    grid->Build();

    //
    // After Grid build has been called, our geometry tag is wrong since we added a composite
    // {x,y,z} XML tag instead of a simple one, so delete one level of xml nodes from 
    // each valid Geometry tag (use stack to handle multiple blocks etc)
    //
    XdmfXmlNode domainNode2 = this->GeneratedDOM.FindElement("Domain");
    XdmfXmlNode   gridNode2 = this->GeneratedDOM.FindElement("Grid", 0, domainNode2);
    std::stack<XdmfXmlNode> nodestack;
    nodestack.push(gridNode2);
    XdmfXmlNode node;
    while (!nodestack.empty()) {
      node = nodestack.top();
      nodestack.pop();
      // Does this grid have child grids, if so push onto stack
      XdmfXmlNode child = this->GeneratedDOM.FindElement("Grid", 0, node);
      if (child) nodestack.push(child);
      // Does this grid have sibling grids, if so push onto stack
      XdmfXmlNode sibling = this->GeneratedDOM.FindNextElement("Grid", node);
      if (sibling) nodestack.push(sibling);
      // Does the node have a Geometry node?
      XdmfXmlNode geometryNode2 = this->GeneratedDOM.FindElement("Geometry", 0, node);
      if (geometryNode2) {
        // replace the Geometry node with its (real) child Geometry node
        XdmfXmlNode tmp = geometryNode2->children;
        if (tmp && !xmlStrcmp(tmp->name, BAD_CAST("Geometry"))) {
          geometryNode2->children = geometryNode2->children->children;
          tmp->children = NULL;
          xmlFreeNode(tmp);
        }
      }
    }

    // Normally container deletes the grid, if no container, we must do it
    if (!spatialGrid && !temporalGrid) {
      delete grid;
    }

  }
  if (temporalGrid) {
    // Look for Time
    // TODO Add Time Node / enhancements??
    timeInfo = new XdmfTime();
    timeInfo->SetTimeType(XDMF_TIME_SINGLE);
    timeInfo->SetValue(timeValue);
    if (spatialGrid) spatialGrid->Insert(timeInfo);
    else if (grid) grid->Insert(timeInfo);
    timeInfo->Build();
    delete timeInfo;
  }
  delete hdfDOM;
  delete lXdmfDOM;
  delete hdfFileDump;

  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfXmlNode XdmfGenerator::FindConvertHDFPath(XdmfHDFDOM *hdfDOM, XdmfConstString path)
{
  std::string newPath = "/HDF5-File/RootGroup/";
  std::string currentBlockName = "";
  XdmfXmlNode node = NULL;

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
  XdmfXmlNode hdfDataspaceNode;
  XdmfInt32   dimSize;
  XdmfString  dimSizeStr = NULL;

  for (int i=0; i<(int)topologyType.length(); i++) {
    topologyType[i] = toupper(topologyType[i]);
  }

  hdfDataspaceNode = hdfDOM->FindElement("Dataspace", 0, hdfTopologyNode);
  dimSizeStr = (XdmfString) hdfDOM->GetAttribute(hdfDOM->GetChild(0,
      hdfDOM->GetChild(0, hdfDataspaceNode)), "DimSize");
  dimSize = atoi(dimSizeStr);
  if (dimSizeStr) free(dimSizeStr);

  // TODO Do other topology types
  if (topologyType == "MIXED") {
    numberOfCells = dimSize - 1;
  }
  else if (topologyType == "POLYVERTEX") {
    numberOfCells = dimSize;
  }
  else if (topologyType == "3DRECTMESH") {
    numberOfCells = dimSize;
  }

  return numberOfCells;
}
//----------------------------------------------------------------------------
XdmfConstString XdmfGenerator::FindDataItemInfo(XdmfHDFDOM *hdfDOM, XdmfXmlNode hdfDatasetNode,
    XdmfConstString hdfFileName, XdmfConstString dataPath, XdmfDOM *lXdmfDOM, XdmfXmlNode templateNode)
{
  XdmfXmlNode hdfDataspaceNode = NULL, hdfDatatypeNode = NULL;
  XdmfString nDimsStr, dataPrecisionStr, dataItemStr;
  XdmfInt32 nDims;
  std::string dimSize, hdfDataType, dataType, dataPrecision, dataItem;

  hdfDataspaceNode = hdfDOM->FindElement("Dataspace", 0, hdfDatasetNode);
  if (!hdfDataspaceNode) {
    XdmfErrorMessage("No Dataspace element found");
    return NULL;
  }
  nDimsStr = (XdmfString) hdfDOM->GetAttribute(hdfDOM->GetChild(0, hdfDataspaceNode), "Ndims");
  nDims = atoi(nDimsStr);
  if (nDimsStr) free(nDimsStr);

  // The attribute type (Scalar/Vector/Tensor/None) should be detected from the 
  // size of the first dimension - so 3x64x64x64 is a cube(64) of vectors
  // and 1x32x32 is a plane of scalars
  for (int i=0; i<nDims; i++) {
    XdmfString dimSizeStr = (XdmfString) hdfDOM->GetAttribute(
        hdfDOM->GetChild(i, hdfDOM->GetChild(0, hdfDataspaceNode)), "DimSize");
    dimSize += dimSizeStr;
    if (i != (nDims-1)) dimSize += " ";
    if (dimSizeStr) free(dimSizeStr);
  }

  hdfDatatypeNode = hdfDOM->FindElement("DataType", 0, hdfDatasetNode);
  if (!hdfDatatypeNode) {
    XdmfErrorMessage("No DataType element found");
    return NULL;
  }
  hdfDataType = hdfDOM->GetElementName(hdfDOM->GetChild(0, hdfDOM->GetChild(0, hdfDatatypeNode)));

  // Float | Int | UInt | Char | UChar
  if (hdfDataType == "IntegerType") {
    dataType = "Int";
  }
  else if (hdfDataType == "FloatType") {
    dataType = "Float";
  }

  dataPrecisionStr = (XdmfString) hdfDOM->GetAttribute(hdfDOM->GetChild(0,
      hdfDOM->GetChild(0, hdfDatatypeNode)), "Size");
  dataPrecision = dataPrecisionStr;
  if (dataPrecisionStr) free(dataPrecisionStr);

  std::string diName = "";
  std::string name = vtksys::SystemTools::GetFilenameName(dataPath);
  if (templateNode) {
    XdmfConstString nodeName = lXdmfDOM->GetAttribute(templateNode, "Name");
    if (nodeName) {
      diName = "Name=\"" + std::string(nodeName) + "\" ";
    }
  }

  // 1) Never use windows stye slashes in hdf paths
  // 2) Only use the relative file name, drop the path 
  // otherwise as you can't copy hdf5 + xml files between locations
  std::string unixname = hdfFileName;
  std::replace(unixname.begin(), unixname.end(), '\\', '/');
  size_t found = unixname.find_last_of("/\\");
  unixname = unixname.substr(found+1);

  // TODO Instead of using a string, may replace this by using XdmfDataItem
  dataItem =
      std::string("<DataItem ") +
      diName + 
      "Dimensions=\"" + dimSize + "\" " +
      "NumberType=\"" + dataType + "\" " +
      "Precision=\"" + dataPrecision + "\" " +
      "Format=\"HDF\">" +
      unixname + ":" + std::string(dataPath) +
      "</DataItem>";
  dataItemStr = new char[dataItem.length()+1];
  strcpy(dataItemStr, dataItem.c_str());

  return (XdmfConstString)dataItemStr;
}
//----------------------------------------------------------------------------
XdmfInt32 XdmfGenerator::FindAttributeType(XdmfHDFDOM *hdfDOM, XdmfXmlNode hdfDatasetNode, XdmfDOM *lXdmfDOM, XdmfXmlNode templateNode)
{
  XdmfXmlNode hdfDataspaceNode;
  XdmfInt32 nDims;
  XdmfString nDimsStr, attrType;

  // if the template has defined the attribute type, use it
  attrType = (XdmfString) lXdmfDOM->GetAttribute(templateNode, "AttributeType");
  if (attrType) {
    XdmfInt32 aType = XDMF_ATTRIBUTE_TYPE_NONE;
    if      (!strcmp(attrType,"Scalar")) aType = XDMF_ATTRIBUTE_TYPE_SCALAR;
    else if (!strcmp(attrType,"Vector")) aType = XDMF_ATTRIBUTE_TYPE_VECTOR;
    else if (!strcmp(attrType,"Tensor")) aType = XDMF_ATTRIBUTE_TYPE_TENSOR;
    if (attrType) free(attrType);
    return aType;
  }

  // Otherwise, we can't be sure, but if only one dimentsion, it must be scalar
  hdfDataspaceNode = hdfDOM->FindElement("Dataspace", 0, hdfDatasetNode);
  nDimsStr = (XdmfString) hdfDOM->GetAttribute(hdfDOM->GetChild(0, hdfDataspaceNode), "Ndims");
  nDims = atoi(nDimsStr);
  if (nDimsStr) free(nDimsStr);

  // This is unreliable, if ndims==1 then scalar is correct, otherwise we are guessing.
  switch (nDims) {
    case 1:
      return XDMF_ATTRIBUTE_TYPE_SCALAR;
    case 2:
      return XDMF_ATTRIBUTE_TYPE_VECTOR;
    default:
      return XDMF_ATTRIBUTE_TYPE_NONE;
  }
  return XDMF_ATTRIBUTE_TYPE_NONE;
}
//----------------------------------------------------------------------------

