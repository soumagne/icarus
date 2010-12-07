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
  this->SteeringConfig = NULL;
}
//----------------------------------------------------------------------------
XdmfSteeringParser::~XdmfSteeringParser()
{
  if (this->ConfigDOM) delete this->ConfigDOM;
  this->ConfigDOM = NULL;
  this->DeleteConfig();
}
//----------------------------------------------------------------------------
void XdmfSteeringParser::DeleteConfig()
{
  if (this->SteeringConfig) {
    for (int i = 0; i < this->SteeringConfig->numberOfGrids; i++) {
      delete []this->SteeringConfig->gridConfig[i].attributeConfig;
    }
    delete []this->SteeringConfig->gridConfig;
    delete this->SteeringConfig;
  }
  this->SteeringConfig = NULL;
}
//----------------------------------------------------------------------------
int XdmfSteeringParser::Parse(const char *configFilePath)
{
  XdmfXmlNode domainNode;
  int numberOfGrids;

  XdmfXmlNode interactionNode;

  if (this->ConfigDOM) delete this->ConfigDOM;
  this->ConfigDOM = new XdmfDOM();
  this->DeleteConfig();

  // Fill configDOM
  XdmfDebug("Parsing file: " << configFilePath);
  if (this->ConfigDOM->Parse(configFilePath) != XDMF_SUCCESS) {
    XdmfErrorMessage("Unable to parse xml steering config file");
    delete this->ConfigDOM;
    this->ConfigDOM = NULL;
    return(XDMF_FAIL);
  }

  this->SteeringConfig = new xmfSteeringConfig;
  //////////////////////////////////////////////////////////////////////
  // Interaction
  interactionNode = this->ConfigDOM->FindElement("Interaction");

  this->CreateProxyXML(interactionNode);

  //////////////////////////////////////////////////////////////////////
  // Domain
  domainNode = this->ConfigDOM->FindElement("Domain");
  numberOfGrids = this->ConfigDOM->FindNumberOfElements("Grid", domainNode);
  this->SteeringConfig->numberOfGrids = numberOfGrids;
  this->SteeringConfig->gridConfig = new xmfSteeringConfigGrid[numberOfGrids];

  for (int currentGridIndex=0; currentGridIndex < numberOfGrids; currentGridIndex++) {
    XdmfXmlNode gridNode = this->ConfigDOM->FindElement("Grid", currentGridIndex, domainNode);
    XdmfString  gridName = (XdmfString) this->ConfigDOM->GetAttribute(gridNode, "Name");
    if (gridName) {
      this->SteeringConfig->gridConfig[currentGridIndex].gridName = gridName;
      free(gridName);
    } else {
      this->SteeringConfig->gridConfig[currentGridIndex].gridName = "Undefined Grid";
    }
    int numberOfAttributes = this->ConfigDOM->FindNumberOfElements("Attribute", gridNode);
    this->SteeringConfig->gridConfig[currentGridIndex].isEnabled = true;
    this->SteeringConfig->gridConfig[currentGridIndex].numberOfAttributes = numberOfAttributes;

    this->SteeringConfig->gridConfig[currentGridIndex].attributeConfig = new xmfSteeringConfigAttribute[numberOfAttributes];
    for(int currentAttributeIndex=0; currentAttributeIndex < numberOfAttributes; currentAttributeIndex++) {
      XdmfXmlNode attributeNode = this->ConfigDOM->FindElement("Attribute", currentAttributeIndex, gridNode);
      XdmfString  attributeName = (XdmfString) this->ConfigDOM->GetAttribute(attributeNode, "Name");
      XdmfXmlNode attributeDINode;
      if (attributeName) {
        this->SteeringConfig->gridConfig[currentGridIndex].attributeConfig[currentAttributeIndex].attributeName = attributeName;
        free(attributeName);
      } else {
        XdmfConstString attributePath = this->ConfigDOM->GetCData(this->ConfigDOM->FindElement("DataItem", 0, attributeNode));
        vtksys::RegularExpression reName("/([^/]*)$");
        reName.find(attributePath);
        this->SteeringConfig->gridConfig[currentGridIndex].attributeConfig[currentAttributeIndex].attributeName = reName.match(1).c_str();
      }
      attributeDINode = this->ConfigDOM->FindElement("DataItem", 0, attributeNode);
      if (attributeDINode) {
        XdmfConstString attributePath = this->ConfigDOM->GetCData(attributeDINode);
        this->SteeringConfig->gridConfig[currentGridIndex].attributeConfig[currentAttributeIndex].hdfPath = attributePath;
      }

      this->SteeringConfig->gridConfig[currentGridIndex].attributeConfig[currentAttributeIndex].isEnabled = true;
    }
  }

  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
int XdmfSteeringParser::CreateProxyXML(XdmfXmlNode interactionNode)
{
  std::ostringstream xmlstring;
  //
  xmlstring << "<ServerManagerConfiguration>" << std::endl;
  xmlstring << "<ProxyGroup name=\"icarus_helpers\">" << std::endl;
  xmlstring << "<Proxy name=\"DSMProxyHelper\" class=\"vtkDSMProxyHelper\">" << std::endl;
  // The Helper needs a DSM manager
  xmlstring << "<ProxyProperty name=\"DSMManager\" command=\"SetDSMManager\">" << std::endl;
  xmlstring << "  <ProxyGroupDomain name=\"groups\">" << std::endl;
  xmlstring << "    <Group name=\"icarus_helpers\"/>" << std::endl;;
  xmlstring << "  </ProxyGroupDomain>" << std::endl;
  xmlstring << "  <ProxyListDomain name=\"proxy_list\">" << std::endl;
  xmlstring << "    <Proxy group=\"icarus_helpers\" name=\"DSMManager\" />" << std::endl;
  xmlstring << "  </ProxyListDomain>" << std::endl;
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
  std::cout << xmlstring.str().c_str();

  vtkProcessModule::InitializeInterpreter(DSMProxyHelperInit);
  vtkSMObject::GetProxyManager()->LoadConfigurationXML(xmlstring.str().c_str());

  return 1;
}
//----------------------------------------------------------------------------
