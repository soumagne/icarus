/*=========================================================================

  Project                 : Icarus
  Module                  : XdmfSteeringParser.cxx

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
//
// libxml, must come before any stl includes
//
#include <libxml/globals.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xinclude.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
//
// STL
//
#include <string>
#include <iostream>
#include <cstdlib>
//
// Xdmf
//
#include "XdmfDOM.h"
#include "XdmfTopology.h"
//
// Our generator
//
#include "XdmfSteeringParser.h"
//
// vtk
//
#include <vtksys/RegularExpression.hxx>
#include <vtksys/SystemTools.hxx>
//
#include "vtkType.h"
//
//----------------------------------------------------------------------------
// Utility
//----------------------------------------------------------------------------
std::string GetXMLString(XdmfConstString str)
{
  std::string value = std::string(str);
  free((void*)str);
  return value;
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
XdmfSteeringParser::XdmfSteeringParser()
{
  this->ConfigDOM  = NULL;
  this->HasXdmf    = false;
  this->HasH5Part  = false;
  this->HasNetCDF  = false;
  this->HasTable  = false;
}
//----------------------------------------------------------------------------
XdmfSteeringParser::~XdmfSteeringParser()
{
  if (this->ConfigDOM) delete this->ConfigDOM;
  this->ConfigDOM = NULL;
}
//----------------------------------------------------------------------------
int XdmfTopologyToVTKDataSetType(XdmfTopology &topology) 
{
  if (topology.GetClass() == XDMF_UNSTRUCTURED ) 
    {
    return VTK_UNSTRUCTURED_GRID;
    } 
  XdmfInt32 topologyType = topology.GetTopologyType();
  if (topologyType == XDMF_2DSMESH || topologyType == XDMF_3DSMESH )
    { 
    return VTK_STRUCTURED_GRID; 
    }
  else if (topologyType == XDMF_2DCORECTMESH ||
    topologyType == XDMF_3DCORECTMESH)
    {
#ifdef USE_IMAGE_DATA
    return VTK_IMAGE_DATA;
#else
    return VTK_UNIFORM_GRID;
#endif
    }
  else if (topologyType == XDMF_2DRECTMESH || topologyType == XDMF_3DRECTMESH)
    {
    return VTK_RECTILINEAR_GRID;
    }
  return -1;
}
//----------------------------------------------------------------------------
int XdmfSteeringParser::Parse(const char *configFilePath)
{
  bool fail = false;
  if (this->ConfigDOM) delete this->ConfigDOM;
  this->ConfigDOM = new XdmfDOM();

  // Fill configDOM
  XdmfDebug("Parsing file: " << configFilePath);
  if (this->ConfigDOM->Parse(configFilePath) != XDMF_SUCCESS) {
    XdmfErrorMessage("Unable to parse xml configuration");
    fail = true;
  }

  // Visualization
  XdmfXmlNode vizNode = this->ConfigDOM->FindElement("Visualization");
  if (!vizNode) {
    XdmfErrorMessage("No Visualization node in xml configuration");
    fail = true;
  }

  if (fail) {
    delete this->ConfigDOM;
    this->ConfigDOM = NULL;
    return(XDMF_FAIL);
  }

  //////////////////////////////////////////////////////////////////////
  // NetCDF
  //////////////////////////////////////////////////////////////////////
  XdmfXmlNode netCDFNode = this->ConfigDOM->FindElement("NetCDF", 0, vizNode);
  this->HasNetCDF = (netCDFNode!=NULL);

  //////////////////////////////////////////////////////////////////////
  // Table
  //////////////////////////////////////////////////////////////////////
  XdmfXmlNode graphNode = this->ConfigDOM->FindElement("Table", 0, vizNode);
  this->HasTable = (graphNode!=NULL);
  if (this->HasTable) {
    this->TableName = GetXMLString(this->ConfigDOM->GetAttribute(graphNode, "Name"));
    int numA = this->ConfigDOM->GetNumberOfChildren(graphNode);
    for (int i=0; i<numA; i++) {
      XdmfXmlNode subNode = this->ConfigDOM->GetChild(i,graphNode);
      TableStrings.push_back(GetXMLString(this->ConfigDOM->GetAttribute(subNode,"Name")));
    }
  }
  
  //////////////////////////////////////////////////////////////////////
  // H5Part
  //////////////////////////////////////////////////////////////////////
  XdmfXmlNode h5PartNode = this->ConfigDOM->FindElement("H5Part", 0, vizNode);
  this->HasH5Part = (h5PartNode!=NULL);
  if (this->HasH5Part) {
    XdmfXmlNode stepNode = this->ConfigDOM->FindElement("Step", 0, h5PartNode);
    std::string  step = GetXMLString(this->ConfigDOM->GetAttribute(stepNode, "Name"));
    this->H5PartStrings.push_back(step);
    XdmfXmlNode XNode = this->ConfigDOM->FindElement("Xarray", 0, h5PartNode);
    std::string     x = GetXMLString(this->ConfigDOM->GetAttribute(XNode, "Name"));
    this->H5PartStrings.push_back(x);
    XdmfXmlNode YNode = this->ConfigDOM->FindElement("Yarray", 0, h5PartNode);
    std::string     y = GetXMLString(this->ConfigDOM->GetAttribute(YNode, "Name"));
    this->H5PartStrings.push_back(y);
    XdmfXmlNode ZNode = this->ConfigDOM->FindElement("Zarray", 0, h5PartNode);
    std::string     z = GetXMLString(this->ConfigDOM->GetAttribute(ZNode, "Name"));
    this->H5PartStrings.push_back(z);
  }

  //////////////////////////////////////////////////////////////////////
  // Xdmf
  //////////////////////////////////////////////////////////////////////
  XdmfXmlNode xdmfNode = this->ConfigDOM->FindElement("Xdmf", 0, vizNode);
  this->HasXdmf = (xdmfNode!=NULL);
/*
        <Topology TopologyType="Polyvertex">
          <DataItem Format="XML" Dimensions="1 1" NumberType="Float">0</DataItem>
        </Topology>
        <Geometry GeometryType="XYZ">
          <DataItem Format="XML" Dimensions="3" NumberType="Float" Precision="4">0.0 0.0 0.0</DataItem>
        </Geometry>
*/

  if (this->HasXdmf) {
    int numberOfGrids = this->ConfigDOM->FindNumberOfElements("Grid", xdmfNode);

    for (int currentGridIndex=0; currentGridIndex < numberOfGrids; currentGridIndex++) {
      XdmfXmlNode gridNode = this->ConfigDOM->FindElement("Grid", currentGridIndex, xdmfNode);
      XdmfString  gridName = (XdmfString) this->ConfigDOM->GetAttribute(gridNode, "Name");
      std::string gname = gridName;
      if (gridName) {
        free(gridName);
      } 
      xmfSteeringConfigGrid &grid = this->SteeringConfig[gname];
      //
      // Store the TopologyType as a VTK dataset Type because the Widget controls make use of it
      // when there are NULL blocks
      //
      XdmfTopology  topology;
      XdmfXmlNode   topologyNode    = this->ConfigDOM->FindElement("Topology", 0, gridNode);
      XdmfString    topologyTypeStr = (XdmfString) this->ConfigDOM->GetAttribute(topologyNode, "TopologyType");
      topology.SetTopologyTypeFromString(topologyTypeStr);
      free(topologyTypeStr);
      this->GridTypeMap[currentGridIndex] = XdmfTopologyToVTKDataSetType(topology);
      //
      int numberOfAttributes = this->ConfigDOM->FindNumberOfElements("Attribute", gridNode);
      for (int currentAttributeIndex=0; currentAttributeIndex < numberOfAttributes; currentAttributeIndex++) {
        XdmfXmlNode attributeNode = this->ConfigDOM->FindElement("Attribute", currentAttributeIndex, gridNode);
        XdmfString  attributeName = (XdmfString) this->ConfigDOM->GetAttribute(attributeNode, "Name");
        XdmfXmlNode attributeDINode;
        std::string attributeMapName;
        if (attributeName) {
          attributeMapName = attributeName;
          free(attributeName);
        } else {
          XdmfConstString attributePath = this->ConfigDOM->GetCData(this->ConfigDOM->FindElement("DataItem", 0, attributeNode));
          vtksys::RegularExpression reName("/([^/]*)$");
          reName.find(attributePath);
          attributeMapName = reName.match(1);
        }
        attributeDINode = this->ConfigDOM->FindElement("DataItem", 0, attributeNode);
        if (attributeDINode) {
          XdmfXmlNode multiDINode = this->ConfigDOM->FindElement("DataItem", 0, attributeDINode);
          if (!multiDINode) {
            XdmfConstString attributePath = this->ConfigDOM->GetCData(attributeDINode);
            grid.attributeConfig[attributeMapName].hdfPath = attributePath;
          } else {
            // If the data item node is composed is composed of multiple sub-items, use the attribute name for
            // matching disabled/enabled arrays in the steerer
            grid.attributeConfig[attributeMapName].hdfPath = attributeMapName;
          }
        }
      }
    }

    this->XdmfXmlDoc = this->ConfigDOM->Serialize(xdmfNode);

  }


  //////////////////////////////////////////////////////////////////////
  // Interaction
  XdmfXmlNode interactionNode = this->ConfigDOM->FindElement("Interaction");

  // Create XML that ParaView can use to create proxy objects and GUI controls
  this->CreateParaViewProxyXML(interactionNode);

  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
int XdmfSteeringParser::CreateParaViewProxyXML(XdmfXmlNode interactionNode)
{
  std::ostringstream xmlstring;
  std::string hintstring;
  //
  xmlstring << "<ServerManagerConfiguration>" << std::endl;
  xmlstring << "<ProxyGroup name=\"icarus_helpers\">" << std::endl;
  xmlstring << "<SourceProxy name=\"DsmProxyHelper\" class=\"vtkDsmProxyHelper\">" << std::endl;
  // The Helper needs a DSM manager
  xmlstring << "<ProxyProperty name=\"DsmManager\" command=\"SetDsmManager\"/>" << std::endl;
  // The Helper needs a Steering Writer
  xmlstring << "<ProxyProperty name=\"SteeringWriter\" command=\"SetSteeringWriter\"/>" << std::endl;
  
  // We need an input to the helper so that dependent domains can be updated (field array lists)
  xmlstring << "<InputProperty name=\"Input\" command=\"SetInputConnection\">" << std::endl;
  xmlstring << "<ProxyGroupDomain name=\"groups\"> <Group name=\"sources\"/> <Group name=\"filters\"/> </ProxyGroupDomain>" << std::endl;
  xmlstring << "<DataTypeDomain name=\"input_type\"> <DataType value=\"vtkDataObject\"/> </DataTypeDomain>" << std::endl;
  xmlstring << "<InputArrayDomain name=\"input_array\"/>" << std::endl;
  xmlstring << "</InputProperty>" << std::endl;

  xmlstring << "<Property name=\"BlockTraffic\" command=\"BlockTraffic\"> </Property> " << std::endl;
  xmlstring << "<Property name=\"UnblockTraffic\" command=\"UnblockTraffic\"> </Property> " << std::endl;

/*
  // Add a transform so that widgets can fake/trigger updates
  xmlstring << "<ProxyProperty name=\"Transform\" command=\"SetTransform\">" << std::endl;
  xmlstring << "<ProxyGroupDomain name=\"groups\"> <Group name=\"transforms\"/> </ProxyGroupDomain>" << std::endl;
  xmlstring << "<ProxyListDomain name=\"proxy_list\"> <Proxy group=\"extended_sources\" name=\"Transform3\" /> </ProxyListDomain>" << std::endl;
  xmlstring << "</ProxyProperty>" << std::endl;
*/

  int numberOfCommandProperties = this->ConfigDOM->FindNumberOfElements("CommandProperty", interactionNode);
  for (int index=0; index < numberOfCommandProperties; index++) {
    XdmfXmlNode xnode = this->ConfigDOM->FindElement("CommandProperty", index, interactionNode);
    // make sure the CommandProperty uses vtkSIIntVectorProperty
    xmlSetProp(xnode, (xmlChar *)"si_class", (xmlChar *)"vtkSIProperty");
    // Set the Command to be ExecuteSteeringCommand + the name of the command
    std::string    name = GetXMLString(this->ConfigDOM->GetAttribute(xnode, "name"));
    std::string command = "ExecuteSteeringCommand" + name;
    xmlSetProp(xnode, (xmlChar *)"command", (xmlChar *)command.c_str());
    //
    xmlstring << this->ConfigDOM->Serialize(xnode) << std::endl;
  }

  int numberOfDataExportProperties = this->ConfigDOM->FindNumberOfElements("DataExportProperty", interactionNode);
  for (int index=0; index < numberOfDataExportProperties; index++) {
    XdmfXmlNode xnode = this->ConfigDOM->FindElement("DataExportProperty", index, interactionNode);
    // make sure the DataExportProperty property uses vtkSIStringVectorProperty
    xmlSetProp(xnode, (xmlChar *)"si_class", (xmlChar *)"vtkSIStringVectorProperty");
    // Set the Command to be SetSteeringArray + the name of the command
    std::string    name = GetXMLString(this->ConfigDOM->GetAttribute(xnode, "name"));
    std::string command = "SetSteeringArray" + name;
    xmlSetProp(xnode, (xmlChar *)"command", (xmlChar *)command.c_str());
    //
    //vtksys::SystemTools::ReplaceString(xml, "<DataExportProperty", "<StringVectorProperty");
    //vtksys::SystemTools::ReplaceString(xml, "</DataExportProperty>", 
    //  // none_string=\"No Data Write\"
    //  "<ArrayListDomain name=\"array_list\" > <RequiredProperties> <Property name=\"Input\" function=\"Input\"/> </RequiredProperties> </ArrayListDomain> "
    //  "<FieldDataDomain name=\"field_list\" > <RequiredProperties> <Property name=\"Input\" function=\"Input\"/> </RequiredProperties> </FieldDataDomain> "
    //  "</StringVectorProperty>"
    //);
    xmlstring << this->ConfigDOM->Serialize(xnode) << std::endl;
  }

  int numberOfStringProperties = this->ConfigDOM->FindNumberOfElements("StringVectorProperty", interactionNode);
  for (int index=0; index < numberOfStringProperties; index++) {
    XdmfXmlNode    xnode = this->ConfigDOM->FindElement("StringVectorProperty", index, interactionNode);
    std::string      xml = this->ConfigDOM->Serialize(xnode);
    std::string     name = GetXMLString(this->ConfigDOM->GetAttribute(xnode, "name"));
    vtksys::SystemTools::ReplaceString(xml, "SetSteeringString", std::string("SetSteeringString" + name).c_str());
    xmlstring << xml << std::endl;
  }

  int numberOfIntVectorProperties = this->ConfigDOM->FindNumberOfElements("IntVectorProperty", interactionNode);
  for (int index=0; index < numberOfIntVectorProperties; index++) {
    XdmfXmlNode    xnode = this->ConfigDOM->FindElement("IntVectorProperty", index, interactionNode);
    std::string      xml = this->ConfigDOM->Serialize(xnode);
    std::string     name = GetXMLString(this->ConfigDOM->GetAttribute(xnode, "name"));
    vtksys::SystemTools::ReplaceString(xml, "SetSteeringValueInt", std::string("SetSteeringValueInt" + name).c_str());
    vtksys::SystemTools::ReplaceString(xml, "GetSteeringValueInt", std::string("GetSteeringValueInt" + name).c_str());
    vtksys::SystemTools::ReplaceString(xml, "GetSteeringScalarInt", std::string("GetSteeringScalarInt" + name).c_str());
    vtksys::SystemTools::ReplaceString(xml, "SetSteeringType", std::string("SetSteeringType" + name).c_str());
    xmlstring << xml << std::endl;
    hintstring += this->BuildWidgetHints(name.c_str(), xnode);
  }

  int numberOfDoubleVectorProperties = this->ConfigDOM->FindNumberOfElements("DoubleVectorProperty", interactionNode);
  for (int index=0; index < numberOfDoubleVectorProperties; index++) {
    XdmfXmlNode    xnode = this->ConfigDOM->FindElement("DoubleVectorProperty", index, interactionNode);
    std::string      xml = this->ConfigDOM->Serialize(xnode);
    std::string     name = GetXMLString(this->ConfigDOM->GetAttribute(xnode, "name"));
    vtksys::SystemTools::ReplaceString(xml, "SetSteeringValueDouble", std::string("SetSteeringValueDouble" + name).c_str());
    vtksys::SystemTools::ReplaceString(xml, "GetSteeringValueDouble", std::string("GetSteeringValueDouble" + name).c_str());
    vtksys::SystemTools::ReplaceString(xml, "GetSteeringScalarDouble", std::string("GetSteeringScalarDouble" + name).c_str());
    xmlstring << xml << std::endl;
    hintstring += this->BuildWidgetHints(name.c_str(), xnode);
  }

  xmlstring << "<Hints>" << std::endl;
  xmlstring << "<Property name=\"DsmManager\" show=\"0\"/>" << std::endl;
  xmlstring << "<Property name=\"SteeringWriter\" show=\"0\"/>" << std::endl;
  xmlstring << "<Property name=\"Transform\" show=\"0\"/>" << std::endl;
  xmlstring << "<Property name=\"Time\" show=\"0\"/>" << std::endl;
  xmlstring << "<Property name=\"TimeRange\" show=\"0\"/>" << std::endl;
  xmlstring << hintstring.c_str() << std::endl;
  xmlstring << "</Hints>" << std::endl;
  xmlstring << "</SourceProxy>" << std::endl;
  xmlstring << "</ProxyGroup>" << std::endl;
  xmlstring << "</ServerManagerConfiguration>" << std::endl << std::ends;

  this->HelperProxyString = xmlstring.str().c_str();

  return 1;
}
//----------------------------------------------------------------------------
std::string XdmfSteeringParser::BuildWidgetHints(XdmfConstString name, XdmfXmlNode propertyNode)
{
  std::string hints;
  std::ostringstream hintstring;
  XdmfXmlNode widgetNode, gridNode, initNode;
  XdmfXmlNode hintsNode = this->ConfigDOM->FindElement("Hints", 0, propertyNode);
  if (hintsNode && (widgetNode = this->ConfigDOM->FindElement("WidgetControl", 0, hintsNode))!=NULL) {
    SteeringGUIWidgetInfo &info = this->SteeringWidgetMap[name];
    //
    XdmfString wname = (XdmfString) this->ConfigDOM->GetAttribute(widgetNode, "name");
    info.WidgetType = wname;
    //
    if ((gridNode=this->ConfigDOM->FindElement("AssociatedGrid", 0, hintsNode))!=NULL) {
      info.AssociatedGrid = GetXMLString(this->ConfigDOM->GetAttribute(gridNode, "name"));
    }
    if ((initNode=this->ConfigDOM->FindElement("Initialization", 0, hintsNode))!=NULL) {
      info.Initialization = GetXMLString(this->ConfigDOM->GetAttribute(initNode, "name"));
    }
    std::string label = GetXMLString(this->ConfigDOM->GetAttribute(propertyNode, "label"));
    hintstring << "<PropertyGroup type=\"" << wname << "\" label=\"" << label << "\">" << std::endl;
    if (!strcmp(wname,"Handle") || !strcmp(wname,"Point")) {
      hintstring << "<Property function=\"WorldPosition\" ";
    }
    else if (!strcmp(wname,"Box")) {
      hintstring << "<Property function=\"Position\" ";
    }
    hintstring << "name=\"" << name << "\" />" << std::endl;
    hintstring << "</PropertyGroup>" << std::endl;
    if (wname) free(wname);

  }
  return hintstring.str();
}
//----------------------------------------------------------------------------
int XdmfSteeringParser::GetGridTypeForBlock(int blockindex)
{
  return GridTypeMap[blockindex];
};
