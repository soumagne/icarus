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

class XdmfSteeringIntVectorProperty;
class XdmfSteeringDoubleVectorProperty;

// Structures to hold information given in the .lxmf file
typedef struct xmfSteeringConfigAttribute_ {
  std::string attributeName;
  std::string hdfPath;
  XdmfBoolean isEnabled;
} xmfSteeringConfigAttribute;

typedef struct xmfSteeringConfigGrid_ {
  std::string gridName;
  XdmfInt32   numberOfAttributes;
  XdmfBoolean isEnabled;
  xmfSteeringConfigAttribute *attributeConfig;
} xmfSteeringConfigGrid;

typedef struct xmfSteeringConfigInteract_ {
  XdmfInt32   numberOfIntVectorProperties;
  XdmfSteeringIntVectorProperty **intVectorProperties;
  XdmfInt32   numberOfDoubleVectorProperties;
  XdmfSteeringDoubleVectorProperty **doubleVectorProperties;
} xmfSteeringConfigInteract;

typedef struct xmfSteeringConfig_ {
  XdmfInt32                    numberOfGrids;
  xmfSteeringConfigGrid       *gridConfig;
  xmfSteeringConfigInteract    interactConfig;
} xmfSteeringConfig;


class XDMF_EXPORT XdmfSteeringParser : public XdmfObject
{
public:
  XdmfSteeringParser();
  ~XdmfSteeringParser();

  void DeleteConfig();
  XdmfGetValueMacro(SteeringConfig, xmfSteeringConfig*);
  XdmfGetValueMacro(ConfigDOM, XdmfDOM*);
  int Parse(const char *filepath);

  int CreateProxyXML(XdmfXmlNode interactionNode);

protected:
  XdmfDOM           *ConfigDOM;
  xmfSteeringConfig *SteeringConfig;
};

#endif /* __XdmfSteeringParser_h */
