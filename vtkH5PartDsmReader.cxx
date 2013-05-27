/*=========================================================================

  Project                 : Icarus
  Module                  : vtkH5PartDsmReader.cxx

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
#include "vtkH5PartDsmReader.h"
//
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkStreamingDemandDrivenPipeline.h"
//
#include "vtkDsmManager.h"
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
// #define PARALLEL_IO 1 must be before include of h5part
//
#define PARALLEL_IO 1
#include "H5Part.h"
#include "H5FDdsm.h"

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
vtkCxxSetObjectMacro(vtkH5PartDsmReader, DsmManager, vtkDsmManager);
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//#define JB_DEBUG__
#ifdef JB_DEBUG__
  #define OUTPUTTEXT(a) std::cout << (a) << std::endl; std::cout.flush();

    #undef vtkDebugMacro
    #define vtkDebugMacro(a)  \
    { \
      if (this->UpdatePiece>=0) { \
        vtkOStreamWrapper::EndlType endl; \
        vtkOStreamWrapper::UseEndl(endl); \
        vtkOStrStreamWrapper vtkmsg; \
        vtkmsg << "P(" << this->UpdatePiece << "): " a << "\n"; \
        OUTPUTTEXT(vtkmsg.str()); \
        vtkmsg.rdbuf()->freeze(0); \
      } \
    }

  #undef vtkErrorMacro
  #define vtkErrorMacro(a) vtkDebugMacro(a)  
#endif
//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkH5PartDsmReader);
//----------------------------------------------------------------------------
vtkH5PartDsmReader::vtkH5PartDsmReader()
{
  this->DsmManager = NULL;
}
//----------------------------------------------------------------------------
vtkH5PartDsmReader::~vtkH5PartDsmReader()
{
  this->SetController(NULL);
  this->SetDsmManager(NULL);
}
//----------------------------------------------------------------------------
void vtkH5PartDsmReader::CloseFile()
{
  if (this->H5FileId != NULL)
    {
    H5PartFile *f = this->H5FileId;
    // delete everything unless we're using cached stuff
	  if ( f->block && f->close_block ) {
		  (*f->close_block) ( f );
	  }
	  if ( f->multiblock && f->close_multiblock ) {
		  (*f->close_multiblock) ( f );
	  }
	  if( f->shape != H5S_ALL ) {
		  H5Sclose( f->shape );
	  }
	  if( f->timegroup >= 0 ) {
		  H5Gclose( f->timegroup );
	  }
	  if( f->diskshape != H5S_ALL ) {
		  H5Sclose( f->diskshape );
	  }
	  if( f->memshape != H5S_ALL ) {
		  H5Sclose( f->memshape );
	  }
	  if( f->xfer_prop != H5P_DEFAULT ) {
		  H5Pclose( f->xfer_prop );
	  }
    if( f->create_prop != H5P_DEFAULT ) {
	    H5Pclose( f->create_prop );
    }

    this->DsmManager->GetDsmManager()->CloseDSM();
    f->access_prop = H5I_BADID;
    f->file = H5I_BADID;

	  /* free memory from H5PartFile struct */
	  if( f->pnparticles ) {
		  free( f->pnparticles );
	  }
	  free( f );
    this->H5FileId = NULL;
  }
#ifdef ENABLE_TIMERS
  vtkMPICommunicator *communicator
  = vtkMPICommunicator::SafeDownCast(this->DsmManager->GetController()->GetCommunicator());
  communicator->Barrier();
  this->ReadTime = MPI_Wtime() - this->ReadTime;
  if (this->DsmManager->GetController()->GetLocalProcessId() == 0)
    std::cout << "H5Part Reader read time: " << this->ReadTime << std::endl;
  this->ReadTime = 0;
#endif
}
//----------------------------------------------------------------------------
void vtkH5PartDsmReader::CloseFileIntermediate()
{
  this->CloseFile();
}
//----------------------------------------------------------------------------
// This is a piece of code taken out of H5Part, but with lots of if()
// clauses removed since we know we are using parallel/dsm etc etc
//----------------------------------------------------------------------------
H5PartFile *vtkH5PartDsmReader::H5Part_open_file_dsm(const char *stepname) 
{
	H5PartFile *f = NULL;

	f = (H5PartFile*) malloc( sizeof (H5PartFile) );
	if ( f == NULL ) {
		goto error_cleanup;
	}
	memset (f, 0, sizeof (H5PartFile));
		f->multiblock = NULL;
		f->close_multiblock = NULL;

	f->flags = H5PART_READ;

	// set default step name 
	strncpy ( f->groupname_step, stepname, H5PART_STEPNAME_LEN );
	f->stepno_width = 0;

	f->comm = this->DsmManager->GetDsmManager()->GetMpiComm();
	if (MPI_Comm_size (f->comm, &f->nprocs) != MPI_SUCCESS) {
		goto error_cleanup;
	}
	if (MPI_Comm_rank (f->comm, &f->myproc) != MPI_SUCCESS) {
		goto error_cleanup;
	}

	f->pnparticles = (h5part_int64_t*) malloc (f->nprocs * sizeof (h5part_int64_t));
	if (f->pnparticles == NULL) {
		goto error_cleanup;
	}

  // If the DSM has been opened by some other object using the dsm manager
  // then we can make use of cached handles and save multiple open/closes
  f->xfer_prop = f->create_prop = H5P_DEFAULT;
  this->DsmManager->GetDsmManager()->OpenDSM(H5F_ACC_RDONLY);
	f->access_prop = this->DsmManager->GetDsmManager()->GetCachedFileAccessHandle();
  f->file = this->DsmManager->GetDsmManager()->GetCachedFileHandle();

  f->nparticles  = 0;
	f->timegroup   = -1;
	f->shape       = H5S_ALL;
	f->diskshape   = H5S_ALL;
	f->memshape    = H5S_ALL;
	f->viewstart   = -1;
	f->viewend     = -1;
	f->viewindexed = 0;
	f->throttle    = 0;

	return f;

 error_cleanup:
	if (f != NULL ) {
		if (f->pnparticles != NULL) {
			free (f->pnparticles);
		}
		free (f);
	}
	return NULL;
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
int vtkH5PartDsmReader::OpenFile()
{
#ifdef ENABLE_TIMERS
  vtkMPICommunicator *communicator
  = vtkMPICommunicator::SafeDownCast(this->DsmManager->GetController()->GetCommunicator());
  communicator->Barrier();
  this->ReadTime = MPI_Wtime();
#endif
  if (!this->DsmManager) {
    vtkWarningMacro("vtkH5PartDsmReader OpenFile aborted, no DSM manager");
  }

  this->H5FileId = H5Part_open_file_dsm(this->StepName ? this->StepName : "Step");
  this->FileOpenedTime.Modified();

  if (!this->H5FileId)
    {
    vtkErrorMacro(<< "Initialize: Could not open DSM");
    return 0;
    }

  //
  // When using DSM, we currently only allow step#0 to be opened
  // so force timestep to zero
  //
  this->ActualTimeStep = 0;

  return 1;
}
//----------------------------------------------------------------------------
int vtkH5PartDsmReader::ProcessRequest(vtkInformation *request,
    vtkInformationVector **inputVector,
    vtkInformationVector *outputVector)
{
  int result = 0;
  if (this->DsmManager) { 
    if (!this->DsmManager->GetDsmManager()->IsOpenDSM()) {
    this->DsmManager->GetDsmManager()->OpenDSM(H5F_ACC_RDONLY);
    result = this->Superclass::ProcessRequest(request, inputVector, outputVector);
    this->DsmManager->GetDsmManager()->CloseDSM();
    } else {
      result = this->Superclass::ProcessRequest(request, inputVector, outputVector);
    }
  } else {
    std::cout << "No DsmManager in H5PartDsmReader" << std::endl;
  }

  return result;
}
//----------------------------------------------------------------------------
int vtkH5PartDsmReader::RequestInformation(
  vtkInformation *request,
  vtkInformationVector **inputVector,
  vtkInformationVector *outputVector)
{
  int result = vtkH5PartReader::RequestInformation(request, inputVector, outputVector);
  //
//  vtkInformation* outInfo = outputVector->GetInformationObject(0);
//  outInfo->Remove(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
//  outInfo->Remove(vtkStreamingDemandDrivenPipeline::TIME_RANGE());
  return result;
}
//----------------------------------------------------------------------------
void vtkH5PartDsmReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
