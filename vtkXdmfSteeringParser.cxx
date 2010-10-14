/*=========================================================================

  Project                 : Icarus
  Module                  : vtkXdmfSteeringParser.cxx

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
#include "vtkXdmfSteeringParser.h"

#include <XdmfDOM.h>
#include <vtksys/RegularExpression.hxx>
#include <iostream>
#include <cstdlib>

//----------------------------------------------------------------------------
vtkXdmfSteeringParser::vtkXdmfSteeringParser()
{
  this->ConfigDOM  = NULL;
  this->SteeringConfig = NULL;
}
//----------------------------------------------------------------------------
vtkXdmfSteeringParser::~vtkXdmfSteeringParser()
{
  if (this->ConfigDOM) delete this->ConfigDOM;
  this->ConfigDOM = NULL;
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
int vtkXdmfSteeringParser::Parse(const char *configFilePath)
{
  XdmfXmlNode domainNode;
  int numberOfGrids;

  if (this->ConfigDOM) delete this->ConfigDOM;
  this->ConfigDOM = new XdmfDOM();

  if (this->SteeringConfig) {
    for (int i = 0; i < this->SteeringConfig->numberOfGrids; i++) {
        delete []this->SteeringConfig->gridConfig[i].attributeConfig;
      }
    delete []this->SteeringConfig->gridConfig;
    delete this->SteeringConfig;
  }

  // Fill configDOM
  XdmfDebug("Parsing file: " << configFilePath);
  if (this->ConfigDOM->Parse(configFilePath) != XDMF_SUCCESS) {
    XdmfErrorMessage("Unable to parse xml steering config file");
    delete this->ConfigDOM;
    this->ConfigDOM = NULL;
    return(XDMF_FAIL);
  }

  this->SteeringConfig = new xmfSteeringConfig;
  //Find domain element
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
      if (attributeName) {
        this->SteeringConfig->gridConfig[currentGridIndex].attributeConfig[currentAttributeIndex].attributeName = attributeName;
        free(attributeName);
      } else {
        XdmfConstString attributePath = this->ConfigDOM->GetCData(this->ConfigDOM->FindElement("DataItem", 0, attributeNode));
        vtksys::RegularExpression reName("/([^/]*)$");
        reName.find(attributePath);
        this->SteeringConfig->gridConfig[currentGridIndex].attributeConfig[currentAttributeIndex].attributeName = reName.match(1).c_str();
      }
      this->SteeringConfig->gridConfig[currentGridIndex].attributeConfig[currentAttributeIndex].isEnabled = true;
    }
  }

  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
