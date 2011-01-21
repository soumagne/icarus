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
#include "XdmfSteeringParser.h"
//
#include "XdmfDOM.h"
#include "XdmfTopology.h"
//
#include <vtksys/RegularExpression.hxx>
#include <vtksys/SystemTools.hxx>
#include <string>
#include <iostream>
#include <cstdlib>
//
#include "vtkType.h"
//
//----------------------------------------------------------------------------
XdmfSteeringParser::XdmfSteeringParser()
{
  this->ConfigDOM  = NULL;
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
  XdmfXmlNode domainNode;
  int numberOfGrids;

  XdmfXmlNode interactionNode;

  if (this->ConfigDOM) delete this->ConfigDOM;
  this->ConfigDOM = new XdmfDOM();

  // Fill configDOM
  XdmfDebug("Parsing file: " << configFilePath);
  if (this->ConfigDOM->Parse(configFilePath) != XDMF_SUCCESS) {
    XdmfErrorMessage("Unable to parse xml steering config file");
    delete this->ConfigDOM;
    this->ConfigDOM = NULL;
    return(XDMF_FAIL);
  }

  //////////////////////////////////////////////////////////////////////
  // Interaction
  interactionNode = this->ConfigDOM->FindElement("Interaction");

  // Create XML that ParaView can use to create proxy objects and GUI controls
  this->CreateParaViewProxyXML(interactionNode);

  //////////////////////////////////////////////////////////////////////
  // Domain
  domainNode = this->ConfigDOM->FindElement("Domain");
  numberOfGrids = this->ConfigDOM->FindNumberOfElements("Grid", domainNode);

  for (int currentGridIndex=0; currentGridIndex < numberOfGrids; currentGridIndex++) {
    XdmfXmlNode gridNode = this->ConfigDOM->FindElement("Grid", currentGridIndex, domainNode);
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
  xmlstring << "<SourceProxy name=\"DSMProxyHelper\" class=\"vtkDSMProxyHelper\">" << std::endl;
  // The Helper needs a DSM manager
  xmlstring << "<ProxyProperty name=\"DSMManager\" command=\"SetDSMManager\"/>" << std::endl;
  // The Helper needs a Steering Writer
  xmlstring << "<ProxyProperty name=\"SteeringWriter\" command=\"SetSteeringWriter\"/>" << std::endl;
  
  // We need an input to the helper so that dependent domains can be updated (field array lists)
  xmlstring << "<InputProperty name=\"Input\" command=\"SetInputConnection\">" << std::endl;
  xmlstring << "<ProxyGroupDomain name=\"groups\"> <Group name=\"sources\"/> <Group name=\"filters\"/> </ProxyGroupDomain>" << std::endl;
  xmlstring << "<DataTypeDomain name=\"input_type\"> <DataType value=\"vtkDataObject\"/> </DataTypeDomain>" << std::endl;
  xmlstring << "<InputArrayDomain name=\"input_array\"/>" << std::endl;
  xmlstring << "</InputProperty>" << std::endl;
/*
  // Add a transform so that widgets can fake/trigger updates
  xmlstring << "<ProxyProperty name=\"Transform\" command=\"SetTransform\">" << std::endl;
  xmlstring << "<ProxyGroupDomain name=\"groups\"> <Group name=\"transforms\"/> </ProxyGroupDomain>" << std::endl;
  xmlstring << "<ProxyListDomain name=\"proxy_list\"> <Proxy group=\"extended_sources\" name=\"Transform3\" /> </ProxyListDomain>" << std::endl;
  xmlstring << "</ProxyProperty>" << std::endl;
*/
  int numberOfCommandProperties = this->ConfigDOM->FindNumberOfElements("CommandProperty", interactionNode);
  for (int index=0; index < numberOfCommandProperties; index++) {
    XdmfXmlNode    xnode = this->ConfigDOM->FindElement("CommandProperty", index, interactionNode);
    std::string      xml = this->ConfigDOM->Serialize(xnode);
    std::string     name = this->ConfigDOM->GetAttribute(xnode, "name");
    vtksys::SystemTools::ReplaceString(xml, "ExecuteSteeringCommand", std::string("ExecuteSteeringCommand" + name).c_str());
    xmlstring << xml << std::endl;
  }

  int numberOfDataArrayProperties = this->ConfigDOM->FindNumberOfElements("DataArrayProperty", interactionNode);
  for (int index=0; index < numberOfDataArrayProperties; index++) {
    XdmfXmlNode    xnode = this->ConfigDOM->FindElement("DataArrayProperty", index, interactionNode);
    std::string      xml = this->ConfigDOM->Serialize(xnode);
    std::string     name = this->ConfigDOM->GetAttribute(xnode, "name");
    vtksys::SystemTools::ReplaceString(xml, "<DataArrayProperty", "<StringVectorProperty");
    vtksys::SystemTools::ReplaceString(xml, "</DataArrayProperty>", 
      // none_string=\"No Data Write\"
      "<ArrayListDomain name=\"array_list\" > <RequiredProperties> <Property name=\"Input\" function=\"Input\"/> </RequiredProperties> </ArrayListDomain> "
      "<FieldDataDomain name=\"field_list\" > <RequiredProperties> <Property name=\"Input\" function=\"Input\"/> </RequiredProperties> </FieldDataDomain> "
      "</StringVectorProperty>"
    );
    vtksys::SystemTools::ReplaceString(xml, "SetSteeringArray", std::string("SetSteeringArray" + name).c_str());
    xmlstring << xml << std::endl;
  }

  int numberOfStringProperties = this->ConfigDOM->FindNumberOfElements("StringVectorProperty", interactionNode);
  for (int index=0; index < numberOfStringProperties; index++) {
    XdmfXmlNode    xnode = this->ConfigDOM->FindElement("StringVectorProperty", index, interactionNode);
    std::string      xml = this->ConfigDOM->Serialize(xnode);
    std::string     name = this->ConfigDOM->GetAttribute(xnode, "name");
    vtksys::SystemTools::ReplaceString(xml, "SetSteeringString", std::string("SetSteeringString" + name).c_str());
    xmlstring << xml << std::endl;
  }

  int numberOfIntVectorProperties = this->ConfigDOM->FindNumberOfElements("IntVectorProperty", interactionNode);
  for (int index=0; index < numberOfIntVectorProperties; index++) {
    XdmfXmlNode    xnode = this->ConfigDOM->FindElement("IntVectorProperty", index, interactionNode);
    std::string      xml = this->ConfigDOM->Serialize(xnode);
    std::string     name = this->ConfigDOM->GetAttribute(xnode, "name");
    vtksys::SystemTools::ReplaceString(xml, "SetSteeringValueInt", std::string("SetSteeringValueInt" + name).c_str());
    vtksys::SystemTools::ReplaceString(xml, "GetSteeringValueInt", std::string("GetSteeringValueInt" + name).c_str());
    vtksys::SystemTools::ReplaceString(xml, "SetSteeringType", std::string("SetSteeringType" + name).c_str());
    xmlstring << xml << std::endl;
    hintstring += this->BuildWidgetHints(name.c_str(), xnode);
  }

  int numberOfDoubleVectorProperties = this->ConfigDOM->FindNumberOfElements("DoubleVectorProperty", interactionNode);
  for (int index=0; index < numberOfDoubleVectorProperties; index++) {
    XdmfXmlNode    xnode = this->ConfigDOM->FindElement("DoubleVectorProperty", index, interactionNode);
    std::string      xml = this->ConfigDOM->Serialize(xnode);
    std::string     name = this->ConfigDOM->GetAttribute(xnode, "name");
    vtksys::SystemTools::ReplaceString(xml, "SetSteeringValueDouble", std::string("SetSteeringValueDouble" + name).c_str());
    vtksys::SystemTools::ReplaceString(xml, "GetSteeringValueDouble", std::string("GetSteeringValueDouble" + name).c_str());
    xmlstring << xml << std::endl;
    hintstring += this->BuildWidgetHints(name.c_str(), xnode);
  }

  xmlstring << "<Hints>" << std::endl;
  xmlstring << "<Property name=\"DSMManager\" show=\"0\"/>" << std::endl;
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
    XdmfConstString wname = this->ConfigDOM->GetAttribute(widgetNode, "name");
    info.WidgetType = wname;
    //
    if ((gridNode=this->ConfigDOM->FindElement("AssociatedGrid", 0, hintsNode))!=NULL) {
      info.AssociatedGrid = this->ConfigDOM->GetAttribute(gridNode, "name");
    }
    if ((initNode=this->ConfigDOM->FindElement("Initialization", 0, hintsNode))!=NULL) {
      info.Initialization = this->ConfigDOM->GetAttribute(initNode, "name");
    }
    XdmfConstString label = this->ConfigDOM->GetAttribute(propertyNode, "label");
    hintstring << "<PropertyGroup type=\"" << wname << "\" label=\"" << label << "\">" << std::endl;
    if (!strcmp(wname,"Handle") || !strcmp(wname,"Point")) {
      hintstring << "<Property function=\"WorldPosition\" ";
    }
    else if (!strcmp(wname,"Box")) {
      hintstring << "<Property function=\"Position\" ";
    }
    hintstring << "name=\"" << name << "\" />" << std::endl;
    hintstring << "</PropertyGroup>" << std::endl;
  }
  return hintstring.str();
}
//----------------------------------------------------------------------------
int XdmfSteeringParser::GetGridTypeForBlock(int blockindex)
{
  return GridTypeMap[blockindex];
};
