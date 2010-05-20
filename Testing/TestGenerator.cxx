/*=========================================================================

  Project                 : vtkCSCS
  Module                  : TestGenerator.h
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

#include <cstdlib>
#include <iostream>
#include <sstream>

#include <H5FDdsmDump.h>
#include <XdmfGenerator.h>

int main(int argc, char *argv[])
{
  if (argc != 3) {
    std::cout << "Usage: " << argv[0] << " <HDF file path>"
    << " <XDMF template file path>" << endl;
    return EXIT_FAILURE;
  }

  std::ostringstream  dumpStream;
  XdmfGenerator      *xdmfGenerator = new XdmfGenerator();
  H5FDdsmDump        *xdmfDsmDump = new H5FDdsmDump();
  const char         *hdfFileName = argv[1];
  const char         *lxdmfFileName = argv[2];

  xdmfDsmDump->SetFileName(hdfFileName);
  xdmfDsmDump->DumpXML(dumpStream);
  xdmfGenerator->SetHdfFileName(hdfFileName);
  xdmfGenerator->GenerateHead();
  xdmfGenerator->Generate(lxdmfFileName, dumpStream.str().c_str());
  std::cout << xdmfGenerator->GetGeneratedFile() << std::endl;

  delete xdmfGenerator;
  delete xdmfDsmDump;

  return EXIT_SUCCESS;
}
