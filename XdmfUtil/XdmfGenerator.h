/*=========================================================================

  Project                 : XdmfUtil
  Module                  : XdmfGenerator.h

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

#ifndef XDMFGENERATOR_H
#define XDMFGENERATOR_H

#include "XdmfUtilconfig.h"
#include "XdmfLightData.h"

#include "XdmfDOM.h"
#include "XdmfRoot.h"
#include "XdmfDomain.h"

class XdmfHDFDOM;
class XdmfGrid;
class H5FDdsmBuffer;

#include <sstream>
#include <string>

class XDMF_EXPORT XdmfGenerator : public XdmfObject
{
public:
  XdmfGenerator();
  ~XdmfGenerator();

  // Get generated DOM
  XdmfDOM         *GetGeneratedDOM();

  // Get generated XML string from generated DOM
  XdmfConstString  GetGeneratedFile();

  // Set DSM Buffer
  void SetDsmBuffer(H5FDdsmBuffer* _arg);

  // Generate an XDMF File from a template file and a list of HDF files
  // the list of HDF files is generated using a specified file and a pattern
  // corresponding to all the matching files contained in the same directory
  // Put the result into an XDMF DOM and generate a temporal collection
  XdmfInt32        GenerateTemporalCollection(XdmfConstString lXdmfFile,
      XdmfConstString anHdfFile, XdmfConstString fileNamePattern = NULL);

  // Generate an XDMF File from a template file and an HDF file
  // Put the result into an XDMF DOM
  XdmfInt32        Generate(XdmfConstString lXdmfFile, XdmfConstString hdfFileName,
      XdmfGrid *temporalGrid=NULL, XdmfInt32 timeValue=0);

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
      XdmfConstString hdfFileName, XdmfConstString dataPath,
      XdmfDOM *lXdmfDOM, XdmfXmlNode templateNode);

  // Find attribute type from a given dataset node of the HDF DOM
  XdmfInt32 FindAttributeType(XdmfHDFDOM *hdfDOM, XdmfXmlNode hdfDatasetNode,
      XdmfDOM *lXdmfDOM, XdmfXmlNode templateNode);

  XdmfDOM             GeneratedDOM;
  std::string         GeneratedFile;
  XdmfRoot            GeneratedRoot;
  XdmfDomain          GeneratedDomain;
  H5FDdsmBuffer      *DsmBuffer;
};

#endif /* XDMFGENERATOR_H */
