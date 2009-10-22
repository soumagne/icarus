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

#include "tree.h"
#include "H5MButil.h"

#include "Xdmf.h"
#include "XdmfDsmDump.h"
#include "hdf5.h"
#include "H5FDdsm.h"

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
    char ch;
    std::cin >> ch;
  }
  MPI_Barrier(MPI_COMM_WORLD);
#endif

  H5MB_tree_type *tree = H5MB_init();
  H5MB_add(tree, "Step#0000/Proc#0000/UGrid_0", NULL);
  H5MB_add(tree, "Step#0000/Proc#0000/UGrid_1", NULL);
  H5MB_add(tree, "Step#0000/Proc#0000/UGrid_2", NULL);
  H5MB_add(tree, "Step#0000/Proc#0001/UGrid_0", NULL);
  H5MB_add(tree, "Step#0001/Proc#0000/UGrid_0", NULL);

//  H5MB_collect(tree, MPI_COMM_WORLD);
//  H5MB_create(tree);

  return EXIT_SUCCESS;
}
//----------------------------------------------------------------------------
