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

#include "hdf5.h"
#include "H5FDdsm.h"
#include "H5FDdsmBuffer.h"
#include "H5FDdsmCommMpi.h"
#include "XdmfDump.h"

using std::cerr;
using std::cout;
using std::endl;

#define JDEBUG
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
  DSMServiceThread(H5FDdsmBuffer *dsmObject)
  {
    this->DSMObject = dsmObject;
  }
  void operator()() {
    //      this->DSMObject->SetGlobalDebug(1);
    this->DSMObject->ServiceThread();
  }
  //
  H5FDdsmBuffer *DSMObject;
};
//----------------------------------------------------------------------------

int
main(int argc, char *argv[])
{
  hid_t           file_id, group_id, dataset_id1, dataspace_id1, dataset_id2, dataspace_id2, fapl;  /* identifiers */
  hsize_t         dims[2];
  int             dset1_data[3][3], dset2_data[2][10];
  int             dset1_data_test[3][3], dset2_data_test[2][10];
#ifdef HAVE_PTHREADS
  pthread_t       ServiceThread  = 0;
#elif HAVE_BOOST_THREADS
  boost::thread  *ServiceThread = NULL;
#endif
  int             rank, size, provided;
  int             dsm = 0, local = 0, dump = 0, ldump = 0, test = 1;
  int             nwrite = 1;

  H5FDdsmBuffer  *MyDsm = NULL;
  H5FDdsmCommMpi *MyComm = NULL;

  std::string     dsm_port_name;

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
      PRINT_INFO("Usage: " << argv[0] << " disk|dsm " << "[--local/port <port_name>] [--dump/ldump/notest]" << endl
          << "  OPTIONS" << endl
          << "      --local  Create and test the DSM locally, otherwise try to connect" << endl
          << "      --port   Specify a port name for the DSM" << endl
          << "      --dump   Dump the generated DSM file" << endl
          << "      --ldump  Dump headers of the generated DSM file" << endl
          << "      --notest Disable tests (only done when no dump requested)" << endl
          << "      --write <n> Do n write while clearing DSM between each step" << endl);
    }
    MPI_Finalize();
    return EXIT_FAILURE;
  }

  if (strcmp(argv[1], "dsm") == 0) {
    dsm = 1;
  }

  if (dsm && (argc > 3)) {
    if ((strlen(argv[3]) > 0) && (strcmp(argv[2], "--port") == 0)) {
      dsm_port_name = argv[3];
    }
  }

  if (dsm && (argc > 2)) {
    if (strcmp(argv[2], "--local") == 0) {
      local = 1;
    }
  }

  if (local && (argc > 3)) {
    if (strcmp(argv[3], "--dump") == 0) {
      dump = 1;
    }
    if (strcmp(argv[3], "--ldump") == 0) {
      ldump = 1;
    }
    if (strcmp(argv[3], "--notest") == 0) {
      test = 0;
    }
  }

  if (local && (argc > 5)) {
    if(strcmp(argv[4], "--write") == 0) {
      nwrite = atoi(argv[5]);
    }
  }

  if (dsm == 1) {
    MyDsm = new H5FDdsmBuffer();
    MyComm = new H5FDdsmCommMpi();

    MyComm->Init();
    // New Communicator for Xdmf Transactions
    MyComm->DupComm(MPI_COMM_WORLD);
    MyDsm->ConfigureUniform(MyComm, 3*1024);
    if (!local) {
      MyDsm->SetIsServer(0);
      if (dsm_port_name.length() > 0) {
        dynamic_cast<H5FDdsmCommMpi*> (MyDsm->GetComm())->SetDsmMasterHostName(dsm_port_name.c_str());
      }
      //MyDsm->DebugOn();
      //MyComm->DebugOn();
    }
    else {
    PRINT_DEBUG_INFO("Creating threads");

    // Start another thread to handle DSM requests from other nodes
#ifdef HAVE_PTHREADS
    pthread_create(&ServiceThread, NULL, &H5FDdsmBufferServiceThread, (void *) MyDsm);
#elif HAVE_BOOST_THREADS
    DSMServiceThread MyDSMServiceThread(MyDsm);
    ServiceThread = new boost::thread(MyDSMServiceThread);
#endif

    // Wait for DSM to be ready
    while (!MyDsm->GetThreadDsmReady())
      {         // Spin
      }
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

  for(int write_step = 0; write_step < nwrite; write_step++) {

    PRINT_DEBUG_INFO(endl << "------ Writing step " << write_step << " ------" << endl);
    // MyDsm->ClearStorage();

    // Create the file access property list
    fapl = H5Pcreate(H5P_FILE_ACCESS);
    if (dsm == 1) {
      // Initialize the DSM VFL driver
      H5FD_dsm_init();
      //}
      H5Pset_fapl_dsm(fapl, MPI_COMM_WORLD, MyDsm);
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
#if (!H5_USE_16_API_DEFAULT && ((H5_VERS_MAJOR>1)||((H5_VERS_MAJOR==1)&&(H5_VERS_MINOR>=8))))
    dataset_id1 = H5Dcreate(file_id, "/dset1", H5T_STD_I32BE, dataspace_id1, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
#else
    dataset_id1 = H5Dcreate(file_id, "/dset1", H5T_STD_I32BE, dataspace_id1, H5P_DEFAULT);
#endif
    // Create the group MyGroup
    PRINT_DEBUG_INFO(endl << "Create the group");
#if (!H5_USE_16_API_DEFAULT && ((H5_VERS_MAJOR>1)||((H5_VERS_MAJOR==1)&&(H5_VERS_MINOR>=8))))
    group_id = H5Gcreate(file_id, "/MyGroup", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
#else
    group_id = H5Gcreate(file_id, "/MyGroup", H5P_DEFAULT);
#endif

    // Create the data space for the second dataset
    PRINT_DEBUG_INFO("Create the second dataspace");
    dims[0] = 2;
    dims[1] = 10;
    dataspace_id2 = H5Screate_simple(2, dims, NULL);

    // Create the second dataset in group "/MyGroup"
    PRINT_DEBUG_INFO("Create the second dataset");
#if (!H5_USE_16_API_DEFAULT && ((H5_VERS_MAJOR>1)||((H5_VERS_MAJOR==1)&&(H5_VERS_MINOR>=8))))
    dataset_id2 = H5Dcreate(group_id, "dset2", H5T_STD_I32BE, dataspace_id2, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
#else
    dataset_id2 = H5Dcreate(group_id, "dset2", H5T_STD_I32BE, dataspace_id2, H5P_DEFAULT);
#endif

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

    // Close the file access property list
     H5Pclose(fapl);

    // Close the file
    MPI_Barrier(MPI_COMM_WORLD);

    PRINT_DEBUG_INFO("Close file");
    H5Fclose(file_id);

    MPI_Barrier(MPI_COMM_WORLD);
  }

  /////////////////////////////////////////////////////////////////
  // Test to validate written data
  /////////////////////////////////////////////////////////////////

  // Create the file access property list
  fapl = H5Pcreate(H5P_FILE_ACCESS);
  if (dsm == 1) {
    // Initialize the DSM VFL driver
    H5FD_dsm_init();
    //}
    H5Pset_fapl_dsm(fapl, MPI_COMM_WORLD, MyDsm);
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
#if (!H5_USE_16_API_DEFAULT && ((H5_VERS_MAJOR>1)||((H5_VERS_MAJOR==1)&&(H5_VERS_MINOR>=8))))
      dataset_id1 = H5Dopen(file_id, "/dset1", H5P_DEFAULT);
#else
      dataset_id1 = H5Dopen(file_id, "/dset1");
#endif

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
                ServiceThread = 0;
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
#if (!H5_USE_16_API_DEFAULT && ((H5_VERS_MAJOR>1)||((H5_VERS_MAJOR==1)&&(H5_VERS_MINOR>=8))))
      group_id = H5Gopen(file_id, "/MyGroup", H5P_DEFAULT);
#else
      group_id = H5Gopen(file_id, "/MyGroup");
#endif

      // Open the second dataset
#if (!H5_USE_16_API_DEFAULT && ((H5_VERS_MAJOR>1)||((H5_VERS_MAJOR==1)&&(H5_VERS_MINOR>=8))))
      dataset_id2 = H5Dopen(group_id, "dset2", H5P_DEFAULT);
#else
      dataset_id2 = H5Dopen(group_id, "dset2");
#endif

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
                ServiceThread = 0;
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

    if (dump && dsm) {
      XdmfDump *myDsmDump = new XdmfDump();
      myDsmDump->SetDsmBuffer(MyDsm);
      myDsmDump->Dump();
      delete myDsmDump;
    }

    if (ldump && dsm) {
      XdmfDump *myDsmDump = new XdmfDump();
      myDsmDump->SetDsmBuffer(MyDsm);
      myDsmDump->DumpLight();
      delete myDsmDump;
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }

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
      ServiceThread = 0;
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

  if(!local) { // temporary here
    MyDsm->RequestLocalChannel(); // Go back to normal channel
    PRINT_DEBUG_INFO("Trying to disconnect");
    MyDsm->GetComm()->RemoteCommDisconnect();
  }

  PRINT_DEBUG_INFO("About to MPI_Finalize");
  MPI_Finalize();
  return EXIT_SUCCESS;
}
//----------------------------------------------------------------------------
