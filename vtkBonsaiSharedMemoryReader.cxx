/*=========================================================================

  Project                 : pv-meshless
  Module                  : vtkBonsaiSharedMemoryReader.h
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
// For PARAVIEW_USE_MPI 
#include "vtkPVConfig.h"     
#ifdef PARAVIEW_USE_MPI
  #include "vtkMPI.h"
  #include "vtkMPIController.h"
  #include "vtkMPICommunicator.h"
#endif
#include "vtkDummyController.h"
//
#include "vtkBonsaiSharedMemoryReader.h"
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
#include "vtkOutlineSource.h"
#include "vtkAppendPolyData.h"
#include "vtkBoundingBox.h"
//
#include <vtksys/SystemTools.hxx>
#include <vtksys/RegularExpression.hxx>
#include <vector>
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
#include "vtkBoundingBox.h"
#include "vtkIdListCollection.h"
//
//#include <functional>
#include <algorithm>
#include <numeric>
//----------------------------------------------------------------------------
vtkCxxSetObjectMacro(vtkBonsaiSharedMemoryReader, Controller, vtkMultiProcessController);
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
vtkStandardNewMacro(vtkBonsaiSharedMemoryReader);
//----------------------------------------------------------------------------
vtkBonsaiSharedMemoryReader::vtkBonsaiSharedMemoryReader()
{
  this->SetNumberOfInputPorts(0);
  //
  this->NumberOfTimeSteps               = 0;
  this->TimeStep                        = 0;
  this->ActualTimeStep                  = 0;
  this->TimeStepTolerance               = 1E-6;
  this->GenerateVertexCells             = 0;
  this->FileName                        = NULL;
  this->UpdatePiece                     = 0;
  this->UpdateNumPieces                 = 0;
  this->TimeOutOfRange                  = 0;
  this->MaskOutOfTimeRangeOutput        = 0;
  this->IntegerTimeStepValues           = 0;
  this->DisplayPieceBoxes               = 0;
  this->PointDataArraySelection         = vtkDataArraySelection::New();
  this->Controller = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());
  if (this->Controller == NULL) {
    this->SetController(vtkSmartPointer<vtkDummyController>::New());
  }
}
//----------------------------------------------------------------------------
vtkBonsaiSharedMemoryReader::~vtkBonsaiSharedMemoryReader()
{
  this->CloseFile();
  delete [] this->FileName;
  this->FileName = NULL;
  
  this->PointDataArraySelection->FastDelete();
  this->PointDataArraySelection = 0;

  this->SetController(NULL);
}

//----------------------------------------------------------------------------
void vtkBonsaiSharedMemoryReader::SetFileName(char *filename)
{
  if (this->FileName == NULL && filename == NULL)
    {
    return;
    }
  if (this->FileName && filename && (!strcmp(this->FileName,filename)))
    {
    return;
    }
  delete [] this->FileName;
  this->FileName = NULL;

  if (filename)
    {
    this->FileName = vtksys::SystemTools::DuplicateString(filename);
    this->SetFileModified();
    }
  this->Modified();
}
//----------------------------------------------------------------------------
void vtkBonsaiSharedMemoryReader::SetFileModified()
{
  this->FileModifiedTime.Modified();
  this->Modified();
}

//----------------------------------------------------------------------------
int vtkBonsaiSharedMemoryReader::OpenFile()
{
  if (!this->FileName)
    {
    vtkErrorMacro(<<"FileName must be specified.");
    return 0;
    }

  if (FileModifiedTime>FileOpenedTime)
    {
    this->CloseFile();
    }

  return 1;
}

//----------------------------------------------------------------------------
int vtkBonsaiSharedMemoryReader::RequestInformation(
  vtkInformation *vtkNotUsed(request),
  vtkInformationVector **vtkNotUsed(inputVector),
  vtkInformationVector *outputVector)
{
  vtkInformation *outInfo = outputVector->GetInformationObject(0);
  //
  this->UpdatePiece = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER());
  this->UpdateNumPieces = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES());
  //
  outInfo->Set(CAN_HANDLE_PIECE_REQUEST(), 1);
  bool NeedToReadInformation = (FileModifiedTime>FileOpenedTime);

  if (NeedToReadInformation)
    {
    if (!this->OpenFile())
      {
      return 0;
      }

    this->NumberOfTimeSteps = 1;
    H5PartSetStep(this->H5FileId, 0);
    int nds = H5PartGetNumDatasets(this->H5FileId);
    char name[512];
    for (int i=0; i<nds; i++)
      {
      // return 0 for no, 1,2,3,4,5 etc for index (1 based offset)
      H5PartGetDatasetName(this->H5FileId, i, name, 512);
      this->PointDataArraySelection->AddArray(name);
      }

    this->TimeStepValues.assign(this->NumberOfTimeSteps, 0.0);
    int validTimes = 0;
    for (int i=0; i<this->NumberOfTimeSteps; ++i)
      {
      H5PartSetStep(this->H5FileId, i);
      // Get the time value if it exists
      h5part_int64_t numAttribs = H5PartGetNumStepAttribs(this->H5FileId);
      if (numAttribs>0)
        {
        char           attribName[128];
        h5part_int64_t attribNameLength = 128;
        h5part_int64_t attribType       = 0;
        h5part_int64_t attribNelem      = 0;
        for (h5part_int64_t a=0; a<numAttribs; a++)
          {
          h5part_int64_t status = H5PartGetStepAttribInfo (
            this->H5FileId, a, attribName, attribNameLength,
            &attribType, &attribNelem);
          if (status==H5PART_SUCCESS && !strncmp("TimeValue",attribName,attribNameLength))
            {
            if (H5Tequal(attribType,H5T_NATIVE_DOUBLE) && attribNelem==1)
              {
              status=H5PartReadStepAttrib(this->H5FileId, attribName, &this->TimeStepValues[i]);
              if (status==H5PART_SUCCESS)
                {
                validTimes++;
                }
              }
            }
          }
        }
      }
    H5PartSetStep(this->H5FileId, 0);

    if (this->NumberOfTimeSteps==0)
      {
      vtkErrorMacro(<<"No time steps in data");
      return 0;
      }

    // if TIME information was either not present ot not consistent, then
    // set something so that consumers of this data can iterate sensibly
    if (this->IntegerTimeStepValues || (this->NumberOfTimeSteps>0 && this->NumberOfTimeSteps!=validTimes))
      {
      for (int i=0; i<this->NumberOfTimeSteps; i++)
        {
        // insert read of Time array here
        this->TimeStepValues[i] = i;
        }
      }
    outInfo->Set(vtkStreamingDemandDrivenPipeline::TIME_STEPS(),
      &this->TimeStepValues[0],
      static_cast<int>(this->TimeStepValues.size()));
    double timeRange[2];
    timeRange[0] = this->TimeStepValues.front();
    timeRange[1] = this->TimeStepValues.back();
    if (this->TimeStepValues.size()>1)
      {
      this->TimeStepTolerance = 0.01*(this->TimeStepValues[1]-this->TimeStepValues[0]);
      }
    else
      {
      this->TimeStepTolerance = 1E-3;
      }
    outInfo->Set(vtkStreamingDemandDrivenPipeline::TIME_RANGE(), timeRange, 2);

    //
    // If the file has bounding box partition support
    //
    vtkIdType partitions = this->IgnorePartitionBoxes ? 0 : this->ReadBoundingBoxes();
    if (partitions>0) 
      {
      outInfo->Set(vtkBoundsExtentTranslator::META_DATA(), this->ExtentTranslator);
      }
    else {
      this->PartitionCount.clear();
      this->PartitionOffset.clear();
      this->PieceId.clear();
      this->PartitionBoundsTable.clear();
      this->PartitionBoundsTableHalo.clear();
    }
  }

  this->CloseFileIntermediate();
                                                       
  return 1;
}

//----------------------------------------------------------------------------
template <class T1, class T2>
void CopyIntoTuple(int offset, vtkDataArray *source, vtkDataArray *dest)
{
  vtkIdType N = source->GetNumberOfTuples();
  T1 *sptr = static_cast<T1*>(source->GetVoidPointer(0));
  T2 *dptr = static_cast<T2*>(dest->WriteVoidPointer(0,N)) + offset;
  for (vtkIdType i=0; i<N; ++i) {
    *dptr = *sptr++;
    dptr += 3;
  }
}
//----------------------------------------------------------------------------
template <class T2>
void vtkBonsaiSharedMemoryReader::CopyIntoVector(int offset, vtkDataArray *source, vtkDataArray *dest)
{
  switch (source->GetDataType())
  {
    case VTK_CHAR:
    case VTK_SIGNED_CHAR:
    case VTK_UNSIGNED_CHAR:
      CopyIntoTuple<char,T2>(offset, source, dest);
      break;
    case VTK_SHORT:
      CopyIntoTuple<short int,T2>(offset, source, dest);
      break;
    case VTK_UNSIGNED_SHORT:
      CopyIntoTuple<unsigned short int,T2>(offset, source, dest);
      break;
    case VTK_INT:
      CopyIntoTuple<int,T2>(offset, source, dest);
      break;
    case VTK_UNSIGNED_INT:
      CopyIntoTuple<unsigned int,T2>(offset, source, dest);
      break;
    case VTK_LONG:
      CopyIntoTuple<long int,T2>(offset, source, dest);
      break;
    case VTK_UNSIGNED_LONG:
      CopyIntoTuple<unsigned long int,T2>(offset, source, dest);
      break;
    case VTK_LONG_LONG:
      CopyIntoTuple<long long,T2>(offset, source, dest);
      break;
    case VTK_UNSIGNED_LONG_LONG:
      CopyIntoTuple<unsigned long long,T2>(offset, source, dest);
      break;
    case VTK_FLOAT:
      CopyIntoTuple<float,T2>(offset, source, dest);
      break;
    case VTK_DOUBLE:
      CopyIntoTuple<double,T2>(offset, source, dest);
      break;
    case VTK_ID_TYPE:
      CopyIntoTuple<vtkIdType,T2>(offset, source, dest);
      break;
    default:
      break;
      vtkErrorMacro(<<"Unexpected data type");
  }
}
//----------------------------------------------------------------------------
/*
std::pair<double, double> GetClosest(std::vector<double> &sortedlist, const double& val) const
{
  std::vector<double>::const_iterator it = std::lower_bound(sortedlist.begin(), sortedlist.end(), val);
  if (it == sortedlist.end())        return std::make_pair(sortedlist.back(), sortedlist.back());
  else if (it == sortedlist.begin()) return std::make_pair(sortedlist.front(), sortedlist.front());
  else return std::make_pair(*(it - 1), *(it));
}
*/
class BonsaiToleranceCheck: public std::binary_function<double, double, bool>
{
public:
  BonsaiToleranceCheck(double tol) { this->tolerance = tol; }
  double tolerance;
  //
    result_type operator()(first_argument_type a, second_argument_type b) const
    {
      bool result = (fabs(a-b)<=(this->tolerance));
      return (result_type)result;
    }
};

//----------------------------------------------------------------------------
int vtkBonsaiSharedMemoryReader::RequestData(
  vtkInformation *vtkNotUsed(request),
  vtkInformationVector **vtkNotUsed(inputVector),
  vtkInformationVector *outputVector)
{
  vtkInformation *outInfo = outputVector->GetInformationObject(0);
  vtkPolyData     *output = vtkPolyData::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));
  //
  this->UpdatePiece = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER());
  this->UpdateNumPieces = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES());
  //
  typedef std::map< std::string, std::vector<std::string> > FieldMap;
  FieldMap scalarFields;
  //
  if (this->TimeStepValues.size()==0) return 0;
  //
  // Make sure that the user selected arrays for coordinates are represented
  //
  std::vector<std::string> coordarrays(3, "");
  //
  int N = this->PointDataArraySelection->GetNumberOfArrays();
  for (int i=0; i<N; i++)
    {
    const char *name = this->PointDataArraySelection->GetArrayName(i);
    // Do we want to load this array
    bool processarray = false;
    if (!vtksys::SystemTools::Strucmp(name,this->Xarray))
      {
      processarray = true;
      coordarrays[0] = name;
      }
    if (!vtksys::SystemTools::Strucmp(name,this->Yarray))
      {
      processarray = true;
      coordarrays[1] = name;
      }
    if (!vtksys::SystemTools::Strucmp(name,this->Zarray))
      {
      processarray = true;
      coordarrays[2] = name;
      }
    if (this->PointDataArraySelection->ArrayIsEnabled(name))
      {
      processarray = true;
      }
    if (!processarray)
      {
      continue;
      }

    // make sure we cater for multi-component vector fields
    int vectorcomponent;
    if ((vectorcomponent=this->IndexOfVectorComponent(name))>0)
      {
      std::string vectorname = this->NameOfVectorComponent(name) + "_v";
      FieldMap::iterator pos = scalarFields.find(vectorname);
      if (pos==scalarFields.end())
        {
        std::vector<std::string> arraylist(1, name);
        FieldMap::value_type element(vectorname, arraylist);
        scalarFields.insert(element);
        }
      else
        {
        pos->second.reserve(vectorcomponent);
        pos->second.resize(std::max((int)(pos->second.size()), vectorcomponent));
        pos->second[vectorcomponent-1] = name;
        }
      }
    else
      {
      std::vector<std::string> arraylist(1, name);
      FieldMap::value_type element(name, arraylist);
      scalarFields.insert(element);
      }
    }
  //
  FieldMap::iterator coordvector=scalarFields.end();
  for (FieldMap::iterator pos=scalarFields.begin(); pos!=scalarFields.end(); ++pos)
    {
    if (pos->second.size()==3 &&
      (pos->second[0]==coordarrays[0]) &&
      (pos->second[1]==coordarrays[1]) &&
      (pos->second[2]==coordarrays[2]))
      {
      // change the keyname of this entry to "coords" to ensure we use it as such
      FieldMap::value_type element("Coords", pos->second);
      scalarFields.erase(pos);
      coordvector = scalarFields.insert(element).first;
      break;
      }
    }

  if (coordvector==scalarFields.end())
    {
    FieldMap::value_type element("Coords", coordarrays);
    scalarFields.insert(element);
    }

  if (!this->MultiComponentArraysAsFieldData) {
    FieldMap::iterator posx=scalarFields.find(coordarrays[0]);
    if (posx!=scalarFields.end()) scalarFields.erase(posx);
    FieldMap::iterator posy=scalarFields.find(coordarrays[1]);
    if (posy!=scalarFields.end()) scalarFields.erase(posy);
    FieldMap::iterator posz=scalarFields.find(coordarrays[2]);
    if (posz!=scalarFields.end()) scalarFields.erase(posz);
  }
  //
  // Get the TimeStep Requested from the information if present
  //
  this->TimeOutOfRange = 0;
  this->ActualTimeStep = this->TimeStep;
  if (outInfo->Has(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP()))
    {
    double requestedTimeValue = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP());
    this->ActualTimeStep = std::find_if(
      this->TimeStepValues.begin(), this->TimeStepValues.end(),
      std::bind2nd( H5PartToleranceCheck( 
          this->IntegerTimeStepValues ? 0.5 : this->TimeStepTolerance ), requestedTimeValue ))
      - this->TimeStepValues.begin();
    //
    if (requestedTimeValue<this->TimeStepValues.front() || requestedTimeValue>this->TimeStepValues.back())
      {
      this->TimeOutOfRange = 1;
      }
    output->GetInformation()->Set(vtkDataObject::DATA_TIME_STEP(), requestedTimeValue);
    }
  else
    {
    double timevalue[1];
    unsigned int index = this->ActualTimeStep;
    if (index<this->TimeStepValues.size())
      {
      timevalue[0] = this->TimeStepValues[index];
      }
    else
      {
      timevalue[0] = this->TimeStepValues[0];
      }
    output->GetInformation()->Set(vtkDataObject::DATA_TIME_STEP(), timevalue[0]);
    }

  if (this->TimeOutOfRange && this->MaskOutOfTimeRangeOutput)
    {
    // don't do anything, just return success
    return 1;
    }

  // open the file if not already done
  if (!this->OpenFile())
    {
    return 0;
    }

  // Set the TimeStep on the H5 file
  H5PartSetStep(this->H5FileId, this->ActualTimeStep);
  //
  // Get the number of particles for this timestep
  //
  vtkIdType Nparticles = H5PartGetNumParticles(this->H5FileId);

  //
  // Split particles up per process for parallel load
  //
  std::vector<vtkIdType> minIds, maxIds, Ids;
  vtkIdType ParticleStart;
  vtkIdType ParticleEnd;
  //
  if (this->PartitionCount.size()>0 && this->PartitionByBoundingBoxes(minIds,maxIds,this->PieceBounds,this->PieceBoundsHalo)) {
    ParticleStart = minIds[this->UpdatePiece];
    ParticleEnd   = maxIds[this->UpdatePiece];
    this->ExtentTranslator->SetBoundsHalosEnabled(1);
    this->ExtentTranslator->SetNumberOfPieces(this->PieceBounds.size());
    for (int i=0; i<this->PieceBounds.size(); i++) {
      double bounds[6];
      this->PieceBounds[i].GetBounds(bounds);
      this->ExtentTranslator->SetBoundsForPiece(i, bounds);
      this->PieceBoundsHalo[i].GetBounds(bounds);
      this->ExtentTranslator->SetBoundsHaloForPiece(i, bounds);
    }
    this->ExtentTranslator->InitWholeBounds();
  }
  else {
    if (this->RandomizePartitionExtents) {
      this->PartitionByExtentsRandomized(Nparticles, Ids);
    }
    else {
      this->PartitionByExtents(Nparticles, Ids);
    }
    ParticleStart = Ids[0];
    ParticleEnd   = Ids[1];
  }
  vtkIdType            Nt = ParticleEnd - ParticleStart + 1;
  //

  // Setup arrays for reading data
  vtkSmartPointer<vtkPoints>    points = vtkSmartPointer<vtkPoints>::New();
  vtkSmartPointer<vtkDataArray> coords = NULL;
  for (FieldMap::iterator it=scalarFields.begin(); it!=scalarFields.end(); it++)
    {
    // use the type of the first array for all if it is a vector field
    std::vector<std::string> &arraylist = (*it).second;
    const char *array_name = arraylist[0].c_str();
    std::string rootname = this->NameOfVectorComponent(array_name);
    int Nc = static_cast<int>(arraylist.size());
    //
    vtkSmartPointer<vtkDataArray> dataarray = NULL;
    hid_t datatype = H5PartGetNativeDatasetType(H5FileId,array_name);
    int vtk_datatype = GetVTKDataType(datatype);

    if (vtk_datatype == VTK_VOID)
      {
      H5Tclose(datatype);
      vtkErrorMacro("An unexpected data type was encountered");
      return 0;
      }

    dataarray.TakeReference(vtkDataArray::CreateDataArray(vtk_datatype));
    dataarray->SetNumberOfComponents(Nc);
    dataarray->SetNumberOfTuples(Nt);
    dataarray->SetName(rootname.c_str());

    // now read the data components.
    herr_t r;
    hsize_t count1_mem[] = { Nt*Nc };
    hsize_t count2_mem[] = { Nt };
    hsize_t offset_mem[] = { 0 };
    hsize_t stride_mem[] = { Nc };
    hsize_t     count1_dsk[] = { Nt };
    hsize_t     offset_dsk[] = { ParticleStart };
    hsize_t     stride_dsk[] = { 1 };
    //
    for (int c=0; c<Nc; c++)
      {
      const char *name = arraylist[c].c_str();
      hid_t dataset   = H5Dopen(H5FileId->timegroup, name h_params);
      hid_t diskshape = H5PartGetDiskShape(H5FileId,dataset);
      /* parallel read needs hyperslab for disk */
      r = H5Sselect_hyperslab(diskshape, H5S_SELECT_SET,
        offset_dsk, stride_dsk, count1_dsk, NULL);
      if (Nc==1 || this->UseStridedMultiComponentRead) 
        {
        hid_t memspace = H5Screate_simple(1, count1_mem, NULL);
        hid_t component_datatype = H5PartGetNativeDatasetType(H5FileId, name);
        /* read x/y/z arrays into strided mem - use hyperslab */
        offset_mem[0] = c;
        r = H5Sselect_hyperslab(
          memspace, H5S_SELECT_SET,
          offset_mem, stride_mem, count2_mem, NULL);
        if (H5Tequal(component_datatype,datatype))
          {
          H5Dread(dataset, datatype, memspace,
            diskshape, H5P_DEFAULT, dataarray->GetVoidPointer(0));
          }
        else
          {
          // read data into a temporary array of the right type and then copy it
          // over to the "dataarray".
          // This can be optimized to create a single component array. But I
          // don't understand the stride/offset stuff too well to fix that.
          vtkDataArray* temparray =
            vtkDataArray::CreateDataArray(GetVTKDataType(component_datatype));
          temparray->SetNumberOfComponents(Nc);
          temparray->SetNumberOfTuples(Nt);
          r = H5Sselect_hyperslab(
            memspace, H5S_SELECT_SET,
            offset_mem, stride_mem, count2_mem, NULL);
          H5Dread(dataset, component_datatype, memspace,
            diskshape, H5P_DEFAULT, temparray->GetVoidPointer(0));
          dataarray->CopyComponent(c, temparray, c);
          temparray->FastDelete();
          }
        H5Sclose(memspace);
        H5Tclose(component_datatype);
        }      
      else 
        {
        vtkSmartPointer<vtkDataArray> onearray = NULL;
        onearray.TakeReference(vtkDataArray::CreateDataArray(vtk_datatype));
        onearray->SetNumberOfComponents(1);
        onearray->SetNumberOfTuples(Nt);
        onearray->SetName(name);
        offset_mem[0] = 0;
        count1_mem[0] = Nt;
        stride_mem[0] = 1;
        hid_t memspace = H5Screate_simple(1, count1_mem, NULL);
        hid_t component_datatype = H5PartGetNativeDatasetType(H5FileId, name);
        r = H5Sselect_hyperslab(
          memspace, H5S_SELECT_SET,
          offset_mem, stride_mem, count2_mem, NULL);
        if (H5Tequal(component_datatype,datatype))
          {
          H5Dread(dataset, datatype, memspace,
            diskshape, H5P_DEFAULT, onearray->GetVoidPointer(0));
          }
        else 
          {
          vtkErrorMacro("H5Part : Unhandled type change condition")
          }
        switch (dataarray->GetDataType()) 
          {
          case VTK_FLOAT:
            this->CopyIntoVector<float>(c,onearray,dataarray);
            break;
          case VTK_DOUBLE:
            this->CopyIntoVector<double>(c,onearray,dataarray);
            break;
          case VTK_CHAR:
          case VTK_SIGNED_CHAR:
          case VTK_UNSIGNED_CHAR:
            this->CopyIntoVector<char>(c,onearray,dataarray);
            break;
          case VTK_SHORT:
            CopyIntoVector<short int>(c,onearray,dataarray);
            break;
          case VTK_UNSIGNED_SHORT:
            CopyIntoVector<unsigned short int>(c,onearray,dataarray);
            break;
          case VTK_INT:
            CopyIntoVector<int>(c,onearray,dataarray);
            break;
          case VTK_UNSIGNED_INT:
            CopyIntoVector<unsigned int>(c,onearray,dataarray);
            break;
          case VTK_LONG:
            CopyIntoVector<long int>(c,onearray,dataarray);
            break;
          case VTK_UNSIGNED_LONG:
            CopyIntoVector<unsigned long int>(c,onearray,dataarray);
            break;
          case VTK_LONG_LONG:
            CopyIntoVector<long long>(c,onearray,dataarray);
            break;
          case VTK_UNSIGNED_LONG_LONG:
            CopyIntoVector<unsigned long long>(c,onearray,dataarray);
            break;
          case VTK_ID_TYPE:
            CopyIntoVector<vtkIdType>(c,onearray,dataarray);
            break;
          default:
          vtkErrorMacro("H5Part : Unhandled vector type")
          }
        H5Sclose(memspace);
        H5Tclose(component_datatype);
        // if the array we read for the vector component is a field array
        // then skip reading it twice.
        if (this->MultiComponentArraysAsFieldData) {
          output->GetPointData()->AddArray(onearray);
          }
        }
      H5Sclose(diskshape);
      H5Dclose(dataset);
      }
    H5Tclose(datatype);
    //
    if (dataarray)
      {
      if ((*it).first=="Coords") {
        coords = dataarray;
        coords->SetName("Coordinates");
        }
      else
        {
        output->GetPointData()->AddArray(dataarray);
        if (!output->GetPointData()->GetScalars())
          {
          output->GetPointData()->SetActiveScalars(dataarray->GetName());
          }
        }
      }
    }

  //
  // generate cells
  //
  if (this->GenerateVertexCells)
    {
    vtkSmartPointer<vtkCellArray> vertices = vtkSmartPointer<vtkCellArray>::New();
    vtkIdType *cells = vertices->WritePointer(Nt, 2*Nt);
    for (vtkIdType i=0; i<Nt; ++i)
      {
      cells[2*i] = 1;
      cells[2*i+1] = i;
      }
    output->SetVerts(vertices);
    }
  //
  //
  //
  if (!this->IgnorePartitionBoxes && (this->DisplayPartitionBoxes || this->DisplayPieceBoxes)) {
    this->DisplayBoundingBoxes(coords, output, ParticleStart, ParticleEnd);
  }
  //
  //
  //
  points->SetData(coords);
  output->SetPoints(points);
  //
  //
  // only subclasses actually close the file.
  //
  this->CloseFileIntermediate();

  return 1;
}
//----------------------------------------------------------------------------
vtkIdType vtkBonsaiSharedMemoryReader::ReadBoundingBoxes()
{
  H5E_auto2_t  errfunc;
  void        *errdata;
  vtkIdType    partitions = 0;

  // Prevent HDF5 printing errors if the bboxes dataset doesn't exist, save error handler
  H5Eget_auto(H5E_DEFAULT, &errfunc, &errdata);
  // Replace with NULL error handler
  H5Eset_auto(H5E_DEFAULT, NULL, NULL);

  // @TODO, use group/name string passed into reader ...
#if (H5_VERS_MAJOR>1)||((H5_VERS_MAJOR==1)&&(H5_VERS_MINOR>=8))
  hid_t partitiongroup = H5Gopen(H5FileId->file, "Partition#0", H5P_DEFAULT );
	hid_t dataset_id = (partitiongroup>0) ? H5Dopen ( partitiongroup, "Box", H5P_DEFAULT ) : -1;
#else
  hid_t partitiongroup = H5Gopen(H5FileId->file, "Partition#0");
	hid_t dataset_id = (partitiongroup>0) ? H5Dopen ( partitiongroup, "Box") : -1;
#endif
  // Replace normal error handler
  H5Eset_auto(H5E_DEFAULT, errfunc, errdata); 

  // if all was ok, go ahead and read the data
  if (partitiongroup>0 && dataset_id>0) { 
    hid_t diskshape = H5PartGetDiskShape(H5FileId, dataset_id);
    hsize_t dims[2], maxdims[2];
    herr_t err = H5Sget_simple_extent_dims(diskshape, dims, maxdims);
    if (err!=1) vtkErrorMacro("Error in H5Part bounding box dimensions read");
    partitions = dims[0]/13;
    //
    std::vector<double> data(dims[0], 0.0);
    err = H5Dread(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &data[0]);
    if (err<0) vtkErrorMacro("Error in H5Part bounding box data read");
    //
    // generate boxes, 2 per partition (6*2+1 = 13 vals per partition)
    // count, min{x,y,z}, max{x,y,z}, ghostmin{x,y,z}, ghostmax{x,y,z}
    //
    this->PartitionCount.resize(partitions);
    this->PartitionOffset.resize(partitions+1,0);
    this->PieceId.resize(partitions,0); // values set in repartitioning methods
    this->PartitionBoundsTable.resize(partitions*6);
    this->PartitionBoundsTableHalo.resize(partitions*6);
    //
    for (vtkIdType i=0; i<partitions; i++) {
      vtkIdType offset1 = i*6;
      vtkIdType offset2 = i*13;
      this->PartitionCount[i]      = static_cast<vtkIdType>(data[0+offset2]);
      this->PartitionBoundsTable[0+offset1] = data[1+offset2];
      this->PartitionBoundsTable[1+offset1] = data[4+offset2];
      this->PartitionBoundsTable[2+offset1] = data[2+offset2];
      this->PartitionBoundsTable[3+offset1] = data[5+offset2];
      this->PartitionBoundsTable[4+offset1] = data[3+offset2];
      this->PartitionBoundsTable[5+offset1] = data[6+offset2];
      //
      this->PartitionBoundsTableHalo[0+offset1] = data[1+6+offset2];
      this->PartitionBoundsTableHalo[1+offset1] = data[4+6+offset2];
      this->PartitionBoundsTableHalo[2+offset1] = data[2+6+offset2];
      this->PartitionBoundsTableHalo[3+offset1] = data[5+6+offset2];
      this->PartitionBoundsTableHalo[4+offset1] = data[3+6+offset2];
      this->PartitionBoundsTableHalo[5+offset1] = data[6+6+offset2];
    }
    std::partial_sum(this->PartitionCount.begin(), this->PartitionCount.end(), this->PartitionOffset.begin()+1);
    //
    // cleanup hdf5
    //
    H5Sclose(diskshape);
  }
  if (dataset_id>0) H5Dclose(dataset_id);
  if (partitiongroup>0) H5Gclose(partitiongroup);
  //
  return partitions;
}
//----------------------------------------------------------------------------
vtkIdType vtkBonsaiSharedMemoryReader::DisplayBoundingBoxes(vtkDataArray *coords, vtkPolyData *output, vtkIdType extent0, vtkIdType extent1)
{ 
  vtkIdType partitions = this->DisplayPartitionBoxes ? this->PartitionCount.size() : 0;
  vtkIdType pieces = this->DisplayPieceBoxes ? this->PieceBounds.size() : 0;
  vtkIdType newBoxes = partitions + pieces;
  //
  std::cout << "Creating boxes " << newBoxes << std::endl;
  if (newBoxes==0) return 0;
  //
  vtkIdType N1 = coords->GetNumberOfTuples();
  vtkIdType N2 = newBoxes*8*2;
  vtkIdType N3 = (N1+N2);
  //
  // We will add 3 new data arrays per point, PartitionId, PieceId and occupation(count)
  //
  vtkSmartPointer<vtkIdTypeArray> occupation = vtkSmartPointer<vtkIdTypeArray>::New();
  occupation->SetNumberOfTuples(N3);
  occupation->SetName("Occupation");
  vtkSmartPointer<vtkIdTypeArray> boxId = vtkSmartPointer<vtkIdTypeArray>::New();
  boxId->SetNumberOfTuples(N3);
  boxId->SetName("Partition");
  vtkSmartPointer<vtkIdTypeArray> piece = vtkSmartPointer<vtkIdTypeArray>::New();
  piece->SetNumberOfTuples(N3);
  piece->SetName("PieceId");
  //
  vtkIdType index = 0;
  vtkBoundingBox box;
  vtkSmartPointer<vtkAppendPolyData> polys = vtkSmartPointer<vtkAppendPolyData>::New();
  //
  // set scalars for each particle
  //
  for (vtkIdType i=0; i<this->PartitionCount.size(); i++) {
    vtkIdType numParticles = this->PartitionCount[i];
    vtkIdType pieceId = this->PieceId[i];
    if ((index+numParticles)>=extent0 && (index<=extent1)) {
      for (vtkIdType p=0; p<numParticles; p++) {
        if (index>=extent0 && index<=extent1) {
          boxId->SetValue(index-extent0, i);
          occupation->SetValue(index-extent0, numParticles);
          piece->SetValue(index-extent0, pieceId);
        }
        index++;
      }
    }
    else {
      index +=numParticles;
      continue;
    }
  }
  //
  // We haven't read all particles, so set index to last valid ID
  //
  index = (extent1-extent0)+1;
  //
  // generate boxes, 2 per partition (6*2+1 = 13 vals per partition)
  // count, min{x,y,z}, max{x,y,z}, ghostmin{x,y,z}, ghostmax{x,y,z}
  //
  for (vtkIdType i=0; i<partitions; i++) {
    vtkSmartPointer<vtkOutlineSource> cube1 = vtkSmartPointer<vtkOutlineSource>::New();
    cube1->SetBounds(&this->PartitionBoundsTable[i*6]);
    cube1->Update();
    polys->AddInputData(cube1->GetOutput());
    //
    vtkSmartPointer<vtkOutlineSource> cube2 = vtkSmartPointer<vtkOutlineSource>::New();
    cube2->SetBounds(&this->PartitionBoundsTableHalo[i*6]);
    cube2->Update();
    polys->AddInputData(cube2->GetOutput());
  }
  //
  // generate boxes, 2 per piece 
  //
  for (vtkIdType i=0; i<pieces; i++) {
    vtkSmartPointer<vtkOutlineSource> cube1 = vtkSmartPointer<vtkOutlineSource>::New();
    double bbb[6];
    this->PieceBounds[i].GetBounds(bbb);
    cube1->SetBounds(bbb);
    cube1->Update();
    polys->AddInputData(cube1->GetOutput());
    //
    this->PieceBoundsHalo[i].GetBounds(bbb);
    vtkSmartPointer<vtkOutlineSource> cube2 = vtkSmartPointer<vtkOutlineSource>::New();
    cube2->SetBounds(bbb);
    cube2->Update();
    polys->AddInputData(cube2->GetOutput());
  }
  polys->Update();
  //
  // Add the generated box points to our original list
  //
  vtkPoints *points = polys->GetOutput()->GetPoints();
  coords->Resize(N3); // might allocate more memory than we need.
  coords->SetNumberOfTuples(N3); // set limits to correct count
  for (vtkIdType P=0; P<N2; P++) {
    coords->SetTuple(N1+P, points->GetPoint(P));
  }
  //
  // Copy lines to output, but add N1 to the point ID of each line vertex
  //
  output->SetLines(polys->GetOutput()->GetLines()); 
  vtkIdType L = output->GetLines()->GetNumberOfCells();
  vtkIdTypeArray *linedata = output->GetLines()->GetData();
  // lines stored as : {2, p1, p2}, {2, p1, p2}, ... 
  for (vtkIdType B=0; B<L; B++) {
    linedata->SetValue(B*3+1, N1 + linedata->GetValue(B*3+1));
    linedata->SetValue(B*3+2, N1 + linedata->GetValue(B*3+2));
  }
  //
  // set scalars for line/box
  //
  for (vtkIdType i=0; i<partitions; i++) {
    vtkIdType numParticles = this->PartitionCount[i];
    vtkIdType pieceId = this->PieceId[i];
    for (vtkIdType p=0; p<8*2; p++) {
      boxId->SetValue(index, i);
      occupation->SetValue(index, numParticles);
      piece->SetValue(index, pieceId);
      index++;
    }
  }
  for (vtkIdType i=0; i<pieces; i++) {
    vtkIdType numParticles = 0;
    vtkIdType pieceId = i;
    for (vtkIdType p=0; p<8*2; p++) {
      boxId->SetValue(index, 0);
      occupation->SetValue(index, numParticles);
      piece->SetValue(index, pieceId);
      index++;
    }
  }
  //
  // The existing scalar array must be padded out for the new points we added
  //
  output->GetPointData()->CopyAllocate(output->GetPointData(),N3);
  for (vtkIdType i=0; i<N2; i++) {
    output->GetPointData()->NullPoint(N1+i);
  }
  //
  // And now add the new arrays (correct size already)
  //
  output->GetPointData()->AddArray(boxId);
  output->GetPointData()->AddArray(occupation);
  output->GetPointData()->AddArray(piece);

  return N2;
}

//----------------------------------------------------------------------------
unsigned int mylog2(unsigned int val) {
  unsigned int ret = -1;
  while (val != 0) {
    val >>= 1;
    ret++;
  }
  return ret;
}

//----------------------------------------------------------------------------
const char* vtkBonsaiSharedMemoryReader::GetPointArrayName(int index)
{
  return this->PointDataArraySelection->GetArrayName(index);
}
//----------------------------------------------------------------------------
int vtkBonsaiSharedMemoryReader::GetPointArrayStatus(const char* name)
{
  return this->PointDataArraySelection->ArrayIsEnabled(name);
}
//----------------------------------------------------------------------------
void vtkBonsaiSharedMemoryReader::SetPointArrayStatus(const char* name, int status)
{
  if (status!=this->GetPointArrayStatus(name))
    {
    if (status)
      {
      this->PointDataArraySelection->EnableArray(name);
      }
    else
      {
      this->PointDataArraySelection->DisableArray(name);
      }
    this->Modified();
    }
}
//----------------------------------------------------------------------------
void vtkBonsaiSharedMemoryReader::Enable(const char* name)
{
  this->SetPointArrayStatus(name, 1);
}
//----------------------------------------------------------------------------
void vtkBonsaiSharedMemoryReader::Disable(const char* name)
{
  this->SetPointArrayStatus(name, 0);
}
//----------------------------------------------------------------------------
void vtkBonsaiSharedMemoryReader::EnableAll()
{
  this->PointDataArraySelection->EnableAllArrays();
}
//----------------------------------------------------------------------------
void vtkBonsaiSharedMemoryReader::DisableAll()
{
  this->PointDataArraySelection->DisableAllArrays();
}
//----------------------------------------------------------------------------
int vtkBonsaiSharedMemoryReader::GetNumberOfPointArrays()
{
  return this->PointDataArraySelection->GetNumberOfArrays();
}
//----------------------------------------------------------------------------
void vtkBonsaiSharedMemoryReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "FileName: " <<
    (this->FileName ? this->FileName : "(none)") << "\n";

  os << indent << "NumberOfSteps: " <<  this->NumberOfTimeSteps << "\n";
}
