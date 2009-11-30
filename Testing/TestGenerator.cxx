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

#include <h5dump.h>
#include <XdmfGenerator.h>

#ifdef H5_HAVE_PARALLEL
#include <mpi.h>
#endif

using std::cerr;
using std::cout;
using std::endl;

#define DEBUG
#ifdef  DEBUG
#  define PRINT_DEBUG_INFO(x) cout << x << endl;
#else
#  define PRINT_DEBUG_INFO(x)
#endif
#define PRINT_INFO(x) cout << x << endl;
#define PRINT_ERROR(x) cerr << x << endl;

int main(int argc, char *argv[])
{
  if (argc != 3) {
    PRINT_INFO("Usage: " << argv[0] << " <HDF file path>" << " <XDMF template file path>" << endl);
    return EXIT_FAILURE;
  }

#ifdef H5_HAVE_PARALLEL
  MPI_Init(&argc, &argv);
#endif

  std::ostringstream  dumpStream;
  std::ostringstream  generatedDescription;
  XdmfGenerator      *xdmfGenerator = new XdmfGenerator();
  std::string         hdfFileName = argv[1];
  std::string         lxdmfFileName = argv[2];

  H5dump_xml(dumpStream, hdfFileName.c_str());
  xdmfGenerator->SetHdfFileName(hdfFileName.c_str());
  // cerr << dumpStream.str().c_str() << endl;
  xdmfGenerator->Generate(lxdmfFileName.c_str(), dumpStream.str().c_str());
  generatedDescription << xdmfGenerator->GetGeneratedDOM()->GenerateHead();
  generatedDescription << xdmfGenerator->GetGeneratedDOM()->Serialize();
  cout << generatedDescription.str().c_str() << endl;

  delete xdmfGenerator;

#ifdef H5_HAVE_PARALLEL
  MPI_Finalize();
#endif

  return EXIT_SUCCESS;
}
