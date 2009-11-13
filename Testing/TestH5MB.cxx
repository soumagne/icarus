//
// cd D:\cmakebuild\pv-shared\bin\RelWithDebInfo
// mpiexec -localonly -n 2 -channel mt D:\cmakebuild\plugins\plugins\bin\RelWithDebInfo\TestDSM.exe dsm --dump
//

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mpi.h>
#include <time.h>

#ifndef WIN32
#  define HAVE_PTHREADS
#  include <pthread.h>
#elif HAVE_BOOST_THREADS
#  include <boost/thread/thread.hpp> // Boost Threads
#endif

#include "H5MButil.h"

#include "Xdmf.h"
#include "XdmfDsmDump.h"
#include "hdf5.h"
#include "H5FDdsm.h"
#include <sstream>

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
//----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
  int rank, size, provided;
  //
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
//    char ch;
//    std::cin >> ch;
  }
  MPI_Barrier(MPI_COMM_WORLD);
#endif

  H5MB_tree_type *tree = H5MB_init("D:\\data\\xdmf\\scratch\\H5MB_Test.h5");

  for (int r=0; r<rank; r++) srand( (unsigned)time( NULL ) );
  //
  int steps = 1 + rank; // + rand() % 3;
  for (int t=0; t<steps; t++) {
    int grids = 2*(rank+1); // + rand() % 5;
    for (int i=0; i<grids; i++) {
      std::stringstream data; 
      data
        <<  "Step#" << std::setw(5) << std::setfill('0') << t 
        << "/Proc#" << std::setw(5) << std::setfill('0') << rank
        << "/Grid#" << std::setw(5) << std::setfill('0') << i;

      int start[3]  = { 0,0,0};
      int count[3]  = { 100,100,3};
      int stride[3] = { 0,0,0};
      H5MB_add(tree, data.str().c_str(), MB_H5T_NATIVE_DOUBLE, 3, start, count, stride);
    }
  }

  std::cout << "\nLocal tree on Rank " << rank << std::endl;
  H5MB_print(tree);
  std::cout << "\nCollecting " << std::endl;
  H5MB_collect(tree, MPI_COMM_WORLD);
  std::cout << "\nCombined tree " << std::endl;
  H5MB_create(tree, MPI_COMM_WORLD);
  H5MB_print(tree);

#ifdef WIN32
  // wait for input char to allow debugger to be connected
  MPI_Barrier(MPI_COMM_WORLD);
  if (rank==0) {
    char ch;
    std::cin >> ch;
  }
#endif

  MPI_Finalize();
  return EXIT_SUCCESS;
}
//----------------------------------------------------------------------------
