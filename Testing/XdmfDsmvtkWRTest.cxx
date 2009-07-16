/*=========================================================================

 Project                 : vtkCSCS
 Module                  : XdmfDsmvtkWRTest.cxx
 Revision of last commit : $Rev: 586 $
 Author of last commit   : $Author: soumagne $
 Date of last commit     : $Date:: 2009-07-07 18:21:36 +0200 #$

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
#ifndef WIN32
  #define HAVE_PTHREADS
#endif
#include "Xdmf.h"
#include "mpi.h"
#ifdef HAVE_PTHREADS
  #include "pthread.h"
#endif
#include "string.h"

#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkDataObject.h"
#include "vtkDataSet.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkSmartPointer.h"
#include "vtkInformation.h"
#include "vtkExtentTranslator.h"
#include "vtkMPIController.h"

#include "vtkXdmfReader.h"
#include "vtkXdmfWriter2.h"

#include <vtksys/SystemTools.hxx>
// Win32 execution test
// mpiexec -localonly -n 4 XdmfDsmvtkWRTest.exe D:\data\xdmf\other\cav_DSM_Test.xmf D:\test-dsm d:\test-disk
//

//----------------------------------------------------------------------------
void
WaitForAll(vtkMultiProcessController *controller, int rank, int value)
{
  cout << "P(" << rank << ") in barrier " << value << endl;
  controller->Barrier();
  cout << "P(" << rank << ") out of barrier " << value << endl;
}
//----------------------------------------------------------------------------
void
printReaderInfos(vtkXdmfReader *reader)
{
  cerr << "Number of Domains: " << reader->GetNumberOfDomains() << endl;
  cerr << "1st Domain name: " << reader->GetDomainName(0) << endl;
  cerr << "Number of Grids: " << reader->GetNumberOfGrids() << endl;
  cerr << "1st Grid Name: " << reader->GetGridName(0) << endl;
  cerr << "Number of Point arrays: " << reader->GetNumberOfPointArrays()
  << endl;
  for (int i = 0; i < (reader->GetNumberOfPointArrays()); i++)
    cerr << "Name of Point Array " << i << ": " << reader->GetPointArrayName(i)
    << endl;
  cerr << "Number of Cell arrays: " << reader->GetNumberOfCellArrays() << endl;
  cerr << "Number of Parameters: " << reader->GetNumberOfParameters() << endl;
}
//----------------------------------------------------------------------------
void
MyMain(vtkMultiProcessController *controller, void *arg)
{
  MPI_Comm XdmfComm;
  int rank, size, last_rank;
  XdmfDsmCommMpi *MyComm = new XdmfDsmCommMpi;

#ifdef HAVE_PTHREADS
  XdmfDsmBuffer *MyDsm = new XdmfDsmBuffer;
  pthread_t thread1;
#endif

  char xdmfFileName_in[200], xdmfFileName_dsm[200], xdmfFileName_out[200];
  char ** argv = (char**) arg;
  strcpy(xdmfFileName_in, argv[1]);
  strcpy(xdmfFileName_dsm, argv[2]);
  strcpy(xdmfFileName_out, argv[3]);

  MyComm->Init();
  // New Communicator for Xdmf Transactions
  MyComm->DupComm(MPI_COMM_WORLD);

  rank = MyComm->GetId();
  size = MyComm->GetTotalSize();
  last_rank = size - 1;

  cout << "Hello from " << rank << " of " << last_rank << endl;

#ifdef _WIN32
 if (rank==0) {
    char ch;
    std::cin >> ch;
  }
  controller->Barrier();
#endif

#ifdef HAVE_PTHREADS
  // Uniform Dsm : every node has a buffer of 1000000. Addresses are sequential
  MyDsm->ConfigureUniform(MyComm, 2000000);

  // Start another thread to handle DSM requests from other nodes
  pthread_create(&thread1, NULL, &XdmfDsmBufferServiceThread, (void *) MyDsm);

  // Wait for DSM to be ready
  while (!MyDsm->GetThreadDsmReady())
    {
      // Spin
    }
  cout << "Service Ready on " << rank << endl;
#endif

  WaitForAll(controller, rank, 1);

  // Read Xdmf first
  cout << "*** Reading from Disk ***" << endl;
  vtkSmartPointer<vtkXdmfReader> reader = vtkSmartPointer<vtkXdmfReader>::New();
  if (!reader->CanReadFile(xdmfFileName_in))
    {
      cerr << "Cannot read input xmf file" << endl;
      exit(0);
    }
  reader->SetController(controller);
  reader->SetFileName(xdmfFileName_in);
  reader->UpdateInformation();
  vtkSmartPointer<vtkInformation> execInfo =
    reader->GetExecutive()->GetOutputInformation(0);
  /*
   int etype = reader->GetOutputDataObject(0)->GetExtentType();
   int extent[6];
   if (etype==VTK_PIECES_EXTENT) {// ???
   reader->GetOutputDataObject(0)->GetUpdateExtent(extent);
   }
   else if (etype==VTK_3D_EXTENT) { // image volume
   }
   vtkSmartPointer<vtkExtentTranslator> extTran = vtkSmartPointer<vtkExtentTranslator>::New();
   int extent[6];
   extTran->SetSplitModeToBlock();
   extTran->SetNumberOfPieces(size);
   extTran->SetPiece(rank);
   extTran->SetWholeExtent(extent);
   extTran->PieceToExtent();
   */
  execInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES(),
      size);
  execInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER(), rank);
  //printReaderInfos(reader);
  reader->EnableAllGrids();
  reader->EnableAllArrays();
  reader->Update();

  WaitForAll(controller, rank, 2);

  vtkSmartPointer<vtkDataSet> data = vtkDataSet::SafeDownCast(
      reader->GetOutputDataObject(0));
  vtkSmartPointer<vtkMultiBlockDataSet> mbdata = vtkMultiBlockDataSet::New();
  mbdata->SetNumberOfBlocks(size);

  if (size > 1)
    {
      for (int i = 0; i < size; i++)
        {
          if (i == rank)
            mbdata->SetBlock(i, data);
          else
            mbdata->SetBlock(i, NULL);
        }
    }

  WaitForAll(controller, rank, 3);

  // Write back to Xdmf using XdmfWriter2 and put data into DSM
#ifdef HAVE_PTHREADS
  cout << "*** Writing to DSM ***" << endl;
#else
  cout << "*** Writing to disk ***" << endl;
#endif
  vtkSmartPointer<vtkXdmfWriter2> writer = vtkSmartPointer<vtkXdmfWriter2>::New();


  vtkstd::string dummy = vtkstd::string(xdmfFileName_dsm)+vtkstd::string(".xmf");
  if (vtksys::SystemTools::FileExists(dummy.c_str())) {
      vtksys::SystemTools::RemoveFile(dummy.c_str());
    }

    writer->SetFileName(xdmfFileName_dsm);
    writer->TemporalCollectionOff();
    if (size > 1) {
      writer->SetController(controller);
      writer->SetInput(mbdata);
    }
    else
      writer->SetInput(data);
#ifdef HAVE_PTHREADS
    writer->SetDsmBuffer(MyDsm);
#endif
    writer->Write();
    WaitForAll(controller, rank, 4);

#ifdef HAVE_PTHREADS
    // Create a new reader and test reading from DSM
    cout << "*** Reading from DSM ***" << endl;
    vtkSmartPointer<vtkXdmfReader> reader_test = vtkSmartPointer<vtkXdmfReader>::New();
    reader_test->SetDsmBuffer(MyDsm);
    strcat(xdmfFileName_dsm, ".xmf");
    reader_test->SetFileName(xdmfFileName_dsm);
    reader_test->UpdateInformation();
    printReaderInfos(reader_test);
    reader_test->EnableAllGrids();
    reader_test->EnableAllArrays();
    reader_test->Update();

    // Write back to disk a new set of xmf + h5 file
    if (size == 1) {
    	cout << "*** Writing to Disk ***" << endl;
    	vtkSmartPointer<vtkXdmfWriter2> writer_test = vtkSmartPointer<vtkXdmfWriter2>::New();
    	//vtkSmartPointer<vtkMultiBlockDataSet> mboutdata;
    	vtkSmartPointer<vtkDataSet> outdata;
    	writer_test->SetFileName(xdmfFileName_out);
    	outdata = vtkDataSet::SafeDownCast(reader_test->GetOutputDataObject(0));
    	writer_test->SetInput(outdata);
    	writer_test->TemporalCollectionOff();
    	writer_test->Write();
    }

    //else {
    //	mboutdata = vtkMultiBlockDataSet::SafeDownCast(reader_test->GetOutputDataObject(0));
    //	writer_test->SetController(controller);
    //	writer_test->SetInput(mboutdata);
    //}


    // Send done to DSM
    WaitForAll(controller, rank, 5);

    if (rank == 0)
    	MyDsm->SendDone();
    pthread_join(thread1, NULL);

    delete MyDsm;
#endif
    delete MyComm;
}
//----------------------------------------------------------------------------
int
main(int argc, char **argv)
{
  int provided;

  if (argc != 4)
    {
      cerr << "usage: ./XdmfDsmvtkWRTest <H5 disk input .xmf file> "
      << "<H5 dsm output .xmf file> " << "<H5 disk output .xmf file"
      << endl;
      exit(0);
    }

  MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
  if (provided != MPI_THREAD_SERIALIZED)
    {
      cerr << "MPI_THREAD_SERIALIZED not set, you may need to recompile"
      << " your MPI used version with threads enabled" << endl;
    }
  vtkMPIController* controller = vtkMPIController::New();
  controller->Initialize(&argc, &argv, 1);

  controller->SetSingleMethod(MyMain, (void*) argv);
  controller->SingleMethodExecute();

  controller->Barrier();
  controller->Finalize();
  controller->Delete();
  return 0;
}
