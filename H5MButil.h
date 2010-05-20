#ifndef H5MButil_H
#define H5MButil_H

//----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------
#if defined(CSCS_DSM_EXPORTS)
  #if defined (_MSC_VER)  /* MSVC Compiler Case */
    #define DSM_DLL __declspec(dllexport)
    #define DSM_DLLVAR extern __declspec(dllexport)
  #elif (__GNUC__ >= 4)  /* GCC 4.x has support for visibility options */
    #define DSM_DLL __attribute__ ((visibility("default")))
    #define DSM_DLLVAR extern __attribute__ ((visibility("default")))
  #endif
#else
  #if defined (_MSC_VER)  /* MSVC Compiler Case */
    #define DSM_DLL __declspec(dllimport)
    #define DSM_DLLVAR __declspec(dllimport)
  #elif (__GNUC__ >= 4)  /* GCC 4.x has support for visibility options */
    #define DSM_DLL __attribute__ ((visibility("default")))
    #define DSM_DLLVAR extern __attribute__ ((visibility("default")))
  #endif
#endif
//--------------------------------------------------------------------------
#include "H5Tpublic.h"
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
DSM_DLL H5MB_tree_type *H5MB_init(const char *filename);
DSM_DLL bool            H5MB_add(H5MB_tree_type *treestruct, const char *path, const char *type, 
  int rank, hssize_t *dims=NULL, hssize_t *start=NULL, hssize_t *stride=NULL, hssize_t *count=NULL);
DSM_DLL bool            H5MB_collect(H5MB_tree_type *treestruct, MPI_Comm comm);
DSM_DLL bool            H5MB_create(H5MB_tree_type *treestruct, MPI_Comm comm, hid_t plist_id);
DSM_DLL hid_t           H5MB_get(H5MB_tree_type *treestruct, const char *datasetpath);
DSM_DLL bool            H5MB_close(const H5MB_tree_type *treestruct, bool savetree);
DSM_DLL void            H5MB_print(const H5MB_tree_type *treestruct);
//--------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

//----------------------------------------------------------------------------

#define JB_DEBUG_MUTEX
#define JB_DEBUG_NORMAL

#ifdef JB_DEBUG_MUTEX
  #include "XdmfObject.h"
  #include <sstream>

  #undef DebugMacro
  #define DebugMacro(p,a)                           \
  {                                                 \
    std::stringstream vtkmsg;                       \
    vtkmsg << "P(" << p << ") " << a;               \
    std::cout << vtkmsg.str().c_str() << std::endl; \
  }

//    SimpleMutexLock::GlobalLock.Lock();             \
//    SimpleMutexLock::GlobalLock.Unlock();           \

  #define Debug(a) DebugMacro(0,a)
  #define Error(a) DebugMacro(0,"Error " << a)
#else
#ifdef JB_DEBUG_NORMAL
  #include <sstream>
  #undef DebugMacro
  #define DebugMacro(p,a)                           \
  {                                                 \
    std::stringstream vtkmsg;                       \
    vtkmsg << "P(" << p << ") " << a;               \
    std::cout << vtkmsg.str().c_str() << std::endl; \
  }
  #define Debug(a) DebugMacro(0,a)
  #define Error(a) DebugMacro(0,"Error " << a)
#else
  #define Debug(a) DebugMacro(a)
  #define Error(a) DebugMacro(a)
#endif // JB_DEBUG_NORMAL
#endif // JB_DEBUG_MUTEX

//----------------------------------------------------------------------------
#endif // H5MButil_H
