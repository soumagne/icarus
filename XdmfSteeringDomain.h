/*=========================================================================

  Project                 : Icarus
  Module                  : XdmfSteeringDomain.h

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

#ifndef __XdmfSteeringDomain_h
#define __XdmfSteeringDomain_h

#include "XdmfObject.h"

class XdmfSteeringProperty;

enum XdmfSteeringDomainType
{
  XDMF_STEERING_UNDEFINED_DOMAIN,
  XDMF_STEERING_BOOLEAN_DOMAIN,
  XDMF_STEERING_INT_RANGE_DOMAIN,
  XDMF_STEERING_DOUBLE_RANGE_DOMAIN,
  XDMF_STEERING_ENUMERATION_DOMAIN
};

class XDMF_EXPORT XdmfSteeringDomain : public XdmfObject
{
public:
  XdmfSteeringDomain();
  ~XdmfSteeringDomain();

  // Description:
  // A XdmfSteeringProperty is often defined with a default value in the
  // XML itself. However, many times, the default value must be determined
  // at run time. To facilitate this, domains can override this method
  // to compute and set the default value for the property.
  // Note that unlike the compile-time default values, the
  // application must explicitly call this method to initialize the
  // property.
  // Returns 1 if the domain updated the property.
  // Default implementation does nothing.
  virtual int SetDefaultValues(XdmfSteeringProperty*) {return 0; };

  // Description:
  // Assigned by the XML parser. The name assigned in the XML
  // configuration. Can be used to figure out the origin of the
  // domain.
  XdmfGetStringMacro(XMLName);

  // Description:
  // Assigned by the domain.
  XdmfGetValueMacro(DomainType, XdmfSteeringDomainType);

protected:
//BTX
  friend class XdmfSteeringProperty;
//ETX

  char* XMLName;

  // Description:
  // Assigned by the XML parser. The name assigned in the XML
  // configuration. Can be used to figure out the origin of the
  // domain.
  XdmfSetStringMacro(XMLName);

  XdmfSteeringDomainType DomainType;

  // Description:
  // Assigned by the domain.
  XdmfSetValueMacro(DomainType, XdmfSteeringDomainType);
};

#endif
