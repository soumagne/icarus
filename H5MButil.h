#ifndef H5MButil_H
#define H5MButil_H

#include "H5Tpublic.h"

#ifdef H5_HAVE_PARALLEL

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------
struct H5MB_tree_type {
  void *TreePtr;      
};
//--------------------------------------------------------------------------
enum H5_MB_types {
  MB_H5T_UNDEFINED = 0,
  MB_H5T_NATIVE_CHAR = 1,
  MB_H5T_NATIVE_SHORT,
  MB_H5T_NATIVE_INT,
  MB_H5T_NATIVE_LONG,
  MB_H5T_NATIVE_LLONG,
  //
  MB_H5T_NATIVE_UCHAR,
  MB_H5T_NATIVE_USHORT,
  MB_H5T_NATIVE_UINT,
  MB_H5T_NATIVE_ULONG,
  MB_H5T_NATIVE_ULLONG,
  //
  MB_H5T_NATIVE_FLOAT,
  MB_H5T_NATIVE_DOUBLE,
  MB_H5T_NATIVE_LDOUBLE,
};
//--------------------------------------------------------------------------
H5_DLL H5MB_tree_type *H5MB_init(const char *filename);
H5_DLL bool            H5MB_add(H5MB_tree_type *treestruct, const char *path, int type, int rank, int *start=NULL, int *count=NULL, int *stride=NULL);
H5_DLL bool            H5MB_collect(H5MB_tree_type *treestruct, MPI_Comm comm);
H5_DLL bool            H5MB_create(H5MB_tree_type *treestruct, MPI_Comm comm);
H5_DLL void            H5MB_print(const H5MB_tree_type *treestruct);
//--------------------------------------------------------------------------
#ifdef __cplusplus
}
#endif

#endif

#endif
