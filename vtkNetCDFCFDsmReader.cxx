#include "vtkNetCDFCFDsmReader.h"
#include "vtkObjectFactory.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkSmartPointer.h"
#include "netcdf.h"
#include "vtkHDF5DsmManager.h"
#include "IcarusConfig.h"
#include <vector>

extern "C" {
#include <netcdf_par.h>
}

#define CALL_NETCDF_GENERIC(call, on_error) \
  { \
    int errorcode = call; \
    if (errorcode != NC_NOERR) \
      { \
      const char * errorstring = nc_strerror(errorcode); \
      on_error; \
      } \
  }

#define CALL_NETCDF(call) \
  CALL_NETCDF_GENERIC(call, vtkErrorMacro(<< "netCDF Error: " << errorstring); return 0;)

#define CALL_NETCDF_GW(call) \
  CALL_NETCDF_GENERIC(call, vtkGenericWarningMacro(<< "netCDF Error: " << errorstring); return 0;)

//=============================================================================
//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkNetCDFCFDsmReader);
vtkCxxSetObjectMacro(vtkNetCDFCFDsmReader, DsmManager, vtkHDF5DsmManager);
//----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
vtkNetCDFCFDsmReader::vtkNetCDFCFDsmReader()
{
  this->DsmManager = NULL;
}

//-----------------------------------------------------------------------------
vtkNetCDFCFDsmReader::~vtkNetCDFCFDsmReader()
{
}
//-----------------------------------------------------------------------------
void vtkNetCDFCFDsmReader::SetFileModified()
{
  this->Modified();
  this->FileNameMTime.Modified();
}
//-----------------------------------------------------------------------------
int vtkNetCDFCFDsmReader::OpenFile(const char *name, int mode, int *id)
{
  if (this->DsmManager) {
    MPI_Comm comm = this->DsmManager->GetDsmBuffer()->GetComm()->GetIntraComm();
    CALL_NETCDF(nc_open_par("dsm", mode | NC_NETCDF4 | NC_DSMHDF5, comm, NULL, id));
    return 1;
  }
  return 0;
}
//-----------------------------------------------------------------------------
int vtkNetCDFCFDsmReader::RequestInformation(
                                 vtkInformation *request,
                                 vtkInformationVector **inputVector,
                                 vtkInformationVector *outputVector)
{
  if (!vtkNetCDFCFReader::RequestInformation(request,inputVector,outputVector)) {
    return 0;
  }
  //
  vtkInformation *outInfo = outputVector->GetInformationObject(0);
  if (outInfo->Has(vtkStreamingDemandDrivenPipeline::TIME_STEPS())) 
  {
    int l = outInfo->Length(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
    std::vector<double> timeValues(l,0.0);
    outInfo->Get(vtkStreamingDemandDrivenPipeline::TIME_STEPS(), &timeValues[0]);
  }
  return 1;
}
