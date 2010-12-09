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

#include <XdmfDOM.h>

#include <vtksys/RegularExpression.hxx>
#include <vtksys/SystemTools.hxx>
#include <string>
#include <iostream>
#include <cstdlib>

#include "vtkDSMProxyHelper.h"
#include "vtkProcessModule.h"
#include "vtkSMObject.h"
#include "vtkSMProxyManager.h"

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
    grid.isEnabled = true;
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
      grid.attributeConfig[attributeMapName].isEnabled = true;
    }
  }

  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
int XdmfSteeringParser::CreateParaViewProxyXML(XdmfXmlNode interactionNode)
{
  std::ostringstream xmlstring;
  //
  xmlstring << "<ServerManagerConfiguration>" << std::endl;
  xmlstring << "<ProxyGroup name=\"icarus_helpers\">" << std::endl;
  xmlstring << "<Proxy name=\"DSMProxyHelper\" class=\"vtkDSMProxyHelper\">" << std::endl;
  // The Helper needs a DSM manager
  xmlstring << "<ProxyProperty name=\"DSMManager\" command=\"SetDSMManager\"/>" << std::endl;
  // Dummy input
  xmlstring << "<InputProperty name=\"Input\" command=\"SetInputConnection\">" << std::endl;
  xmlstring << "<ProxyGroupDomain name=\"groups\"> <Group name=\"sources\"/> <Group name=\"filters\"/> </ProxyGroupDomain>" << std::endl;
  xmlstring << "<DataTypeDomain name=\"input_type\"> <DataType value=\"vtkPointSet\"/> </DataTypeDomain>" << std::endl;
  xmlstring << "</InputProperty>" << std::endl;
  // Add a transform so that widgets can fake/trigger updates
  xmlstring << "<ProxyProperty name=\"Transform\" command=\"SetTransform\">" << std::endl;
  xmlstring << "<ProxyGroupDomain name=\"groups\"> <Group name=\"transforms\"/> </ProxyGroupDomain>" << std::endl;
  xmlstring << "<ProxyListDomain name=\"proxy_list\"> <Proxy group=\"extended_sources\" name=\"Transform3\" /> </ProxyListDomain>" << std::endl;
  xmlstring << "</ProxyProperty>" << std::endl;

  int numberOfIntVectorProperties = this->ConfigDOM->FindNumberOfElements("IntVectorProperty", interactionNode);
  for (int currentIVPIndex=0; currentIVPIndex < numberOfIntVectorProperties; currentIVPIndex++) {
    XdmfXmlNode  ivpNode = this->ConfigDOM->FindElement("IntVectorProperty", currentIVPIndex, interactionNode);
    std::string      xml = this->ConfigDOM->Serialize(ivpNode);
    XdmfConstString name = this->ConfigDOM->GetAttribute(ivpNode, "name");
    vtksys::SystemTools::ReplaceString(xml, "@@@", name);
    xmlstring << xml << std::endl;
  }

  int numberOfDoubleVectorProperties = this->ConfigDOM->FindNumberOfElements("DoubleVectorProperty", interactionNode);
  for (int currentDVPIndex=0; currentDVPIndex < numberOfDoubleVectorProperties; currentDVPIndex++) {
    XdmfXmlNode  dvpNode = this->ConfigDOM->FindElement("DoubleVectorProperty", currentDVPIndex, interactionNode);
    std::string      xml = this->ConfigDOM->Serialize(dvpNode);
    XdmfConstString name = this->ConfigDOM->GetAttribute(dvpNode, "name");
    vtksys::SystemTools::ReplaceString(xml, "@@@", name);
    xmlstring << xml << std::endl;
  }

  xmlstring << "<Hints> <Property name=\"DSMManager\" show=\"0\"/> </Hints>" << std::endl;
  xmlstring << "</Proxy>" << std::endl;
  xmlstring << "</ProxyGroup>" << std::endl;
  xmlstring << "</ServerManagerConfiguration>" << std::endl << std::ends;

  // Register a constructor function for the DSMProxyHelper
  vtkProcessModule::InitializeInterpreter(DSMProxyHelperInit);
  // Pass the DSMProxyHelper XML into the proxy manager for use by NewProxy(...)
  vtkSMObject::GetProxyManager()->LoadConfigurationXML(xmlstring.str().c_str());

  return 1;
}
//----------------------------------------------------------------------------
