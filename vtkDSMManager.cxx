/*=========================================================================

  Project                 : vtkCSCS
  Module                  : vtkDSMManager.h
  Revision of last commit : $Rev: 793 $
  Author of last commit   : $Author: biddisco $
  Date of last commit     : $Date:: 2009-02-19 12:15:43 +0100 #$

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
#include "vtkDSMManager.h"
//
#include "vtkObjectFactory.h"
//
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

#include "XdmfDsmCommMpi.h"
#include "XdmfDsmMsg.h"

typedef void* (*servicefn)(void *DsmObj) ;
//----------------------------------------------------------------------------
// #ifdef JB_DEBUG__
//----------------------------------------------------------------------------
  #ifdef AAWIN32
      #define OUTPUTTEXT(a) vtkOutputWindowDisplayText(a);
  #else
      #define OUTPUTTEXT(a) std::cout << (a) << "\n"; std::cout.flush();
  #endif

    #undef vtkDebugMacro
    #define vtkDebugMacro(a)  \
    { \
      vtkOStreamWrapper::EndlType endl; \
      vtkOStreamWrapper::UseEndl(endl); \
      vtkOStrStreamWrapper vtkmsg; \
      vtkmsg a << "\n"; \
      OUTPUTTEXT(vtkmsg.str()); \
      vtkmsg.rdbuf()->freeze(0); \
    }

  #undef vtkErrorMacro
  #define vtkErrorMacro(a) vtkDebugMacro(a)  
// #endif
//----------------------------------------------------------------------------
vtkCxxRevisionMacro(vtkDSMManager, "$Revision: 793 $");
vtkStandardNewMacro(vtkDSMManager);
//----------------------------------------------------------------------------
vtkDSMManager::vtkDSMManager() 
{
  this->DSMBuffer                = NULL;
  this->LocalBufferSizeMBytes    = 128;
  this->UpdatePiece              = 0;
  this->UpdateNumPieces          = 0;
#ifdef HAVE_PTHREADS
  this->ServiceThread            = 0;
#elif HAVE_BOOST_THREADS
  this->ServiceThread            = 0;
#endif
  //
#ifdef VTK_USE_MPI
  this->Controller = NULL;
  this->DSMComm = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());
#endif
  //
  this->NumberOfTimeSteps        = 0;
  this->TimeStep                 = 0;
  this->FileName                 = NULL;
}
//----------------------------------------------------------------------------
vtkDSMManager::~vtkDSMManager()
{ 
  this->DestroyDSM(); 
  //
  if (this->FileName) {
    delete [] this->FileName;
    this->FileName = NULL;
  }
#ifdef VTK_USE_MPI
  this->SetController(NULL);
#endif
}
//----------------------------------------------------------------------------
bool vtkDSMManager::DestroyDSM()
{
#ifdef VTK_USE_MPI
  if(this->UpdatePiece == 0)
    this->DSMBuffer->SendDone();
#endif
#ifdef HAVE_PTHREADS
  pthread_join(this->ServiceThread, NULL);
  this->ServiceThread = NULL;
#elif HAVE_BOOST_THREADS
  delete this->ServiceThread;
  this->ServiceThread = NULL;
#endif
  delete this->DSMComm;
  this->DSMComm = NULL;
  delete this->DSMBuffer;
  this->DSMBuffer = NULL;
  return true;
}
//----------------------------------------------------------------------------
XdmfDsmBuffer *vtkDSMManager::GetDSMHandle()
{
  return DSMBuffer;
}
//----------------------------------------------------------------------------
class DSMServiceThread 
{
  public:
    DSMServiceThread(XdmfDsmBuffer *dsmObject)
    {
      this->DSMObject = dsmObject;
    }
    void operator()() {
      this->DSMObject->ServiceThread();
    }
    //
    XdmfDsmBuffer *DSMObject;
};
//----------------------------------------------------------------------------
bool vtkDSMManager::CreateDSM()
{
  if (this->DSMBuffer) {
    return true;
  }

  if (!this->GetController())
    {
    vtkErrorMacro(<<"No MPI Controller specified");
    return false;
    }

  //
  // Get the raw MPI_Comm handle
  //
  vtkMPICommunicator *communicator
    = vtkMPICommunicator::SafeDownCast(this->GetController()->GetCommunicator());
  if (!communicator)
    {
    vtkErrorMacro(<<"No MPI communicator specified");
    return false;
    }
  MPI_Comm mpiComm = *communicator->GetMPIComm()->GetHandle();

  //
  // Create Xdmf DSM communicator
  //
  this->DSMComm = new XdmfDsmCommMpi;
  this->DSMComm->DupComm(mpiComm);
  this->DSMComm->Init();
  int rank = this->DSMComm->GetId();
  int size = this->DSMComm->GetTotalSize();
  int last_rank = size - 1;

  //
  // Create the DSM buffer
  //
  this->DSMBuffer = new XdmfDsmBuffer();
  // Uniform Dsm : every node has a buffer the same size. (Addresses are sequential)
  this->DSMBuffer->ConfigureUniform(this->DSMComm, this->LocalBufferSizeMBytes*1024*1024);

  //
  // setup service thread
  //
#ifdef HAVE_PTHREADS
  // Start another thread to handle DSM requests from other nodes
  pthread_create(&this->ServiceThread, NULL, &XdmfDsmBufferServiceThread, (void *) this->DSMBuffer);
#elif HAVE_BOOST_THREADS
    DSMServiceThread MyDSMServiceThread(this->DSMBuffer);
    this->ServiceThread = new boost::thread(MyDSMServiceThread);
#endif

  // Wait for DSM to be ready
  while (!this->DSMBuffer->GetThreadDsmReady()) {
    // Spin until service initialized
  }
  vtkDebugMacro(<<"DSM Service Ready on " << rank);
  return true;
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void vtkDSMManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  //os << indent << "FileName: " <<
  //  (this->FileName ? this->FileName : "(none)") << "\n";

  //os << indent << "NumberOfSteps: " <<  this->NumberOfTimeSteps << "\n";
}
