//
// cd D:\cmakebuild\pv-shared\bin\RelWithDebInfo
// mpiexec -localonly -n 2 -channel mt D:\cmakebuild\plugins\plugins\bin\RelWithDebInfo\TestDSM.exe dsm --dump
//

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mpi.h>

#ifndef WIN32
#  define HAVE_PTHREADS
#  include <pthread.h>
#elif HAVE_BOOST_THREADS
#  include <boost/thread/thread.hpp> // Boost Threads
#endif

#include "Xdmf.h"
#include "XdmfDsmDump.h"
#include "hdf5.h"
#include "H5FDdsm.h"

using std::cerr;
using std::cout;
using std::endl;

//#define JDEBUG
#ifdef  JDEBUG
#  define PRINT_DEBUG_INFO(x) cout << x << endl;
#else
#  define PRINT_DEBUG_INFO(x)
#endif
#define PRINT_INFO(x) cout << x << endl;
#define PRINT_ERROR(x) cerr << x << endl;

#define FILE "TestDSM.h5"

//----------------------------------------------------------------------------
class DSMServiceThread
{
public:
  DSMServiceThread(XdmfDsmBuffer *dsmObject)
  {
    this->DSMObject = dsmObject;
  }
  void operator()() {
    //      this->DSMObject->SetGlobalDebug(1);
    this->DSMObject->ServiceThread();
  }
  //
  XdmfDsmBuffer *DSMObject;
};
//----------------------------------------------------------------------------

int
main(int argc, char *argv[])
{
  hid_t          file_id, group_id, dataset_id1, dataspace_id1, dataset_id2, dataspace_id2, fapl;  /* identifiers */
  hsize_t        dims[2];
  herr_t         status;
  int           dset1_data[3][3], dset2_data[2][10];
  int           dset1_data_test[3][3], dset2_data_test[2][10];
#ifdef HAVE_PTHREADS
  pthread_t     ServiceThread  = NULL;
#elif HAVE_BOOST_THREADS
  boost::thread *ServiceThread = NULL;
#endif
  int            rank, size, provided;
  int            dsm = 0, local = 0, dump = 0, ldump = 0, test = 1;

  XdmfDsmBuffer  *MyDsm;
  XdmfDsmCommMpi *MyComm;

  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
  if (provided != MPI_THREAD_MULTIPLE) {
    PRINT_INFO("MPI_THREAD_MULTIPLE not set, you may need to recompile your "
        << "MPI distribution with threads enabled");
  }

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

#ifdef WIN32
  // wait for input char to allow debugger to be connected
  if (rank==0) {
    char ch;
    std::cin >> ch;
  }
  MPI_Barrier(MPI_COMM_WORLD);
#endif

  if (argc < 2) {
    if (rank == 0) {
      PRINT_INFO("Usage: " << argv[0] << " disk|dsm " << "[OPTIONS]" << endl
          << "  OPTIONS" << endl
          << "      --local  Create and test the DSM locally, otherwise try to connect" << endl
          << "      --dump   Dump the generated DSM file" << endl
          << "      --ldump  Dump headers of the generated DSM file" << endl
          << "      --notest Disable tests (only done when no dump requested)" << endl);
    }
    MPI_Finalize();
    return EXIT_FAILURE;
  }

  if (strcmp(argv[1], "dsm") == 0) {
    dsm = 1;
  }

  if (argc == 3 || argc == 4) {
    if ((strcmp(argv[2], "--dump") == 0) || (strcmp(argv[3], "--dump") == 0)) {
      if (dsm == 1)
        dump = 1;
      else
        PRINT_INFO("--dump option only available with DSM enabled");
    }
    if ((strcmp(argv[2], "--ldump") == 0) || (strcmp(argv[3], "--ldump") == 0)) {
      if (dsm == 1)
        ldump = 1;
      else
        PRINT_INFO("--ldump option only available with DSM enabled");
    }
    if ((strcmp(argv[2], "--notest") == 0) || (strcmp(argv[3], "--notest") == 0)) {
      test = 0;
    }
    if ((strcmp(argv[2], "--local") == 0) || (strcmp(argv[3], "--local") == 0)) {
      if (dsm == 1)
        local = 1;
      else
        PRINT_INFO("--local option only available with DSM enabled");
    }
  }

  if (dsm == 1 && local == 1) {
    MyDsm = new XdmfDsmBuffer();
    MyComm = new XdmfDsmCommMpi();

    MyComm->Init();
    // New Communicator for Xdmf Transactions
    MyComm->DupComm(MPI_COMM_WORLD);
    MyDsm->ConfigureUniform(MyComm, 3*1024);

    PRINT_DEBUG_INFO("Creating threads");

    // Start another thread to handle DSM requests from other nodes
#ifdef HAVE_PTHREADS
    pthread_create(&ServiceThread, NULL, &XdmfDsmBufferServiceThread, (void *) MyDsm);
#elif HAVE_BOOST_THREADS
    DSMServiceThread MyDSMServiceThread(MyDsm);
    ServiceThread = new boost::thread(MyDSMServiceThread);
#endif

    // Wait for DSM to be ready
    while (!MyDsm->GetThreadDsmReady())
      {         // Spin
      }
  }

  MPI_Barrier(MPI_COMM_WORLD);

  // Initialize the first dataset
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      dset1_data[i][j] = j + 1;

  // Initialize the second dataset
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < 10; j++)
      dset2_data[i][j] = j + 1;

  // Create the file access property list
  fapl = H5Pcreate(H5P_FILE_ACCESS);
  if (dsm == 1) {
    //if (rank == 0) {
      // Initialize the DSM VFL driver
      H5FD_dsm_init();
    //}
    if (local == 1)
      H5Pset_fapl_dsm(fapl, H5FD_DSM_INCREMENT, MyDsm);
    else
      H5Pset_fapl_dsm(fapl, H5FD_DSM_INCREMENT, NULL);
  } else {
    if (size > 1) {
      // Check for Parallel HDF5 ... MPI must already be initialized
#if H5_HAVE_PARALLEL && ((H5_VERS_MAJOR>1)||((H5_VERS_MAJOR==1)&&(H5_VERS_MINOR>=6)))
      PRINT_INFO("Using Parallel File Interface");
      H5Pset_fapl_mpio(fapl, MPI_COMM_WORLD, MPI_INFO_NULL);
#else
      PRINT_INFO("Using Serial File Interface");
#endif
    }
  }

  // Create a new file
  if (dsm == 1) {
    PRINT_DEBUG_INFO("Writing data to DSM...");
    file_id = H5Fcreate(FILE, H5F_ACC_TRUNC, H5P_DEFAULT, fapl);
    PRINT_DEBUG_INFO(endl << "Create file");
    if (file_id < 0) {
      PRINT_ERROR("Cannot create file");
      return EXIT_FAILURE;
    }
  }
  else {
    PRINT_DEBUG_INFO("Writing data to disk...");
    file_id = H5Fcreate(FILE, H5F_ACC_TRUNC, H5P_DEFAULT, fapl);
    PRINT_DEBUG_INFO(endl << "Create file");
    if (file_id < 0) {
      PRINT_ERROR("Cannot create file");
      return EXIT_FAILURE;
    }
  }

  // Create the data space for the first dataset
  PRINT_DEBUG_INFO(endl << "Create the first dataspace");
  dims[0] = 3;
  dims[1] = 3;
  dataspace_id1 = H5Screate_simple(2, dims, NULL);

  // Create a dataset in group "/"
  PRINT_DEBUG_INFO("Create the first dataset");
  dataset_id1 = H5Dcreate(file_id, "/dset1", H5T_STD_I32BE, dataspace_id1, H5P_DEFAULT);

  // Create the group MyGroup
  PRINT_DEBUG_INFO(endl << "Create the group");
  group_id = H5Gcreate(file_id, "/MyGroup", H5P_DEFAULT);

  // Create the data space for the second dataset
  PRINT_DEBUG_INFO("Create the second dataspace");
  dims[0] = 2;
  dims[1] = 10;
  dataspace_id2 = H5Screate_simple(2, dims, NULL);

  // Create the second dataset in group "/MyGroup"
  PRINT_DEBUG_INFO("Create the second dataset");
  dataset_id2 = H5Dcreate(group_id, "dset2", H5T_STD_I32BE, dataspace_id2, H5P_DEFAULT);

  // Write the first dataset
  if ((rank % 2) == 0) {
    PRINT_DEBUG_INFO(endl << "Write the first dataset");
    H5Dwrite(dataset_id1, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, dset1_data);
  }

  // Write the second dataset
  if ((rank % 2) == 1 || size == 1) {
    PRINT_DEBUG_INFO(endl << "Write the second dataset");
    H5Dwrite(dataset_id2, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, dset2_data);
  }

  // Close the data space for the first dataset
  PRINT_DEBUG_INFO(endl << "Close datasets");
  H5Sclose(dataspace_id1);

  // Close the first dataset
  H5Dclose(dataset_id1);

  // Close the data space for the second dataset
  H5Sclose(dataspace_id2);

  // Close the second dataset
  H5Dclose(dataset_id2);

  // Close the group
  PRINT_DEBUG_INFO("Close group");
  H5Gclose(group_id);

  // Close the file
  PRINT_DEBUG_INFO("Close file");
  H5Fclose(file_id);

  MPI_Barrier(MPI_COMM_WORLD);

  /////////////////////////////////////////////////////////////////
  // Test to validate written data
  /////////////////////////////////////////////////////////////////

  if (local) {
    if (!dump && !ldump && test) {
      if (dsm == 1) {
        PRINT_DEBUG_INFO(endl << "Checking data from DSM...");
      }
      else
        PRINT_DEBUG_INFO("Checking data from disk...");

      // Open the "file"
      file_id = H5Fopen(FILE, H5F_ACC_RDONLY, fapl);
      if (file_id < 0) {
        PRINT_ERROR("Cannot open file");
        return EXIT_FAILURE;
      }

      // Open the first dataset
      dataset_id1 = H5Dopen(file_id, "/dset1");

      // Read the dataset
      H5Dread(dataset_id1, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, dset1_data_test);

      // Close the dataset
      H5Dclose(dataset_id1);

      // Compare values of the first dataset
      for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
          if (dset1_data[i][j] != dset1_data_test[i][j]) {
            PRINT_ERROR("Test failed for first dataset[" << i << "][" << j << "] got "
                << dset1_data_test[i][j] << " expected " << dset1_data[i][j]);
            // Close the file
            H5Fclose(file_id);
            // Close the file access property list
            H5Pclose(fapl);

            if (dsm == 1) {
              if (rank == 0) {
                MyDsm->SendDone();
              }
              MPI_Barrier(MPI_COMM_WORLD);
#ifdef HAVE_PTHREADS
              if (ServiceThread) {
                pthread_join(ServiceThread, NULL);
                ServiceThread = NULL;
              }
#elif HAVE_BOOST_THREADS
              if (ServiceThread) {
                delete ServiceThread;
                ServiceThread = NULL;
              }
#endif
              delete MyComm;
              delete MyDsm;
            }

            MPI_Finalize();
            return EXIT_FAILURE;
          }

      // Open the group MyGroup
      group_id = H5Gopen(file_id, "/MyGroup");

      // Open the second dataset
      dataset_id2 = H5Dopen(group_id, "dset2");

      // Read the dataset
      H5Dread(dataset_id2, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, dset2_data_test);

      // Close the dataset
      H5Dclose(dataset_id2);

      // Close the group
      H5Gclose(group_id);

      // Compare values of the second dataset
      for (int i = 0; i < 2; i++)
        for (int j = 0; j < 10; j++)
          if (dset2_data[i][j] != dset2_data_test[i][j]) {
            PRINT_ERROR("Test failed for second dataset[" << i << "][" << j << "] got " <<
                dset2_data_test[i][j] << " expected " << dset2_data[i][j]);
            // Close the file
            H5Fclose(file_id);
            // Close the file access property list
            H5Pclose(fapl);

            if (dsm == 1) {
              if (rank == 0) {
                MyDsm->SendDone();
              }
              MPI_Barrier(MPI_COMM_WORLD);
#ifdef HAVE_PTHREADS
              if (ServiceThread) {
                pthread_join(ServiceThread, NULL);
                ServiceThread = NULL;
              }
#elif HAVE_BOOST_THREADS
              if (ServiceThread) {
                delete ServiceThread;
                ServiceThread = NULL;
              }
#endif
              delete MyComm;
              delete MyDsm;
            }
            MPI_Finalize();
            return EXIT_FAILURE;
          }

      PRINT_INFO("Test passed!");

      // Close the file
      PRINT_DEBUG_INFO("Closing File");
      H5Fclose(file_id);
    }

    if (dump && dsm && (rank == 0)) {
      XdmfDsmDump *myDsmDump = new XdmfDsmDump();
      myDsmDump->SetDsmBuffer(MyDsm);
      myDsmDump->Dump();
      delete myDsmDump;
    }

    if (ldump && dsm && (rank == 0)) {
      XdmfDsmDump *myDsmDump = new XdmfDsmDump();
      myDsmDump->SetDsmBuffer(MyDsm);
      myDsmDump->DumpLight();
      delete myDsmDump;
    }
  }
    MPI_Barrier(MPI_COMM_WORLD);
    // Close the file access property list
    H5Pclose(fapl);

    if (dsm && local) {
      if (rank == 0) {
        MyDsm->SendDone();
      }
      MPI_Barrier(MPI_COMM_WORLD);
#ifdef HAVE_PTHREADS
      if (ServiceThread) {
        pthread_join(ServiceThread, NULL);
        ServiceThread = NULL;
      }
#elif HAVE_BOOST_THREADS
      if (ServiceThread) {
        delete ServiceThread;
        ServiceThread = NULL;
      }
#endif
      delete MyComm;
      delete MyDsm;
    }

#ifdef WIN32
  // wait for input char to allow debugger to be connected
  if (rank==0) {
    char ch;
    std::cin >> ch;
  }
  MPI_Barrier(MPI_COMM_WORLD);
#endif

  PRINT_DEBUG_INFO("About to MPI_Finalize");
  MPI_Finalize();
  return EXIT_SUCCESS;
}
//----------------------------------------------------------------------------
