/*=========================================================================

  Project                 : Icarus
  Module                  : vtkBonsaiDsmManager.cxx

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
  Framework Programme (FP7/2007-2013) under grant agreement 225967 ���NextMuSE���

=========================================================================*/
#include "vtkBonsaiDsmManager.h"
//
#include "vtkObjectFactory.h"
//
#include "vtkProcessModule.h"
#include "vtkSMProxyManager.h"
#include "vtkClientServerInterpreterInitializer.h"
#include "vtkSMProxyDefinitionManager.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkPVSessionServer.h"
#include "vtkPVSessionCore.h"
//
// For PARAVIEW_USE_MPI
#include "vtkPVConfig.h"
#ifdef PARAVIEW_USE_MPI
 #include "vtkMPI.h"
 #include "vtkMPIController.h"
 #include "vtkMPICommunicator.h"
#endif
// Otherwise
#include "vtkMultiProcessController.h"
//
#include "vtkDsmProxyHelper.h"
#include "vtkPVOptions.h"
#include "vtkClientSocket.h"

#include "vtkMultiThreader.h"
#include "vtkConditionVariable.h"
//
//----------------------------------------------------------------------------
#include "BonsaiSharedData.h"
#include "BonsaiIO.h"
#include "SharedMemory.h"
#include "ParaViewData.h"
//----------------------------------------------------------------------------
using ShmQHeader = SharedMemoryClient<BonsaiSharedQuickHeader>;
using ShmQData   = SharedMemoryClient<BonsaiSharedQuickData>;
static ShmQHeader *shmQHeader = NULL;
static ShmQData   *shmQData   = NULL;
static bool terminateRenderer = false;
static bool first = true;
static float tLast = -1.0f;
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkBonsaiDsmManager);
//----------------------------------------------------------------------------
// We need stdlibc++ and we can't use c++11 for this file due to the boost stuff
//
/* skip particles that failed to get density, or with too big h */
bool skipPtcl(ShmQData &data, const int i)
{
  return (data[i].rho == 0 || data[i].h == 0.0 || data[i].h > 100);
};
//----------------------------------------------------------------------------
bool vtkBonsaiDsmManager::WaitForNewData(const bool quickSync,
    const int rank, const int nrank)
{
  auto &header = *shmQHeader;
  auto &data   = *shmQData;

  header.acquireLock();
  header.releaseLock();

  return true;
}

//----------------------------------------------------------------------------
bool vtkBonsaiDsmManager::fetchSharedData(const bool quickSync, ParaViewData *rData,
    const int rank, const int nrank, const MPI_Comm &comm,
    const int reduceDM, const int reduceS)
{
  auto &header = *shmQHeader;
  auto &data   = *shmQData;

  std::cout << "Entering fetchSharedData on rank " << rank << " of " << nrank << std::endl;
  // rank 0 acquires lock in notification thread polling for new data
  // other ranks get the lock once rank 0 signals the gui to go ahead.
//  if (rank>0) {
    // header
    header.acquireLock();
//  }
//  std::cout << "Fetched lock on rank " << rank << " of " << nrank << std::endl;

#if 0
  //  if (rank == 0)
  fprintf(stderr, " rank= %d: attempting to fetch data \n",rank);
#endif

  const float tCurrent = header[0].tCurrent;

  terminateRenderer = tCurrent == -1;

  int sumL = quickSync ? !header[0].done_writing : tCurrent != tLast;
  int sumG ;
  MPI_Allreduce(&sumL, &sumG, 1, MPI_INT, MPI_SUM, comm);


  bool completed = false;
  if (sumG == nrank) //tCurrent != tLast)
  {
    tLast = tCurrent;
    completed = true;

    // data
    const size_t nBodies = header[0].nBodies;
    data.acquireLock();

    const size_t size = data.size();
    assert(size == nBodies);
/*
    // skip particles that failed to get density, or with too big h
    dsm_std::function<bool(const int i)> skipPtcl = [&](const int i)
    {
      return (data[i].rho == 0 || data[i].h == 0.0 || data[i].h > 100);
    };
*/

    size_t nDM = 0, nS = 0;
    const int ntypecount = 10;
    dsm_std::array<size_t,ntypecount> ntypeloc, ntypeglb;
    std::fill(ntypeloc.begin(), ntypeloc.end(), 0);
    for (size_t i = 0; i < size; i++)
    {
      const int type = data[i].ID.getType();
      if  (type < ntypecount)
        ntypeloc[type]++;
      if (skipPtcl(data,i))
        continue;
      switch (type)
      {
        case 0:
          nDM++;
          break;
        default:
          nS++;
      }
    }

    MPI_Reduce(&ntypeloc, &ntypeglb, ntypecount, MPI_LONG_LONG, MPI_SUM, 0, comm);
    if (rank == 0)
    {
      for (int type = 0; type < ntypecount; type++)
        if (ntypeglb[type] > 0)
          fprintf(stderr, " ptype= %d:  np= %zu \n",type, ntypeglb[type]);
    }


    rData->resize(nS);
    size_t ip = 0;
    float *coords = rData->coords->GetPointer(0);
    float *mass   = rData->mass->GetPointer(0);
    float *vel    = rData->vel->GetPointer(0);
    float *rho    = rData->rho->GetPointer(0);
    float *Hval   = rData->Hval->GetPointer(0);
    vtkIdType *Id = rData->Id->GetPointer(0);

    for (size_t i = 0; i < size; i++)
    {
      if (skipPtcl(data,i))
        continue;
      if (data[i].ID.getType() == 0 )  /* pick stars only */
        continue;

      coords[ip*3+0] = data[i].x;
      coords[ip*3+1] = data[i].y;
      coords[ip*3+2] = data[i].z;
      mass[ip]       = data[i].mass;
      vel[ip]        = std::sqrt(
            data[i].vx*data[i].vx+
            data[i].vy*data[i].vy+
            data[i].vz*data[i].vz);

      Id[ip]         = data[i].ID.get();
      Hval[ip]       = data[i].h;
      rho[ip]        = data[i].rho;

      ip++;
      assert(ip <= nS);
    }
    rData->resize(ip);

    data.releaseLock();
  }

  header[0].done_writing = true;
  header.releaseLock();

#if 0
  //  if (rank == 0)
  fprintf(stderr, " rank= %d: done fetching data \n", rank);
#endif

  return completed;
}

//----------------------------------------------------------------------------
vtkBonsaiDsmManager::vtkBonsaiDsmManager()
{
//  std::cout << "Created a Bonsai DSM manager" << std::endl;
    this->ThreadActive = 0;
}

//----------------------------------------------------------------------------
vtkBonsaiDsmManager::~vtkBonsaiDsmManager()
{ 
  if (this->ThreadActive) {
    std::cout <<" Must shut down polling thread" << std::endl;
  }
}

//----------------------------------------------------------------------------
int vtkBonsaiDsmManager::CreateSharedMemStructures(int quickSync)
{
  if (shmQHeader == NULL)
  {
    shmQHeader = new ShmQHeader(ShmQHeader::type::sharedFile(this->UpdatePiece));
    shmQData   = new ShmQData  (ShmQData  ::type::sharedFile(this->UpdatePiece));
  }
  std::cout << "Created shared mem on rank " << this->UpdatePiece << " of " << this->UpdateNumPieces << std::endl;

  auto &header = *shmQHeader;
  auto &data   = *shmQData;

  if (quickSync && first)
  {
    /* handshake */

    header.acquireLock();
    header[0].handshake = true;
    header.releaseLock();

    while (header[0].handshake)
      usleep(1000);

    header.acquireLock();
    header[0].handshake = true;
    header.releaseLock();

    /* handshake complete */
    first = false;
  }
  std::cout << "initialized handshake on rank " << this->UpdatePiece << " of " << this->UpdateNumPieces << std::endl;
}
//----------------------------------------------------------------------------
int vtkBonsaiDsmManager::Publish()
{
  this->CreateSharedMemStructures(true);
  //
  if (this->UpdatePiece == 0) {
    this->DSMPollingFunction = dsm_std::bind(&vtkBonsaiDsmManager::PollingBonsai, this, _1);

    this->DSMPollingThread = dsm_std::thread(&vtkAbstractDsmManager::NotificationThread, this);
    this->WaitForNotifThreadCreated();
  }
  //
  // make sure all ranks are initialized safely before going into main operation
  //
  vtkMPICommunicator *communicator
    = vtkMPICommunicator::SafeDownCast(this->GetController()->GetCommunicator());
  communicator->Barrier();
  //
  return 1;
}

//----------------------------------------------------------------------------
bool vtkBonsaiDsmManager::PollingBonsai(unsigned int *flag)
{
  usleep(1000);
  std::cout << "Polling for new data on rank " << this->UpdatePiece << " of " << this->UpdateNumPieces << std::endl;
  bool temp = vtkBonsaiDsmManager::WaitForNewData(true, this->UpdatePiece, this->UpdateNumPieces);
  std::cout << "Got new data " << std::endl;
  *flag = DSM_NOTIFY_DATA;
  return 1;
}
