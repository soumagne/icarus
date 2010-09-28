/*=========================================================================

  Project                 : Icarus
  Module                  : vtkXdmfSteeringParser.h

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

#ifndef __vtkXdmfSteeringParser_h
#define __vtkXdmfSteeringParser_h

#include <XdmfDOM.h>

#include <sstream>
#include <string>

// Structures to hold information given in the .lxmf file
typedef struct xmfSteeringConfigAttribute_ {
  std::string attributeName;
  XdmfBoolean isEnabled;
} xmfSteeringConfigAttribute;

typedef struct xmfSteeringConfigGrid_ {
  std::string gridName;
  XdmfInt32   numberOfAttributes;
  XdmfBoolean isEnabled;
  xmfSteeringConfigAttribute *attributeConfig;
} xmfSteeringConfigGrid;

typedef struct xmfSteeringConfig_ {
  XdmfInt32              numberOfGrids;
  xmfSteeringConfigGrid *gridConfig;
} xmfSteeringConfig;


class XDMF_EXPORT vtkXdmfSteeringParser : public XdmfObject
{
public:
  vtkXdmfSteeringParser();
  ~vtkXdmfSteeringParser();

  XdmfGetValueMacro(SteeringConfig, xmfSteeringConfig*);
  XdmfGetValueMacro(ConfigDOM, XdmfDOM*);
  int Parse(const char *filepath);

protected:
  XdmfDOM           *ConfigDOM;
  xmfSteeringConfig *SteeringConfig;
};

#endif /* __vtkXdmfSteeringParser_h */
