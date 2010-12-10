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

//
// Structures to hold information given in the .lxmf file
//
//----------------------------------------------------------------------------
typedef struct xmfSteeringConfigAttribute_ {
  std::string hdfPath;
} xmfSteeringConfigAttribute;

typedef std::map<std::string, xmfSteeringConfigAttribute> AttributeMap;
//----------------------------------------------------------------------------
typedef struct xmfSteeringConfigGrid_ {
  AttributeMap attributeConfig;
  std::string  WidgetControl;
} xmfSteeringConfigGrid;

typedef std::map<std::string, xmfSteeringConfigGrid> GridMap;
//----------------------------------------------------------------------------
class pq3DWidget;
class vtkSMProperty;
class vtkSMProxy;
// 
// In order to map paraview 3DWidget controls onto properties and proxies
// we shall store information. This info is hard to get at because it is buried
// deep inside proxies etc, so we extract as much as we can when parsing XML 
// and the paraview specific stuff is added when the object panel is created
// 
struct SteeringGUIWidgetInfo {
  pq3DWidget    *pqWidget;
  vtkSMProperty *Property;
  vtkSMProxy    *ControlledProxy;
  vtkSMProxy    *ReferenceProxy;
  std::string    AssociatedGrid;
  std::string    Initialization;
  std::string    WidgetType;
  SteeringGUIWidgetInfo() {
    pqWidget        = NULL;
    Property        = NULL;
    ControlledProxy = NULL;
    ReferenceProxy  = NULL;
  }
};
typedef std::map<std::string, SteeringGUIWidgetInfo> SteeringGUIWidgetMap;
//----------------------------------------------------------------------------
class XdmfSteeringParser : public XdmfObject {
public:
   XdmfSteeringParser();
  ~XdmfSteeringParser();

  XdmfGetValueMacro(ConfigDOM, XdmfDOM*);

  int Parse(const char *filepath);
  int CreateParaViewProxyXML(XdmfXmlNode interactionNode);
  std::string BuildWidgetHints(XdmfConstString name, XdmfXmlNode propertyNode);
  
  GridMap &GetSteeringConfig() { 
    return this->SteeringConfig; 
  }
  SteeringGUIWidgetMap &GetSteeringWidgetMap() {
    return this->SteeringWidgetMap;
  }

protected:
  XdmfDOM             *ConfigDOM;
  GridMap              SteeringConfig;
  SteeringGUIWidgetMap SteeringWidgetMap;
  
};

#endif /* __XdmfSteeringParser_h */
