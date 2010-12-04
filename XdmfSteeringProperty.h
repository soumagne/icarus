/*=========================================================================

  Project                 : Icarus
  Module                  : XdmfSteeringProperty.h

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

#ifndef __XdmfSteeringProperty_h
#define __XdmfSteeringProperty_h

#include "XdmfObject.h"

class XdmfSteeringDomain;

struct XdmfSteeringPropertyInternals;

class XDMF_EXPORT XdmfSteeringProperty : public XdmfObject
{
public:
  XdmfSteeringProperty();
  ~XdmfSteeringProperty();

  // Description:
  // The command name used to set the value on the server object.
  // For example: SetThetaResolution
  XdmfSetStringMacro(Command);
  XdmfGetStringMacro(Command);

  // Description:
  // Returns a domain give a name.
  XdmfSteeringDomain* GetDomain(const char* name);

  // Description:
  // Returns the first domain which is of the specified type.
  XdmfSteeringDomain* FindDomain(const char* classname);

  // Description:
  // Returns the number of domains this property has. This can be
  // used to specify a valid index for GetDomain(index).
  unsigned int GetNumberOfDomains();

  // Description:
  // Properties can have one or more domains.
  void AddDomain(const char* name, XdmfSteeringDomain* dom);

  // Description:
  // Returns the documentation for this property. The return value
  // may be NULL if no documentation is defined in the XML
  // for this property.
  XdmfGetStringMacro(Documentation);

  // Description:
  // The label assigned by the xml parser.
  XdmfGetStringMacro(XMLLabel);

protected:
  //BTX
  friend class XdmfSteeringParser;
  friend class pqDSMViewerPanel;
  //ETX

  // Description:
  // The name assigned by the xml parser. Used to get the property
  // from a proxy. Note that the name used to obtain a property
  // that is on a subproxy may be different from the XMLName of the property,
  // see the note on ExposedProperties for vtkSMProxy.
  XdmfSetStringMacro(XMLName);

  // Description:
  // The name assigned by the xml parser. Used to get the property
  // from a proxy. Note that the name used to obtain a property
  // that is on a subproxy may be different from the XMLName of the property,
  // see the note on ExposedProperties for vtkSMProxy.
  XdmfGetStringMacro(XMLName);

  XdmfString Command;

  XdmfSteeringPropertyInternals* PInternals;

  XdmfString XMLName;
  XdmfString XMLLabel;
  XdmfSetStringMacro(XMLLabel);

  XdmfString Documentation;
  XdmfSetStringMacro(Documentation);

};

#endif /* __XdmfSteeringProperty_h */
