/*=========================================================================

  Project                 : Icarus
  Module                  : vtkDSMManager.cxx

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
#include "vtkDSMManager.h"
//
#include "vtkObjectFactory.h"
//
#include <vtksys/SystemTools.hxx>
#include <vtksys/RegularExpression.hxx>
#include <vtkstd/vector>
//
#include "vtkSmartPointer.h"

#ifdef VTK_USE_MPI
#include "vtkMPI.h"
#include "vtkMPIController.h"
#include "vtkMPICommunicator.h"
vtkCxxSetObjectMacro(vtkDSMManager, Controller, vtkMultiProcessController);
#endif
//
#include "XdmfGenerator.h"

//----------------------------------------------------------------------------
#undef  vtkDebugMacro
#define vtkDebugMacro(a) H5FDdsmExternalDebug(a)

#undef  vtkErrorMacro
#define vtkErrorMacro(a) H5FDdsmExternalError(a)
//----------------------------------------------------------------------------
vtkCxxRevisionMacro(vtkDSMManager, "$Revision$");
vtkStandardNewMacro(vtkDSMManager);
//----------------------------------------------------------------------------
vtkDSMManager::vtkDSMManager() 
{
  this->UpdatePiece             = 0;
  this->UpdateNumPieces         = 0;
  //
#ifdef VTK_USE_MPI
  this->Controller              = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());
#endif
  this->XMFDescriptionFilePath  = NULL;
  this->DsmManager              = new H5FDdsmManager();
}
//----------------------------------------------------------------------------
vtkDSMManager::~vtkDSMManager()
{ 
  if (this->DsmManager) delete this->DsmManager;
  this->DsmManager = NULL;

#ifdef VTK_USE_MPI
  this->SetController(NULL);
#endif
}
//----------------------------------------------------------------------------
bool vtkDSMManager::DestroyDSM()
{
  return this->DsmManager->DestroyDSM();
}
//----------------------------------------------------------------------------
void vtkDSMManager::CheckMPIController()
{
  if (this->Controller->IsA("vtkDummyController"))
  {
    vtkDebugMacro(<<"Running vtkDummyController : replacing it");
    int flag = 0;
    MPI_Initialized(&flag);
    if (flag == 0)
    {
      vtkDebugMacro(<<"Running without MPI, attempting to initialize ");
      //int argc = 1;
      //const char *argv = "D:\\cmakebuild\\pv-shared\\bin\\RelWithDebInfo\\paraview.exe";
      //char **_argv = (char**) &argv;
      int provided, rank, size;
      MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);
      MPI_Comm_size(MPI_COMM_WORLD, &size);
      //
      if (rank == 0) {
        if (provided != MPI_THREAD_MULTIPLE) {
          vtkstd::cout << "MPI_THREAD_MULTIPLE not set, you may need to recompile your "
            << "MPI distribution with threads enabled" << vtkstd::endl;
        }
        else {
          vtkstd::cout << "MPI_THREAD_MULTIPLE is OK" << vtkstd::endl;
        }
      }
    }
    //
    vtkDebugMacro(<<"Setting Global MPI controller");
    vtkSmartPointer<vtkMPIController> controller = vtkSmartPointer<vtkMPIController>::New();
    if (flag == 0) controller->Initialize(NULL, NULL, 1);
    this->SetController(controller);
    vtkMPIController::SetGlobalController(controller);
  }
}
//----------------------------------------------------------------------------
bool vtkDSMManager::ReadDSMConfigFile()
{
  this->CheckMPIController();
  //
  // Get the raw MPI_Comm handle
  //
  vtkMPICommunicator *communicator
  = vtkMPICommunicator::SafeDownCast(this->Controller->GetCommunicator());
  this->DsmManager->SetCommunicator(*communicator->GetMPIComm()->GetHandle());

#ifdef VTK_USE_MPI
  this->UpdatePiece     = this->Controller->GetLocalProcessId();
  this->UpdateNumPieces = this->Controller->GetNumberOfProcesses();
#else
  this->UpdatePiece     = 0;
  this->UpdateNumPieces = 1;
  vtkErrorMacro(<<"No MPI");
  return 0;
#endif

  return this->DsmManager->ReadDSMConfigFile();
}
//----------------------------------------------------------------------------
bool vtkDSMManager::CreateDSM()
{
  this->CheckMPIController();

  //
  // Get the raw MPI_Comm handle
  //
  vtkMPICommunicator *communicator
  = vtkMPICommunicator::SafeDownCast(this->Controller->GetCommunicator());
  this->DsmManager->SetCommunicator(*communicator->GetMPIComm()->GetHandle());

#ifdef VTK_USE_MPI
  this->UpdatePiece     = this->Controller->GetLocalProcessId();
  this->UpdateNumPieces = this->Controller->GetNumberOfProcesses();
#else
  this->UpdatePiece     = 0;
  this->UpdateNumPieces = 1;
  vtkErrorMacro(<<"No MPI");
  return 0;
#endif

  return this->DsmManager->CreateDSM();
}
//----------------------------------------------------------------------------
void vtkDSMManager::GenerateXMFDescription()
{
  XdmfGenerator *xdmfGenerator = new XdmfGenerator();

  if (this->GetDSMHandle()) {
    xdmfGenerator->SetDsmBuffer(this->GetDSMHandle());
    xdmfGenerator->Generate((const char*)this->GetXMFDescriptionFilePath(), "DSM:file.h5");
  }
  else {
    xdmfGenerator->Generate((const char*)this->GetXMFDescriptionFilePath(), "file.h5");
  }

  vtkDebugMacro(<< xdmfGenerator->GetGeneratedFile());

  if (this->GetDSMHandle()) this->GetDSMHandle()->SetXMLDescription(xdmfGenerator->GetGeneratedFile());
  delete xdmfGenerator;
}
//----------------------------------------------------------------------------
void vtkDSMManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
