/*=========================================================================

  Project                 : Icarus
  Module                  : vtkDsmProxyHelper.cxx

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
#include "vtkDsmProxyHelper.h"
//
#include "vtkInformation.h"
#include "vtkObjectFactory.h"
#include "vtkClientServerInterpreter.h"
#include "vtkClientServerStream.h"
//
#include <vtksys/SystemTools.hxx>
//
#include "vtkAbstractDsmManager.h"
#ifdef ICARUS_HAVE_H5FDDSM
#include "vtkSteeringWriter.h"
#endif
//----------------------------------------------------------------------------
void vtkObject_Init(vtkClientServerInterpreter* csi);
int VTK_EXPORT vtkDataObjectAlgorithmCommand(vtkClientServerInterpreter *arlu, vtkObjectBase *ob, const char *method, const vtkClientServerStream& msg, vtkClientServerStream& resultStream, void* /*ctx*/);
int VTK_EXPORT vtkDsmProxyHelperCommand(vtkClientServerInterpreter *arlu, vtkObjectBase *ob, const char *method, const vtkClientServerStream& msg, vtkClientServerStream& resultStream);
//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkDsmProxyHelper);
vtkCxxSetObjectMacro(vtkDsmProxyHelper, DsmManager, vtkAbstractDsmManager);
#ifdef ICARUS_HAVE_H5FDDSM
vtkCxxSetObjectMacro(vtkDsmProxyHelper, SteeringWriter, vtkSteeringWriter);
#endif
//----------------------------------------------------------------------------
vtkDsmProxyHelper::vtkDsmProxyHelper()
{
  this->DsmManager     = NULL;
#ifdef ICARUS_HAVE_H5FDDSM
  this->SteeringWriter = NULL;
#endif
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
  // start in blocked mode to protect DSM file from spurious writes
  this->BlockTraffic   = 1;
}
//----------------------------------------------------------------------------
vtkDsmProxyHelper::~vtkDsmProxyHelper()
{ 
  this->SetDsmManager(NULL);
#ifdef ICARUS_HAVE_H5FDDSM
  this->SetSteeringWriter(NULL);
#endif
}
//----------------------------------------------------------------------------
int vtkDsmProxyHelper::FillInputPortInformation(int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataObject");
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
  return 1;
}
//----------------------------------------------------------------------------
void vtkDsmProxyHelper::WriteDataSetArrayData(const char *desc)
{
#ifdef ICARUS_HAVE_H5FDDSM
  if (this->SteeringWriter && this->SteeringWriter->GetDsmManager()) {
    this->SteeringWriter->SetWriteDescription(desc);
/*
    this->SteeringWriter->SetArrayName(arrayname);
    this->SteeringWriter->SetArrayType(atype);
    this->SteeringWriter->SetFieldType(ftype);
    this->SteeringWriter->SetGroupPath("/Mesh_Nodes#1/NewXYZ");
*/
    this->SteeringWriter->WriteData();
  }
  else {
    std::cout << "Steering writer called with invalid DSM setup " << std::endl;
  }
#endif
}

//----------------------------------------------------------------------------
vtkObjectBase *vtkDsmProxyHelperClientServerNewCommand(void* /*ctx*/);
int VTK_EXPORT vtkDsmProxyHelperCommand(vtkClientServerInterpreter *arlu, vtkObjectBase *ob, const char *method, const vtkClientServerStream& msg, vtkClientServerStream& resultStream, void* /*ctx*/);

//----------------------------------------------------------------------------
void VTK_EXPORT DsmProxyHelperInit(vtkClientServerInterpreter* csi)
{
  vtkObject_Init(csi);
  csi->AddNewInstanceFunction("vtkDsmProxyHelper", vtkDsmProxyHelperClientServerNewCommand);
  csi->AddCommandFunction("vtkDsmProxyHelper", vtkDsmProxyHelperCommand);
}

//----------------------------------------------------------------------------
vtkObjectBase *vtkDsmProxyHelperClientServerNewCommand(void* /*ctx*/)
{
  return vtkDsmProxyHelper::New();
}
//----------------------------------------------------------------------------
int VTK_EXPORT vtkDsmProxyHelperCommand(vtkClientServerInterpreter *arlu, vtkObjectBase *ob, const char *method, const vtkClientServerStream& msg, vtkClientServerStream& resultStream, void* /*ctx*/)
{
  vtkDsmProxyHelper *helper = vtkDsmProxyHelper::SafeDownCast(ob);
  if (!helper)
    {
    vtkOStrStreamWrapper vtkmsg;
    vtkmsg << "Cannot cast " << ob->GetClassName() << " object to vtkDsmProxyHelper.  "
           << "This probably means the class specifies the incorrect superclass in vtkTypeMacro.";
    resultStream.Reset();
    resultStream << vtkClientServerStream::Error
                 << vtkmsg.str() << 0 << vtkClientServerStream::End;
    return 0;
    }

  //
  // We need to use these normal ClientServer set/getters
  //
  if (!strcmp("BlockTraffic",method)) {
    helper->BlockTraffic = 1;
    return 1;
  }

  if (!strcmp("UnblockTraffic",method)) {
    helper->BlockTraffic = 0;
    return 1;
  }

  //
  // We need to use these normal ClientServer set/getters
  //
  if (!strcmp("SetDsmManager",method) && msg.GetNumberOfArguments(0) == 3) {
    vtkAbstractDsmManager  *temp0;
    if(vtkClientServerStreamGetArgumentObject(msg, 0, 2, &temp0, "vtkAbstractDsmManager")) {
      helper->SetDsmManager(temp0);
      return 1;
    }
  }
  if (!strcmp("GetDsmManager",method) && msg.GetNumberOfArguments(0) == 2) {
    vtkAbstractDsmManager  *temp20;
    temp20 = (helper)->GetDsmManager();
    resultStream.Reset();
    resultStream << vtkClientServerStream::Reply << (vtkObjectBase *)temp20 << vtkClientServerStream::End;
    return 1;
  }
  if (!strcmp("SetSteeringWriter",method) && msg.GetNumberOfArguments(0) == 3) {
    vtkSteeringWriter  *temp0;
    if(vtkClientServerStreamGetArgumentObject(msg, 0, 2, &temp0, "vtkSteeringWriter")) {
#ifdef ICARUS_HAVE_H5FDDSM
      helper->SetSteeringWriter(temp0);
#endif
      return 1;
    }
  }
  if (!strcmp("GetSteeringWriter",method) && msg.GetNumberOfArguments(0) == 2) {
    vtkSteeringWriter  *temp20;
#ifdef ICARUS_HAVE_H5FDDSM
    temp20 = (helper)->GetSteeringWriter();
#endif
    resultStream.Reset();
    resultStream << vtkClientServerStream::Reply << (vtkObjectBase *)temp20 << vtkClientServerStream::End;
    return 1;
  }

  //
  // If there's no DSM manager, then there's no point doing anything below here ....
  //
  if (!helper->BlockTraffic && helper && helper->GetDsmManager()) {
    if (!strncmp ("SetSteeringValueInt",method, 19))
    {
      int ival[8];
      int nArgs = 0;
      //
      std::string param_name = method;
      vtksys::SystemTools::ReplaceString(param_name, "SetSteeringValueInt", "");
      // std::cout << "Calling SetSteeringValueInt(" << param_name.c_str() << ",";
      for (int i = 0; i < msg.GetNumberOfArguments(0); i++) {
        if (msg.GetArgument(0, i, &ival[nArgs])) nArgs++;
      }
      // std::cout << nArgs << "," << "{";
      // for (int i = 0; i < nArgs; i++) {
      //   std::cout << ival[i] << (i<(nArgs-1) ? "," : "}");
      // }
      // std::cout << ");" << std::endl;
      helper->GetDsmManager()->SetSteeringValues(param_name.c_str(), nArgs, ival);
      helper->Modified();
      return 1;
    }

    if (!strncmp ("SetSteeringValueDouble",method, 22))
    {
      double dval[8];
      int nArgs = 0;
      //
      std::string param_name = method;
      vtksys::SystemTools::ReplaceString(param_name, "SetSteeringValueDouble", "");
      // std::cout << "Calling SetSteeringValueDouble(" << param_name.c_str() << ",";
      for (int i = 0; i < msg.GetNumberOfArguments(0); i++) {
        if (msg.GetArgument(0, i, &dval[nArgs])) nArgs++;
      }
      // std::cout << nArgs << "," << "{";
      // for (int i = 0; i < nArgs; i++) {
      //   std::cout << dval[i] << (i<(nArgs-1) ? "," : "}");
      // }
      // std::cout << ");" << std::endl;
      helper->GetDsmManager()->SetSteeringValues(param_name.c_str(), nArgs, dval);
      helper->Modified();
      return 1;
    }

    if (!strncmp ("SetSteeringArray",method, 16))
    {
      char *text;
      int nArgs = msg.GetNumberOfArguments(0);
      //
      std::string param_name = method;
      vtksys::SystemTools::ReplaceString(param_name, "SetSteeringArray", "");
      // std::cout << "Calling SetSteeringArray(" << param_name.c_str() << ", {";
      msg.GetArgument(0, nArgs-1, &text); 
      // std::cout << text << ")" << std::endl;
#ifdef ICARUS_HAVE_H5FDDSM
      helper->GetSteeringWriter()->SetWriteDescription(text);
      helper->WriteDataSetArrayData(text);
#endif
  return 1;
    }

    if (!strncmp ("GetSteeringValueDouble",method, 22)) {
      if (msg.GetNumberOfArguments(0) == 2) {
        double temp[2];
        std::string param_name = method;
        vtksys::SystemTools::ReplaceString(param_name, "GetSteeringValueDouble", "");
        // std::cout << "Calling GetSteeringValueDouble(" << param_name.c_str() << ");" << std::endl;
        //
        helper->GetDsmManager()->GetSteeringValues(param_name.c_str(), 2, temp);
        resultStream.Reset();
        resultStream << vtkClientServerStream::Reply << vtkClientServerStream::InsertArray(temp,2) << vtkClientServerStream::End;
        return 1;
      }
    }

    if (!strncmp ("GetSteeringScalarDouble",method, 23)) {
      if (msg.GetNumberOfArguments(0) == 2) {
        double temp;
        std::string param_name = method;
        vtksys::SystemTools::ReplaceString(param_name, "GetSteeringScalarDouble", "");
        // std::cout << "Calling GetSteeringScalarDouble(" << param_name.c_str() << ");" << std::endl;
        //
        helper->GetDsmManager()->GetSteeringValues(param_name.c_str(), 1, &temp);
        resultStream.Reset();
        resultStream << vtkClientServerStream::Reply << vtkClientServerStream::InsertArray(&temp,1) << vtkClientServerStream::End;
        return 1;
      }
    }

    if (!strncmp ("ExecuteSteeringCommand",method, 22)) {
      if (msg.GetNumberOfArguments(0) == 2) {
        int temp;
        std::string param_name = method;
        vtksys::SystemTools::ReplaceString(param_name, "ExecuteSteeringCommand", "");
        // std::cout << "Calling ExecuteSteeringCommand(" << param_name.c_str() << ");" << std::endl;
        //
        helper->GetDsmManager()->SetSteeringValues(param_name.c_str(), 1, &temp);
        return 1;
      }
    }

  }
  if (vtkDataObjectAlgorithmCommand(arlu, helper,method,msg,resultStream, NULL))
    {
    return 1;
    }

  return 1;
}

  /*
      for (int i=0; i<nArgs; i++) {
        int t = msg.GetArgumentType(0,i);
        if (t==vtkClientServerStream::int32_value) {
          msg.GetArgument(0, i, &ival); 
          std::cout << ival << (i<(nArgs-1) ? "(int)," : "(int)}");
        }
        if (t==vtkClientServerStream::float64_value) {
          msg.GetArgument(0, i, &dval); 
          std::cout << dval << (i<(nArgs-1) ? "(double)," : "(double)}");
        }
        if (t==vtkClientServerStream::string_value) {
          msg.GetArgument(0, i, &text); 
          std::cout << text << (i<(nArgs-1) ? "(string)," : "(string)}");
        }
      }
  */
