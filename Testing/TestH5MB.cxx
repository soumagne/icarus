//
// cd D:\cmakebuild\pv-shared\bin\RelWithDebInfo
// mpiexec -localonly -n 2 -channel mt D:\cmakebuild\plugins\plugins\bin\RelWithDebInfo\TestDSM.exe dsm --dump
//

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <time.h>
#include <mpi.h>
//
#include "H5MButil.h"
#include "H5FDdsm.h"
//
#define JDEBUG
#ifdef  JDEBUG
#  define PRINT_DEBUG_INFO(x) cout << x << std::endl;
#else
#  define PRINT_DEBUG_INFO(x)
#endif
#define PRINT_INFO(x) std::cout << x << std::endl;
#define PRINT_ERROR(x) std::cerr << x << std::endl;
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

#ifdef WIN32
  H5MB_tree_type *tree = H5MB_init("D:\\data\\xdmf\\scratch\\H5MB_Test.h5");
#else
  H5MB_tree_type *tree = H5MB_init("/H5MB_Test.h5");
#endif

  H5FD_dsm_init();

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

      hssize_t count[3]  = { 3, 100, 100 }; // reverse order please
      H5MB_add(tree, data.str().c_str(), "H5T_STD_I32LE", 3, count);
    }
  }

  std::cout << "\nLocal tree on Rank " << rank << std::endl;
  std::cout <<   "--------------------" << std::endl;
  H5MB_print(tree);
  std::cout << "\nCollecting " << std::endl;
  H5MB_collect(tree, MPI_COMM_WORLD);
  std::cout << "\nCombined tree " << std::endl;
  std::cout <<   "--------------" << std::endl;
  H5MB_create(tree, MPI_COMM_WORLD, NULL);
  std::cout << "\nCombined tree " << std::endl;
  H5MB_print(tree);

  H5close();

#ifdef WIN32
  // wait for input char to allow debugger to be connected
  MPI_Barrier(MPI_COMM_WORLD);
  if (rank==0) {
//    char ch;
//    std::cin >> ch;
  }
#endif

  MPI_Finalize();
  return EXIT_SUCCESS;
}
//----------------------------------------------------------------------------
