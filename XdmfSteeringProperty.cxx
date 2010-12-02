/*=========================================================================

  Project                 : Icarus
  Module                  : XdmfSteeringProperty.cxx

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
#include "XdmfSteeringProperty.h"

#include "XdmfSteeringDomain.h"

#include <vector>
#include <map>
#include <string>

struct XdmfSteeringPropertyInternals
{
  typedef std::map<std::string, XdmfSteeringDomain*> DomainMap;
  DomainMap Domains;

  typedef std::map<std::string, XdmfSteeringProperty*> PropertyMap;
  PropertyMap SubProperties; // May need it but not used for now

  typedef std::vector<XdmfSteeringDomain*> DependentsVector;
  DependentsVector Dependents; // May need it but not used for now
};

//---------------------------------------------------------------------------
XdmfSteeringProperty::XdmfSteeringProperty()
{
  this->Command = 0;
  this->PInternals = new XdmfSteeringPropertyInternals;
  this->XMLName = 0;
  this->XMLLabel = 0;
  this->Documentation = 0;
}

//---------------------------------------------------------------------------
XdmfSteeringProperty::~XdmfSteeringProperty()
{
  this->SetCommand(0);
  delete this->PInternals;
  this->SetXMLName(0);
  this->SetXMLLabel(0);
  this->SetDocumentation(0);
}

//---------------------------------------------------------------------------
void XdmfSteeringProperty::AddDomain(const char* name, XdmfSteeringDomain* domain)
{
  // Check if the proxy already exists. If it does, we will
  // replace it
  XdmfSteeringPropertyInternals::DomainMap::iterator it =
    this->PInternals->Domains.find(name);

  if (it != this->PInternals->Domains.end())
    {
    XdmfDebug("Domain " << name  << " already exists. Replacing");
    }

  this->PInternals->Domains[name] = domain;
}

//---------------------------------------------------------------------------
XdmfSteeringDomain* XdmfSteeringProperty::GetDomain(const char* name)
{
  XdmfSteeringPropertyInternals::DomainMap::iterator it =
    this->PInternals->Domains.find(name);

  if (it == this->PInternals->Domains.end())
    {
    return 0;
    }

  return it->second;
}

//---------------------------------------------------------------------------
unsigned int XdmfSteeringProperty::GetNumberOfDomains()
{
  return this->PInternals->Domains.size();
}

