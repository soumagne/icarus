/*=========================================================================

  Project                 : Icarus
  Module                  : XdmfSteeringParser.h

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

#ifndef __XdmfSteeringParser_h
#define __XdmfSteeringParser_h

#include <XdmfDOM.h>

#include <sstream>
#include <string>
#include <map>

// Structures to hold information given in the .lxmf file
typedef struct xmfSteeringConfigAttribute_ {
  std::string hdfPath;
  XdmfBoolean isEnabled;
} xmfSteeringConfigAttribute;

class AttributeMap : public std::map<std::string, xmfSteeringConfigAttribute>
{
};

typedef struct xmfSteeringConfigGrid_ {
  XdmfBoolean  isEnabled;
  AttributeMap attributeConfig;
} xmfSteeringConfigGrid;

class GridMap : public std::map<std::string, xmfSteeringConfigGrid>
{
};

class XdmfSteeringParser : public XdmfObject {
public:
   XdmfSteeringParser();
  ~XdmfSteeringParser();

  void DeleteConfig();

  XdmfGetValueMacro(ConfigDOM, XdmfDOM*);

  int Parse(const char *filepath);
  int CreateParaViewProxyXML(XdmfXmlNode interactionNode);
  
  GridMap &GetSteeringConfig() { return this->SteeringConfig; }

protected:
  XdmfDOM  *ConfigDOM;
  GridMap   SteeringConfig;
  
};

#endif /* __XdmfSteeringParser_h */
