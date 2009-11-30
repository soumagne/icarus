/*=========================================================================

  Project                 : vtkCSCS
  Module                  : XdmfGenerator.h
  Revision of last commit : $Rev$
  Author of last commit   : $Author$
  Date of last commit     : $Date::                            $

  Copyright (C) CSCS - Swiss National Supercomputing Centre.
  You may use modify and and distribute this code freely providing
  1) This copyright notice appears on all copies of source code
  2) An acknowledgment appears with any substantial usage of the code
  3) If this code is contributed to any other open source project, it
  must not be reformatted such that the indentation, bracketing or
  overall style is modified significantly.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

=========================================================================*/

#ifndef XDMFGENERATOR_H
#define XDMFGENERATOR_H

#include "H5MButil.h"     // for DSM_DLL
#include "XdmfLightData.h"
#include "XdmfDOM.h"
#include "XdmfHDFDOM.h"

class DSM_DLL XdmfGenerator : public XdmfObject
{
public:
  XdmfGenerator();
  ~XdmfGenerator();

  // Generated DOM put into an XDMF DOM
  XdmfDOM     *GetGeneratedDOM();

  // Generate an XDMF File from a template file and the H5dump XML output
  // Puts the result into an XDMF DOM
  XdmfInt32    Generate(XdmfConstString, XdmfConstString);

  // Set/Get HDF file name used with h5dump and where heavy data is stored
  XdmfSetStringMacro(HdfFileName);
  XdmfGetStringMacro(HdfFileName);

protected:

  // Convert written light XDMF Dataset path into HDF XML XPath and return
  // corresponding XML Node from the HDF DOM
  XdmfXmlNode     FindConvertHDFPath(XdmfConstString);

  // Find the number of cells using the Topology Node
  // from HDF DOM and topology type
  XdmfInt32       FindNumberOfCells(XdmfXmlNode, XdmfConstString);

  // Find DataItem structure information from a given dataset node of the HDF DOM
  // and the path of this node as defined in the light XDMF template
  XdmfConstString FindDataItemInfo(XdmfXmlNode, XdmfConstString);

  // Find attribute type from a given dataset node of the HDF DOM
  XdmfInt32       FindAttributeType(XdmfXmlNode);

  XdmfDOM    *GeneratedDOM;
  XdmfDOM    *LXdmfDOM;
  XdmfHDFDOM *HdfDOM;
  XdmfString  HdfFileName;
};

#endif /* XDMFGENERATOR_H */
