/*=========================================================================

  Project                 : vtkCSCS
  Module                  : vtkXdmfReader4.h
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
#include "vtkXdmfReader4.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkMultiProcessController.h"
//
#include "vtkDSMManager.h"
//
//----------------------------------------------------------------------------
vtkCxxRevisionMacro(vtkXdmfReader4, "$Revision$");
vtkStandardNewMacro(vtkXdmfReader4);
vtkCxxSetObjectMacro(vtkXdmfReader4, Controller, vtkMultiProcessController);
//----------------------------------------------------------------------------
//#define JB_DEBUG__
#ifdef JB_DEBUG__
  #ifdef ___WIN32
    #define OUTPUTTEXT(a) vtkOutputWindowDisplayText(a);
  #else
    #define OUTPUTTEXT(a) std::cout << a; std::cout.flush();
  #endif

  #undef vtkDebugMacro
  #define vtkDebugMacro(p,a)                           \
  {                                                 \
    std::stringstream vtkmsg;                       \
    SimpleMutexLock::GlobalLock.Lock();             \
    vtkmsg << "P(" << p << ") " << a;               \
    std::cout << vtkmsg.str().c_str() << std::endl; \
    SimpleMutexLock::GlobalLock.Unlock();           \
  }
  #define vtkXDRDebug(a) vtkDebugMacro(this->UpdatePiece,a)
  #define vtkXDRError(a) vtkDebugMacro(this->UpdatePiece,a)
  //
  #undef  vtkGenericWarningMacro
  #define vtkGenericWarningMacro(a) vtkDebugMacro(-1,a)
#else
  #define vtkXDRDebug(a) vtkDebugMacro(a)
  #define vtkXDRError(a) vtkDebugMacro(a)
#endif
//----------------------------------------------------------------------------
vtkXdmfReader4::vtkXdmfReader4()
{
  this->DSMManager = 0;
}
//----------------------------------------------------------------------------
vtkXdmfReader4::~vtkXdmfReader4()
{
}
//----------------------------------------------------------------------------
void vtkXdmfReader4::SetDSMManager(vtkDSMManager* dsmmanager)
{
  this->DSMManager = dsmmanager;
  this->SetDsmBuffer(dsmmanager->GetDSMHandle());
}
//----------------------------------------------------------------------------
bool vtkXdmfReader4::PrepareDsmBufferDocument()
{
  if (this->DSMManager && this->DSMManager->GetXMLStringReceive()) {
    this->SetReadFromInputString(1);
    this->SetInputString(this->DSMManager->GetXMLStringReceive());
  }
  return true;
}
//----------------------------------------------------------------------------
void vtkXdmfReader4::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

