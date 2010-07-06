/*=========================================================================

  Project                 : XdmfUtil
  Module                  : XdmfGenerate.cxx

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

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>

#include "XdmfGenerator.h"

int main(int argc, char *argv[])
{
  if (argc < 3) {
    printf("Usage: %s template_file hdf_file is_collection\n"
           "  template_file: path to file containing an Xdmf template description\n"
           "  hdf_file: path to an HDF file which has to be used by the Xdmf generator\n"
           "  is_collection: 0 or 1 (default): tell the generator to try to find other\n"
           "                 hdf files in the same directory like the one already specified\n"
        , argv[0]);
    return EXIT_FAILURE;
  }

  XdmfGenerator      *xdmfGenerator = new XdmfGenerator();
  const char         *lxdmfFileName = argv[1];
  const char         *hdfFileName = argv[2];
  bool                isCollection = true;

  if (argc == 4) {
    isCollection = (bool) atoi(argv[3]);
  }

  if (isCollection) {
    xdmfGenerator->GenerateTemporalCollection(lxdmfFileName, hdfFileName);
  } else {
    xdmfGenerator->Generate(lxdmfFileName, hdfFileName);
  }
  std::cout << xdmfGenerator->GetGeneratedFile() << std::endl;

  delete xdmfGenerator;

  return EXIT_SUCCESS;
}
