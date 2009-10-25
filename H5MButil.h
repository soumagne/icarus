#ifndef H5MButil_H
#define H5MButil_H

#include "H5Ipublic.h"

#include <string>
#include "XdmfGrid.h"

#ifdef H5_HAVE_PARALLEL

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
struct H5MB_info {
  H5MB_info(const char *Path, XdmfGrid *Grid)
  {
    this->grid = Grid;
    this->path = Path;
  }
  //
  std::string path;
  XdmfGrid   *grid;
};
//--------------------------------------------------------------------------
struct H5MB_tree_type {
  void *TreePtr;      
};
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
H5_DLL H5MB_tree_type *H5MB_init();
H5_DLL bool            H5MB_add(H5MB_tree_type *tree, const char *path, XdmfGrid *grid);
H5_DLL bool            H5MB_collect(H5MB_tree_type *tree, MPI_Comm *comm);

//--------------------------------------------------------------------------
#ifdef __cplusplus
}
#endif

#endif

#endif
