/*=========================================================================

  Project                 : Icarus
  Module                  : vtkDSMProxyHelper.cxx

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
#include "vtkDSMProxyHelper.h"
//
#include "vtkObjectFactory.h"
#include "vtkClientServerInterpreter.h"
#include "vtkClientServerStream.h"
//
#include <vtksys/SystemTools.hxx>
#include <vtksys/RegularExpression.hxx>
#include <vtkstd/vector>
//
#include "vtkSmartPointer.h"
//
#include "vtkDSMManager.h"
//
int vtkObjectCommand(vtkClientServerInterpreter*, vtkObjectBase*, const char*, const vtkClientServerStream&, vtkClientServerStream& resultStream);
void vtkObject_Init(vtkClientServerInterpreter* csi);
int VTK_EXPORT vtkDSMProxyHelperCommand(vtkClientServerInterpreter *arlu, vtkObjectBase *ob, const char *method, const vtkClientServerStream& msg, vtkClientServerStream& resultStream);
//
vtkObjectBase *vtkDSMProxyHelperClientServerNewCommand()
{
  return vtkDSMProxyHelper::New();
}

void VTK_EXPORT DSMProxyHelperInit(vtkClientServerInterpreter* csi)
{
  static bool once;
  if(!once)
    {
    once = true;
    vtkObject_Init(csi);
    csi->AddNewInstanceFunction("vtkDSMProxyHelper", vtkDSMProxyHelperClientServerNewCommand);
    csi->AddCommandFunction("vtkDSMProxyHelper", vtkDSMProxyHelperCommand);
    }
}

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkDSMProxyHelper);
vtkCxxSetObjectMacro(vtkDSMProxyHelper, DSMManager, vtkDSMManager);
//----------------------------------------------------------------------------
vtkDSMProxyHelper::vtkDSMProxyHelper() 
{
}
//----------------------------------------------------------------------------
vtkDSMProxyHelper::~vtkDSMProxyHelper()
{ 
}
//----------------------------------------------------------------------------
int VTK_EXPORT vtkDSMProxyHelperCommand(vtkClientServerInterpreter *arlu, vtkObjectBase *ob, const char *method, const vtkClientServerStream& msg, vtkClientServerStream& resultStream)
{
  vtkDSMProxyHelper *op = vtkDSMProxyHelper::SafeDownCast(ob);
  if(!op)
    {
    vtkOStrStreamWrapper vtkmsg;
    vtkmsg << "Cannot cast " << ob->GetClassName() << " object to vtkDSMProxyHelper.  "
           << "This probably means the class specifies the incorrect superclass in vtkTypeMacro.";
    resultStream.Reset();
    resultStream << vtkClientServerStream::Error
                 << vtkmsg.str() << 0 << vtkClientServerStream::End;
    return 0;
    }
  (void)arlu;
  
  if (!strncmp ("SetSteeringValueInt",method, 19))
  {
    int ival[256];
    int nArgs = 0;
    //
    std::string param_name = method;
    vtksys::SystemTools::ReplaceString(param_name, "SetSteeringValueInt_", "");
    std::cout << "Calling SetSteeringValueInt(" << param_name.c_str() << ",";
    for (int i=0; i<msg.GetNumberOfArguments(0); i++) {
      if (msg.GetArgument(0, i, &ival[nArgs])) nArgs++;
    }
    std::cout << nArgs << "," << "{";
    for (int i=0; i<nArgs; i++) {
      std::cout << ival[i] << (i<(nArgs-1) ? "," : "}");
    }
    std::cout << ");" << std::endl;
//    op->DSMManager->SetSteeringValueInt(param_name.c_str(), nArgs, ival);
  }

  if (!strncmp ("SetSteeringValueDouble",method, 22))
  {
    double dval[256];
    int nArgs = 0;
    //
    std::string param_name = method;
    vtksys::SystemTools::ReplaceString(param_name, "SetSteeringValueDouble_", "");
    std::cout << "Calling SetSteeringValueDouble(" << param_name.c_str() << ",";
    for (int i=0; i<msg.GetNumberOfArguments(0); i++) {
      if (msg.GetArgument(0, i, &dval[nArgs])) nArgs++;
    }
    std::cout << nArgs << "," << "{";
    for (int i=0; i<nArgs; i++) {
      std::cout << dval[i] << (i<(nArgs-1) ? "," : "}");
    }
    std::cout << ");" << std::endl;
//    op->DSMManager->SetSteeringValueDouble(param_name.c_str(), nArgs, dval);
  }

  return 1;
}
