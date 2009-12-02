/*=========================================================================

  Project                 : vtkCSCS
  Module                  : XdmfDsmDump.cxx
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
#include "h5dump.h"

#include "XdmfDsmBuffer.h"
#include "XdmfDsmDump.h"

//----------------------------------------------------------------------------
XdmfDsmDump::XdmfDsmDump()
{
    this->DsmBuffer = NULL;
}
//----------------------------------------------------------------------------
XdmfDsmDump::~XdmfDsmDump()
{

}
//----------------------------------------------------------------------------
void
XdmfDsmDump::SetDsmBuffer(XdmfDsmBuffer *dsmBuffer)
{
    this->DsmBuffer = dsmBuffer;
}
//----------------------------------------------------------------------------
void
XdmfDsmDump::Dump()
{
    H5dump_dsm(this->DsmBuffer);
}
//----------------------------------------------------------------------------
void
XdmfDsmDump::DumpLight()
{
    H5dump_dsm_light(this->DsmBuffer);
}
//----------------------------------------------------------------------------
void
XdmfDsmDump::DumpXML(std::ostringstream &stream)
{
    H5dump_dsm_xml(stream, this->DsmBuffer);
}
//----------------------------------------------------------------------------
