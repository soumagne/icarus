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
//----------------------------------------------------------------------------
void vtkObject_Init(vtkClientServerInterpreter* csi);
int vtkDataObjectAlgorithmCommand(vtkClientServerInterpreter *arlu, vtkObjectBase *ob, const char *method, const vtkClientServerStream& msg, vtkClientServerStream& resultStream);
int VTK_EXPORT vtkDSMProxyHelperCommand(vtkClientServerInterpreter *arlu, vtkObjectBase *ob, const char *method, const vtkClientServerStream& msg, vtkClientServerStream& resultStream);
//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkDSMProxyHelper);
vtkCxxSetObjectMacro(vtkDSMProxyHelper, DSMManager, vtkDSMManager);
//----------------------------------------------------------------------------
vtkDSMProxyHelper::vtkDSMProxyHelper() 
{
  this->DSMManager = NULL;
  this->SetNumberOfInputPorts(0);
  this->SetNumberOfOutputPorts(1);
}
//----------------------------------------------------------------------------
vtkDSMProxyHelper::~vtkDSMProxyHelper()
{ 
  this->SetDSMManager(NULL);
}
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
vtkObjectBase *vtkDSMProxyHelperClientServerNewCommand()
{
  return vtkDSMProxyHelper::New();
}
//----------------------------------------------------------------------------
void VTK_EXPORT DSMProxyHelperInit(vtkClientServerInterpreter* csi)
{
  vtkObject_Init(csi);
  csi->AddNewInstanceFunction("vtkDSMProxyHelper", vtkDSMProxyHelperClientServerNewCommand);
  csi->AddCommandFunction("vtkDSMProxyHelper", vtkDSMProxyHelperCommand);
}
//----------------------------------------------------------------------------
int VTK_EXPORT vtkDSMProxyHelperCommand(vtkClientServerInterpreter *arlu, vtkObjectBase *ob, const char *method, const vtkClientServerStream& msg, vtkClientServerStream& resultStream)
{
  vtkDSMProxyHelper *op = vtkDSMProxyHelper::SafeDownCast(ob);
  if (!op)
    {
    vtkOStrStreamWrapper vtkmsg;
    vtkmsg << "Cannot cast " << ob->GetClassName() << " object to vtkDSMProxyHelper.  "
           << "This probably means the class specifies the incorrect superclass in vtkTypeMacro.";
    resultStream.Reset();
    resultStream << vtkClientServerStream::Error
                 << vtkmsg.str() << 0 << vtkClientServerStream::End;
    return 0;
    }
  
  if (!strncmp ("SetSteeringValueInt",method, 19))
  {
    int ival[256];
    int nArgs = 0;
    //
    std::string param_name = method;
    vtksys::SystemTools::ReplaceString(param_name, "SetSteeringValueInt", "");
    std::cout << "Calling SetSteeringValueInt(" << param_name.c_str() << ",";
    for (int i=0; i<msg.GetNumberOfArguments(0); i++) {
      if (msg.GetArgument(0, i, &ival[nArgs])) nArgs++;
    }
    std::cout << nArgs << "," << "{";
    for (int i=0; i<nArgs; i++) {
      std::cout << ival[i] << (i<(nArgs-1) ? "," : "}");
    }
    std::cout << ");" << std::endl;
    if (op && op->GetDSMManager()) {
      op->GetDSMManager()->SetSteeringValues(param_name.c_str(), nArgs, ival);
    }
    return 1;
  }

  if (!strncmp ("SetSteeringValueDouble",method, 22))
  {
    double dval[256];
    int nArgs = 0;
    //
    std::string param_name = method;
    vtksys::SystemTools::ReplaceString(param_name, "SetSteeringValueDouble", "");
    std::cout << "Calling SetSteeringValueDouble(" << param_name.c_str() << ",";
    for (int i=0; i<msg.GetNumberOfArguments(0); i++) {
      if (msg.GetArgument(0, i, &dval[nArgs])) nArgs++;
    }
    std::cout << nArgs << "," << "{";
    for (int i=0; i<nArgs; i++) {
      std::cout << dval[i] << (i<(nArgs-1) ? "," : "}");
    }
    std::cout << ");" << std::endl;
    if (op && op->GetDSMManager()) {
      op->GetDSMManager()->SetSteeringValues(param_name.c_str(), nArgs, dval);
    }
    return 1;
  }

  if (!strncmp ("GetSteeringValueDouble",method, 22)) {
    if (msg.GetNumberOfArguments(0) == 2) {
      double temp[2];
      int nArgs = msg.GetNumberOfArguments(0);
      std::string param_name = method;
      vtksys::SystemTools::ReplaceString(param_name, "GetSteeringValueDouble", "");
      std::cout << "Calling GetSteeringValueDouble(" << param_name.c_str() << ");" << std::endl;

      if (op && op->GetDSMManager()) {
        op->GetDSMManager()->GetSteeringValues(param_name.c_str(), 2, temp);
        resultStream.Reset();
        resultStream << vtkClientServerStream::Reply << vtkClientServerStream::InsertArray(temp,2) << vtkClientServerStream::End;
        return 1;
      }
    }
  }

  if (!strncmp ("ExecuteSteeringCommand",method, 22)) {
    if (msg.GetNumberOfArguments(0) == 3) {
      int temp;
      int nArgs = msg.GetNumberOfArguments(0);
      std::string param_name = method;
      vtksys::SystemTools::ReplaceString(param_name, "ExecuteSteeringCommand", "");
      std::cout << "Calling ExecuteSteeringCommand(" << param_name.c_str() << ");" << std::endl;

      if (op && op->GetDSMManager()) {
        op->GetDSMManager()->SetSteeringValues(param_name.c_str(), 1, &temp);
        return 1;
      }
    }
  }

  //
  // We actually need to use these real ClientServer set/getters
  //
  if (!strcmp("SetDSMManager",method) && msg.GetNumberOfArguments(0) == 3)
    {
    vtkDSMManager  *temp0;
    if(vtkClientServerStreamGetArgumentObject(msg, 0, 2, &temp0, "vtkDSMManager"))
      {
      op->SetDSMManager(temp0);
      return 1;
      }
    }
  if (!strcmp("GetDSMManager",method) && msg.GetNumberOfArguments(0) == 2)
    {
    vtkDSMManager  *temp20;
      {
      temp20 = (op)->GetDSMManager();
      resultStream.Reset();
      resultStream << vtkClientServerStream::Reply << (vtkObjectBase *)temp20 << vtkClientServerStream::End;
      return 1;
      }
    }

  if (vtkDataObjectAlgorithmCommand(arlu, op,method,msg,resultStream))
    {
    return 1;
    }

  return 1;
}
