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

class XdmfDOM;
class XdmfHDFDOM;
class XdmfRoot;
class XdmfDomain;
class XdmfGrid;
class XdmfDsmBuffer;

#include <sstream>
#include <string>

class DSM_DLL XdmfGenerator : public XdmfObject
{
public:
  XdmfGenerator();
  ~XdmfGenerator();

  // Get generated DOM
  XdmfDOM         *GetGeneratedDOM();

  // Get generated XML string from generated DOM
  XdmfConstString  GetGeneratedFile();

  // Set DSM Buffer
  void SetDsmBuffer(XdmfDsmBuffer* _arg);

  // Generate an XDMF File from a template file and a list of HDF files
  // Put the result into an XDMF DOM and generate a temporal collection
  XdmfInt32        GenerateTemporalCollection(XdmfConstString lXdmfFile,
      XdmfConstString anHdfFile);

  // Generate an XDMF File from a template file and an HDF file
  // Put the result into an XDMF DOM
  XdmfInt32        Generate(XdmfConstString lXdmfFile, XdmfConstString hdfFileName,
      XdmfGrid *temporalGrid=NULL);

protected:

  // Convert written light XDMF Dataset path into HDF XML XPath and return
  // corresponding XML Node from the HDF DOM
  XdmfXmlNode      FindConvertHDFPath(XdmfHDFDOM *hdfDOM, XdmfConstString path);

  // Find the number of cells using the Topology Node
  // from HDF DOM and topology type
  XdmfInt32        FindNumberOfCells(XdmfHDFDOM *hdfDOM,
      XdmfXmlNode hdfTopologyNode, XdmfConstString topologyTypeStr);

  // Find DataItem structure information from a given dataset node of the HDF DOM
  // and the path of this node as defined in the light XDMF template
  XdmfConstString  FindDataItemInfo(XdmfHDFDOM *hdfDOM, XdmfXmlNode hdfDatasetNode,
      XdmfConstString hdfFileName, XdmfConstString dataPath);

  // Find attribute type from a given dataset node of the HDF DOM
  XdmfInt32        FindAttributeType(XdmfHDFDOM *hdfDOM, XdmfXmlNode hdfDatasetNode);

  XdmfDOM            *GeneratedDOM;
  std::string        *GeneratedFile;
  XdmfRoot           *GeneratedRoot;
  XdmfDomain         *GeneratedDomain;
  XdmfDsmBuffer      *DsmBuffer;
};

#endif /* XDMFGENERATOR_H */
