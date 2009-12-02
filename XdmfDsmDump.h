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
#ifndef __XdmfDsmDump_h
#define __XdmfDsmDump_h

#include "XdmfObject.h"
#include "XdmfDsmBuffer.h"

#include <sstream>

class XDMF_EXPORT XdmfDsmDump : public XdmfObject {

    public :
        XdmfDsmDump();
        ~XdmfDsmDump();

        void Dump();
        void DumpLight();
        void DumpXML(std::ostringstream &);

        void SetDsmBuffer(XdmfDsmBuffer* _arg);

    protected:
        XdmfDsmBuffer *DsmBuffer;
};

#endif // __XdmfDsmDump_h
