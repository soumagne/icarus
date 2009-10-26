#include "H5private.h"    // Generic Functions
#include "H5ACprivate.h"  // Metadata cache
#include "H5Dprivate.h"   // Dataset functions
#include "H5Eprivate.h"   // Error handling
#include "H5Fprivate.h"   // File access
#include "H5FDprivate.h"  // File drivers
#include "H5FDdsm.h"      // MPI-based file drivers
#include "H5Iprivate.h"   // IDs
#include "H5MMprivate.h"  // Memory management
#include "H5Pprivate.h"   // Property lists

#include <cstdlib>
#include <vtksys/SystemTools.hxx>

#include "H5MButil.h"
#include "Tree.h"

#ifdef H5_HAVE_PARALLEL

#define HDF_IO_DEBUG 1
#ifdef HDF_IO_DEBUG
#  define PRINT_INFO(x) std::cout << "(" << file->DsmBuffer->GetComm()->GetId() << ") " << x << std::endl;
#  define PRINT_DSM_INFO(a,x) std::cout << "(" << a << ") " << x << std::endl;
#else
#  define PRINT_INFO(x)
#  define PRINT_DSM_INFO(a,x)
#endif
//
//
//
typedef Tree<H5MB_info> TreeClass;
//
//
//
//--------------------------------------------------------------------------
H5MB_tree_type *H5MB_init() {
  H5MB_tree_type *data = new H5MB_tree_type();
  TreeClass *tree = new TreeClass();
  data->TreePtr = tree;
  tree->set_root( new TreeClass::Node(H5MB_info("/", NULL)) );
  return data;
}
//--------------------------------------------------------------------------
TreeClass::Node *H5MB_find(H5MB_tree_type *treestruct, const std::string &path)
{
  TreeClass *tree = reinterpret_cast<TreeClass *>(treestruct->TreePtr);
  if (!tree) return NULL;
  //
  std::string currentpath;
  for (TreeClass::iterator_dfs it=tree->begin(); it!=tree->end(); ++it) {
    currentpath = "";
    TreeClass::Node *node = it.current_node();
    while (node->parent()) {  
      currentpath = "/" + node->data().path + currentpath;
      node = node->parent();
    }
    if (currentpath==path) {
      return it.current_node();
    }
  }
  return NULL;
}

//--------------------------------------------------------------------------
bool H5MB_add(H5MB_tree_type *treestruct, const char *path, XdmfGrid *grid)
{
  TreeClass *tree = reinterpret_cast<TreeClass *>(treestruct->TreePtr);
  if (!tree) return false;
  //
  if (!tree->get_root()) {
    tree->set_root( new TreeClass::Node(H5MB_info("/", NULL)) );
  }

  std::vector<vtksys::String> paths = 
    vtksys::SystemTools::SplitString(path, '/', true);
  //
  TreeClass::Node *newNode;
  TreeClass::Node *parent = tree->get_root();
  std::string currentpath;
  //
  for (std::vector<vtksys::String>::iterator it=paths.begin(); it!=paths.end(); ++it) {
    //
    currentpath += "/" + (*it);
    //
    TreeClass::Node *temp = H5MB_find(treestruct, currentpath);
    if (temp) {
      parent = temp;
      continue;
    }
    else {
      if ((*it)==paths.back()) {       
        temp = new TreeClass::Node( H5MB_info((*it).c_str(), grid));
      }
      else {
        temp = new TreeClass::Node( H5MB_info((*it).c_str(), NULL));
      }
      parent->add_child(temp);
      parent = temp;
    }
  }
  return true; 
}
//--------------------------------------------------------------------------
bool H5MB_collect(H5MB_tree_type *treestruct, MPI_Comm *comm)
{
  TreeClass *tree = reinterpret_cast<TreeClass *>(treestruct->TreePtr);
  if (!tree) return false;
  //
  bool ret_value = true;

  FUNC_ENTER_NOAPI(H5MB_collect, NULL)

  done: FUNC_LEAVE_NOAPI(ret_value)
}

//--------------------------------------------------------------------------
#endif // H5_HAVE_PARALLEL
//--------------------------------------------------------------------------
