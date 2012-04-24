/*=========================================================================

  Project                 : pv-meshless
  Module                  : vtkH5PartDsmReader.h
  Revision of last commit : $Rev: 884 $
  Author of last commit   : $Author: biddisco $
  Date of last commit     : $Date:: 2010-04-06 12:03:55 +0200 #$

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
#include "vtkH5PartDsmReader.h"
//
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkDataArraySelection.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkPolyData.h"
#include "vtkDataArray.h"
//
#include "vtkCharArray.h"
#include "vtkUnsignedCharArray.h"
#include "vtkShortArray.h"
#include "vtkUnsignedShortArray.h"
#include "vtkLongArray.h"
#include "vtkUnsignedLongArray.h"
#include "vtkLongLongArray.h"
#include "vtkUnsignedLongLongArray.h"
#include "vtkIntArray.h"
#include "vtkUnsignedIntArray.h"
#include "vtkFloatArray.h"
#include "vtkDoubleArray.h"
#include "vtkCellArray.h"
//
#include <vtksys/SystemTools.hxx>
#include <vtksys/RegularExpression.hxx>
#include <vtkstd/vector>
//
#include "vtkCharArray.h"
#include "vtkUnsignedCharArray.h"
#include "vtkCharArray.h"
#include "vtkShortArray.h"
#include "vtkUnsignedShortArray.h"
#include "vtkIntArray.h"
#include "vtkLongArray.h"
#include "vtkFloatArray.h"
#include "vtkDoubleArray.h"
#include "vtkSmartPointer.h"
#include "vtkExtentTranslator.h"
//
#include "vtkDummyController.h"
#include "vtkDsmManager.h"

//
// #define PARALLEL_IO 1 must be before include of h5part
//
#define PARALLEL_IO 1
#include "H5FDdsm.h"
#include "H5Part.h"
#include "H5PartTypes.h"
#include "H5PartErrors.h"
#include "string.h"
#include <algorithm>

//----------------------------------------------------------------------------
static unsigned			_debug = H5PART_VERB_ERROR;
static char *__funcname;
// we must be using parallel IO to be here
#define H5PART_GROUPNAME_STEP	"Step"
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
    H5PartCloseFile(this->H5FileId);
    this->H5FileId = NULL;
    }
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
static H5PartFile* H5Part_open_file_custom(const char *filename, const char flags, MPI_Comm comm) 
{
	H5PartFile *f = NULL;

	f = (H5PartFile*) malloc( sizeof (H5PartFile) );
	if ( f == NULL ) {
		goto error_cleanup;
	}
	memset (f, 0, sizeof (H5PartFile));
		f->multiblock = NULL;
		f->close_multiblock = NULL;

	f->flags = flags;

	/* set default step name */
	strncpy ( f->groupname_step, H5PART_GROUPNAME_STEP, H5PART_STEPNAME_LEN );
	f->stepno_width = 0;

	f->xfer_prop = f->create_prop = H5P_DEFAULT;
	f->access_prop = H5Pcreate (H5P_FILE_ACCESS);
	if (f->access_prop < 0) {
		goto error_cleanup;
	}

	if (MPI_Comm_size (comm, &f->nprocs) != MPI_SUCCESS) {
		goto error_cleanup;
	}
	if (MPI_Comm_rank (comm, &f->myproc) != MPI_SUCCESS) {
		goto error_cleanup;
	}

	f->pnparticles = (h5part_int64_t*) malloc (f->nprocs * sizeof (h5part_int64_t));
	if (f->pnparticles == NULL) {
		goto error_cleanup;
	}

	/* select the HDF5 VFD - JB force DSM */
	if (H5Pset_fapl_dsm ( f->access_prop, comm, NULL, 0 ) < 0) {
		goto error_cleanup;
	}

	f->comm = comm;
  f->file = H5Fopen(filename, H5F_ACC_RDONLY, f->access_prop);
	
	if (f->file < 0) {
		goto error_cleanup;
	}
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
  if (FileModifiedTime>FileOpenedTime)
    {
    this->CloseFile();
    }

  if (!this->H5FileId)
    {
    this->H5FileId = H5Part_open_file_custom(
      "DSM", H5PART_READ, this->DsmManager->GetDsmManager()->GetMpiComm());
    this->FileOpenedTime.Modified();
    }

  if (!this->H5FileId)
    {
    vtkErrorMacro(<< "Initialize: Could not open DSM");
    return 0;
    }

  return 1;
}
//----------------------------------------------------------------------------
void vtkH5PartDsmReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
