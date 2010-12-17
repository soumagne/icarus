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
    XdmfInt32 topologyType = topology.GetTopologyType();
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
        XdmfConstString attributePath = this->ConfigDOM->GetCData(attributeDINode);
        grid.attributeConfig[attributeMapName].hdfPath = attributePath;
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
  // Dummy input
  xmlstring << "<InputProperty name=\"Input\" command=\"SetInputConnection\">" << std::endl;
  xmlstring << "<ProxyGroupDomain name=\"groups\"> <Group name=\"sources\"/> <Group name=\"filters\"/> </ProxyGroupDomain>" << std::endl;
  xmlstring << "<DataTypeDomain name=\"input_type\"> <DataType value=\"vtkPointSet\"/> </DataTypeDomain>" << std::endl;
  xmlstring << "</InputProperty>" << std::endl;
/*
  // Add a transform so that widgets can fake/trigger updates
  xmlstring << "<ProxyProperty name=\"Transform\" command=\"SetTransform\">" << std::endl;
  xmlstring << "<ProxyGroupDomain name=\"groups\"> <Group name=\"transforms\"/> </ProxyGroupDomain>" << std::endl;
  xmlstring << "<ProxyListDomain name=\"proxy_list\"> <Proxy group=\"extended_sources\" name=\"Transform3\" /> </ProxyListDomain>" << std::endl;
  xmlstring << "</ProxyProperty>" << std::endl;
*/
  int numberOfIntVectorProperties = this->ConfigDOM->FindNumberOfElements("IntVectorProperty", interactionNode);
  for (int currentIVPIndex=0; currentIVPIndex < numberOfIntVectorProperties; currentIVPIndex++) {
    XdmfXmlNode  ivpNode = this->ConfigDOM->FindElement("IntVectorProperty", currentIVPIndex, interactionNode);
    std::string      xml = this->ConfigDOM->Serialize(ivpNode);
    XdmfConstString name = this->ConfigDOM->GetAttribute(ivpNode, "name");
    vtksys::SystemTools::ReplaceString(xml, "@@@", name);
    xmlstring << xml << std::endl;
    hintstring += this->BuildWidgetHints(name, ivpNode);
  }

  int numberOfDoubleVectorProperties = this->ConfigDOM->FindNumberOfElements("DoubleVectorProperty", interactionNode);
  for (int currentDVPIndex=0; currentDVPIndex < numberOfDoubleVectorProperties; currentDVPIndex++) {
    XdmfXmlNode  dvpNode = this->ConfigDOM->FindElement("DoubleVectorProperty", currentDVPIndex, interactionNode);
    std::string      xml = this->ConfigDOM->Serialize(dvpNode);
    XdmfConstString name = this->ConfigDOM->GetAttribute(dvpNode, "name");
    vtksys::SystemTools::ReplaceString(xml, "@@@", name);
    xmlstring << xml << std::endl;
    hintstring += this->BuildWidgetHints(name, dvpNode);
  }

  xmlstring << "<Hints>" << std::endl;
  xmlstring << "<Property name=\"DSMManager\" show=\"0\"/>" << std::endl;
  xmlstring << "<Property name=\"Transform\" show=\"0\"/>" << std::endl;
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
