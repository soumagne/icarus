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
// Include this before MPI because when cxx11 is set, it tries to get <cstdint>
// which is not present when we use -stdlib=libstdc++
#include <stdint.h>
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
#include "vtkNew.h"
//
//#include <functional>
#include <algorithm>
#include <numeric>
//----------------------------------------------------------------------------
#include "BonsaiSharedData.h"
#include "BonsaiIO.h"
#include "SharedMemory.h"
//----------------------------------------------------------------------------
vtkCxxSetObjectMacro(vtkBonsaiSharedMemoryReader, Controller, vtkMultiProcessController);

//----------------------------------------------------------------------------
using ShmQHeader = SharedMemoryClient<BonsaiSharedQuickHeader>;
using ShmQData   = SharedMemoryClient<BonsaiSharedQuickData>;
static ShmQHeader *shmQHeader = NULL;
static ShmQData   *shmQData   = NULL;
static bool terminateRenderer = false;
//----------------------------------------------------------------------------

bool fetchSharedData(const bool quickSync, RendererData &rData,
    const int rank, const int nrank, const MPI_Comm &comm,
    const int reduceDM = 1, const int reduceS = 1)
{
  if (shmQHeader == NULL)
  {
    shmQHeader = new ShmQHeader(ShmQHeader::type::sharedFile(rank));
    shmQData   = new ShmQData  (ShmQData  ::type::sharedFile(rank));
  }

  auto &header = *shmQHeader;
  auto &data   = *shmQData;

  static bool first = true;
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


  static float tLast = -1.0f;


  if (rData.isNewData())
    return false;


#if 0
  //  if (rank == 0)
  fprintf(stderr, " rank= %d: attempting to fetch data \n",rank);
#endif

  // header
  header.acquireLock();
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

    /* skip particles that failed to get density, or with too big h */
    bonsaistd::function<bool(const int i)> skipPtcl = [&](const int i)
    {
      return (data[i].rho == 0 || data[i].h == 0.0 || data[i].h > 100);
    };

    size_t nDM = 0, nS = 0;
    constexpr int ntypecount = 10;
    bonsaistd::array<size_t,ntypecount> ntypeloc, ntypeglb;
    std::fill(ntypeloc.begin(), ntypeloc.end(), 0);
    for (size_t i = 0; i < size; i++)
    {
      const int type = data[i].ID.getType();
      if  (type < ntypecount)
        ntypeloc[type]++;
      if (skipPtcl(i))
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


    rData.resize(nS);
    size_t ip = 0;
    float *coords = rData.coords->GetPointer(0);
    float *mass   = rData.mass->GetPointer(0);
    float *vel    = rData.vel->GetPointer(0);
    float *rho    = rData.rho->GetPointer(0);
    float *Hval   = rData.Hval->GetPointer(0);
    vtkIdType *Id = rData.Id->GetPointer(0);

    for (size_t i = 0; i < size; i++)
    {
      if (skipPtcl(i))
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
    rData.resize(ip);

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
  this->bonsaiData = NULL;
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
    this->NumberOfTimeSteps = 1;
    bool inSitu = true;
    bool quickSync = false;

    MPI_Comm *comm = vtkMPICommunicator::SafeDownCast(this->Controller->GetCommunicator())->GetMPIComm()->GetHandle();
    if (!this->bonsaiData) {
      if (inSitu) {
        this->bonsaiData = new RendererData(this->UpdatePiece, this->UpdateNumPieces, *comm);
      }
      else {
              /*
              if ((this->bonsaiData = readBonsai<BonsaiCatalystDataT>(rank, nranks, comm, fileName, reduceDM, reduceS)))
              {}
              else
              {
                if (rank == 0)
                  fprintf(stderr, " I don't recognize the format ... please try again , or recompile to use with old tipsy if that is what you use ..\n");
                MPI_Finalize();
                ::exit(-1);
              }
              this->bonsaiData->computeMinMax();
              this->bonsaiData->setNewData();
              */
      }
    }

    assert(this->bonsaiData != 0);

    const char *names[] = { "Id", "coords", "mass", "vel", "rho", "Hval" };
    int nds = sizeof(names);
    for (auto name : names)
    {
      this->PointDataArraySelection->AddArray(name);
    }

    this->TimeStepValues.assign(this->NumberOfTimeSteps, 0.0);
    int validTimes = 0;

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

    }
                                                       
  return 1;
}

//----------------------------------------------------------------------------
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


    int rank = this->UpdatePiece;
    int nranks = this->UpdateNumPieces;
    int reduceDM = 10;
    int reduceS = 1;
    bool inSitu = true;
    bool quickSync = false;
    MPI_Comm *comm = vtkMPICommunicator::SafeDownCast(this->Controller->GetCommunicator())->GetMPIComm()->GetHandle();

    if (inSitu) {
        if (fetchSharedData(quickSync, *this->bonsaiData, rank, nranks, *comm, reduceDM, reduceS))
        {
          this->bonsaiData->setNewData();
        }
    };


  /*
  int N = this->PointDataArraySelection->GetNumberOfArrays();
  for (int i=0; i<N; i++)
    {
    const char *name = this->PointDataArraySelection->GetArrayName(i);
    // Do we want to load this array
    bool processarray = false;
    if (this->PointDataArraySelection->ArrayIsEnabled(name))
      {
      processarray = true;
      }
    if (!processarray)
      {
      continue;
      }
*/
  vtkNew<vtkPoints> points;
  points->SetData(this->bonsaiData->coords);

  output->GetPointData()->AddArray(this->bonsaiData->Id);
  output->GetPointData()->AddArray(this->bonsaiData->mass);
  output->GetPointData()->AddArray(this->bonsaiData->vel);
  output->GetPointData()->AddArray(this->bonsaiData->rho);
  output->GetPointData()->AddArray(this->bonsaiData->Hval);

  vtkIdType Nt = this->bonsaiData->coords->GetNumberOfTuples();
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
//  if (!this->IgnorePartitionBoxes && (this->DisplayPartitionBoxes || this->DisplayPieceBoxes)) {
 //   this->DisplayBoundingBoxes(coords, output, ParticleStart, ParticleEnd);
//  }
  //
  //
  //
  output->SetPoints(points.Get());
  //
  return 1;
}
//----------------------------------------------------------------------------
vtkIdType vtkBonsaiSharedMemoryReader::DisplayBoundingBoxes(vtkDataArray *coords, vtkPolyData *output, vtkIdType extent0, vtkIdType extent1)
{ 
  vtkIdType partitions = 0; // this->DisplayPartitionBoxes ? this->PartitionCount.size() : 0;
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
