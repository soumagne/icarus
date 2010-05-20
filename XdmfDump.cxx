/*=========================================================================

  Project                 : vtkCSCS
  Module                  : XdmfDump.cxx

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
#include "h5dump.h"

#include "H5FDdsmBuffer.h"
#include "XdmfDump.h"

//----------------------------------------------------------------------------
XdmfDump::XdmfDump()
{
    this->DsmBuffer = NULL;
    this->FileName = NULL;
}
//----------------------------------------------------------------------------
XdmfDump::~XdmfDump()
{
    if (this->FileName) delete this->FileName;
    this->FileName = NULL;
}
//----------------------------------------------------------------------------
void
XdmfDump::SetDsmBuffer(H5FDdsmBuffer *dsmBuffer)
{
    this->DsmBuffer = dsmBuffer;
}
//----------------------------------------------------------------------------
void
XdmfDump::Dump()
{
    H5dump_dsm(this->FileName, this->DsmBuffer);
}
//----------------------------------------------------------------------------
void
XdmfDump::DumpLight()
{
    H5dump_dsm_light(this->FileName, this->DsmBuffer);
}
//----------------------------------------------------------------------------
void
XdmfDump::DumpXML(std::ostringstream &stream)
{
  if (this->DsmBuffer) {
    H5dump_dsm_xml(this->FileName, stream, this->DsmBuffer);
  } else {
    H5dump_xml(this->FileName, stream);
  }
}
//----------------------------------------------------------------------------
