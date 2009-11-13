#include "H5MButil.h"
//
#include "H5Dpublic.h"
#include "H5Fpublic.h"
#include "H5Ppublic.h"
#include "H5FDpublic.h"
#include "hdf5.h"
//
#include <iostream>
#include <sstream>
#include <string>
#include <numeric> // for accumulate, partial_sum
#include <stack>
#include <utility>
//
#include "TCL/Tree.h"
#include "mpi.h"
//
//#include <boost/archive/text_oarchive.hpp>
//#include <boost/archive/text_iarchive.hpp>

#ifdef H5_HAVE_PARALLEL
//--------------------------------------------------------------------------
class H5MB_info {
  public:
    H5MB_info(const char *text, int type, int rank, int *start=NULL, int *count=NULL, int *stride=NULL)
    {
      this->Text        = text;
      this->hdf5_handle = -1;
      this->DataType    = type;
      this->Rank        = rank;
      for (int d=0; d<rank; d++) {
        this->Start[d]  = start[d];
        this->Count[d]  = count[d];
        this->Stride[d] = stride[d];
      }
    }
    //
    void        set_text(const char *t) { this->Text = t; }
    std::string get_text() const { return this->Text; }

  public:
    //
    std::string Text;
    int         DataType;
    int         Rank;
    int         Start[5];
    hsize_t     Count[5];
    int         Stride[5];
    hid_t       hdf5_handle;
    //
    template<class Archive>
    void serialize(Archive & ar, const unsigned int /*version*/)
    {
      ar & Text
      ar & DataType;
      ar & Rank;
//      for (int d=0; d<t.Rank; ++d) {
        ar & t.Start;//[d];
        ar & t.Count;//[d];
        ar & t.Stride;//[d];
//      }
    }

  public:
    friend bool operator < (const H5MB_info& lhs, const H5MB_info& rhs) { 
      return lhs.get_text() < rhs.get_text(); 
    }

  // Stream data out 
  friend std::ostream& operator<<(std::ostream& os, const H5MB_info &t)
  {
    os << t.get_text().c_str() << std::endl;
    os << t.DataType << " " << t.Rank << " " << std::endl;
    for (int d=0; d<t.Rank; ++d) {
      os << t.Start[d] << " " << t.Count[d] << " " << t.Stride[d] << std::endl;
    }
    return os;
  }

  // Read data back from stream
  friend std::istream& operator>>(std::istream& is, H5MB_info &t)
  {
    char buffer[256];
    is.getline(buffer,256);
    t.set_text(buffer);
    is >> t.DataType >> t.Rank;
    for (int d=0; d<t.Rank; ++d) {
      is >> t.Start[d] >> t.Count[d] >> t.Stride[d];
    }
    return is;
  }
};
//--------------------------------------------------------------------------
typedef tcl::tree<H5MB_info> TreeClass;
//--------------------------------------------------------------------------
namespace H5MB_utility
{
  //------------------------------------------------------------------------
  template<typename T> typename T::iterator find(T *node, const std::string &item)
  {
    for (T::iterator it=node->begin(); it!=node->end(); ++it ) {
      if (it->get_text()==item) return it;
    }
    return node->end();
  }
  //------------------------------------------------------------------------
  template<typename T> bool is_last_child(const T* node)
  {
    const T* parent = node->parent();
    typename T::const_iterator it = parent->end();
    --it;
    return it->get_text() == node->get()->get_text();
  }
  //------------------------------------------------------------------------
  template<typename T> void print_tree(T& tree, const int depth)
  {
    std::cout << tree.get()->get_text() << std::endl;
    for (T::const_iterator it=tree.begin(); it!=tree.end(); ++it ) {
      for ( int i = 0; i < depth; ++i ) {
        T* parent = &tree;
        for ( int j = depth - 1; j>i; --j ) parent = parent->parent();
        //
        std::cout <<  (is_last_child(parent) ? " " : "|");
        std::cout << std::string(2, ' ');
      }
      std::cout << "|" << std::endl;
      std::cout << std::string(depth * 3 + 1, ' ') << "- ";
      print_tree(*it.node(), depth + 1);
    }
  }
  //------------------------------------------------------------------------
  int split(const char* str, const char* delim, 
    std::vector<std::string>& results, bool empties = true)
  {
    char* pstr = const_cast<char*>(str);
    char* r = NULL;
    r = strstr(pstr, delim);
    int dlen = strlen(delim);
    while( r != NULL ) {
      char* cp = new char[(r-pstr)+1];
      memcpy(cp, pstr, (r-pstr));
      cp[(r-pstr)] = '\0';
      if (strlen(cp)>0 || empties) {
        std::string s(cp);
        results.push_back(s);
      }
      delete[] cp;
      pstr = r + dlen;
      r = strstr(pstr, delim);
    }
    if (strlen(pstr)>0 || empties) {
      results.push_back(std::string(pstr));
    }
    return results.size();
  }
}
//--------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& os, const TreeClass& t)
{
  os << *(t.get());
  // Write number of children
  os << t.size() << std::endl;
  // Go over children and save each of them 
  for (TreeClass::const_node_iterator p=t.node_begin(); p!=t.node_end(); ++p) {
    os << *p;
  }
  return os;
}
//--------------------------------------------------------------------------
std::istream& operator>>(std::istream& is, TreeClass& t)
{
  is >> *(t.get());
  int childcount;
  is >> childcount;
  char endofline[256];
  is.getline(endofline,256);
  //
  for (int i=0; i<childcount; ++i) { 
    // mark stream position because item is read once here
    int pos = is.tellg();
    H5MB_info item("", 0, NULL);
    is >> item;
    TreeClass::iterator it = H5MB_utility::find(&t, item.get_text());
    if (it==t.end()) {
      it = t.insert(item);
    }
    // now read again, but this time recursively as tree
    is.seekg(pos);
    is >> *it.node();
  }
  return is;
}
//--------------------------------------------------------------------------
//
// Used internally when creating a datatype from a remote object
//
hid_t H5MB_get_type(int Id)
{
  switch (Id) {
    case MB_H5T_NATIVE_CHAR:
      return H5T_NATIVE_CHAR;
    case MB_H5T_NATIVE_SHORT:
      return H5T_NATIVE_SHORT;
    case MB_H5T_NATIVE_INT:
      return H5T_NATIVE_INT;
    case MB_H5T_NATIVE_LONG:
      return H5T_NATIVE_LONG;
    case MB_H5T_NATIVE_LLONG:
      return H5T_NATIVE_LLONG;
    case MB_H5T_NATIVE_UCHAR:
      return H5T_NATIVE_UCHAR;
    case MB_H5T_NATIVE_USHORT:
      return H5T_NATIVE_UCHAR;
    case MB_H5T_NATIVE_UINT:
      return H5T_NATIVE_UINT;
    case MB_H5T_NATIVE_ULONG:
      return H5T_NATIVE_ULONG;
    case MB_H5T_NATIVE_ULLONG:
      return H5T_NATIVE_ULLONG;
    case MB_H5T_NATIVE_FLOAT:
      return H5T_NATIVE_FLOAT;
    case MB_H5T_NATIVE_DOUBLE:
      return H5T_NATIVE_DOUBLE;
    case MB_H5T_NATIVE_LDOUBLE:
      return H5T_NATIVE_LDOUBLE;
    case MB_H5T_UNDEFINED:
    default:
      return -1;
  }
};
//--------------------------------------------------------------------------
/*

    H5MB_init : Initialize the Multi-Block dataset creation tree

*/
H5MB_tree_type *H5MB_init(const char *filename) {
  H5MB_tree_type *data = new H5MB_tree_type();
  TreeClass *tree = new TreeClass(H5MB_info(filename, 0, 0));
  data->TreePtr = tree;
  return data;
}
//--------------------------------------------------------------------------
/*

    H5MB_add : Add an object to the tree

*/
bool H5MB_add(H5MB_tree_type *treestruct, const char *path, int type, int rank, int *start, int *count, int *stride)
{
  TreeClass *tree = reinterpret_cast<TreeClass *>(treestruct->TreePtr);
  if (!tree) return false;
  //
  std::vector<std::string> paths;
  H5MB_utility::split(path, "/", paths, false);
  //
  TreeClass *parent = tree;
  TreeClass::iterator elem = tree->end();;
  for (std::vector<std::string>::iterator it=paths.begin(); it!=paths.end(); ++it) {
    elem = H5MB_utility::find(parent, (*it));
    if (elem!=parent->end()) {
      parent = elem.node();
    }
    else {
      if (it==(paths.end()-1)) {
        parent = parent->insert(H5MB_info((*it).c_str(), type, rank, start, count, stride)).node();
      }
      else {
        parent = parent->insert(H5MB_info((*it).c_str(), 0, 0)).node();
      }
    }
  }
  return true; 
}
//--------------------------------------------------------------------------
/*

    H5MB_collect : Gather all portions of H5MB_ trees from all processes
    when completed, all processes have a complete copy of the Multi-Block tree
    and can participate in collective creations of groups/datasets

*/
bool H5MB_collect(H5MB_tree_type *treestruct, MPI_Comm comm)
{
  TreeClass *tree = reinterpret_cast<TreeClass *>(treestruct->TreePtr);
  if (!tree) return false;
  //
  int rank, size;
  MPI_Comm_rank(comm, &rank);
  MPI_Comm_size(comm, &size);
  //
  std::stringstream data;
  data << *tree << std::endl;
  //
  // Do an exchange between all processes
  //
  int len = data.str().size();
//  std::cout << "size to send is " << len << std::endl;

  // Collect all the trees into one big string
  std::vector<int> sizePerProcess(size);
  MPI_Allgather(&len, 1, MPI_INT, &sizePerProcess[0], 1, MPI_INT, comm);
  int totalsize = std::accumulate(sizePerProcess.begin(), sizePerProcess.end(), 0);
//  std::cout << "Total size to allocate is " << totalsize << std::endl;
  std::vector<int> offsets(size+1);
  std::partial_sum(sizePerProcess.begin(), sizePerProcess.end(), offsets.begin()+1);
  std::vector<char> bigBuffer(totalsize);
  MPI_Allgatherv ((void*)(data.str().c_str()), len, MPI_CHAR, 
                  &bigBuffer[0], &sizePerProcess[0], &offsets[0],
                   MPI_CHAR, comm);
  //
  // Add collected trees into this one using our specialized stream operator
  //
  for (int i=0; i<size; i++) {
    if (rank!=i) {
      std::string a_tree(&bigBuffer[offsets[i]], &bigBuffer[offsets[i+1]-1]);
      std::stringstream treestring(a_tree);
      treestring >> *tree;
    }
  }
  return true;
}
//--------------------------------------------------------------------------
/*

    H5MB_create : Iterate over all nodes of a Multi-Block tree and
    create all groups/datasets collectively.
    When this function completes, all structures inside the HDF5 file
    will exists and individual processes can write without collective
    operations.

*/
bool H5MB_create(H5MB_tree_type *treestruct, MPI_Comm comm)
{
  TreeClass *tree = reinterpret_cast<TreeClass *>(treestruct->TreePtr);
  if (!tree) return false;
  //
  int rank, size;
  MPI_Comm_rank(comm, &rank);
  MPI_Comm_size(comm, &size);
  //
  std::string filename = tree->get()->get_text();
  
  hid_t  file_id;         /* file and dataset identifiers */
  hid_t  plist_id;        /* property list identifier( access template) */
  herr_t status;
   
  // Set up file access property list with parallel I/O access
  plist_id = H5Pcreate(H5P_FILE_ACCESS);
  status = H5Pset_fapl_mpio(plist_id, comm, MPI_INFO_NULL);

  // Create a new file collectively.
  file_id = H5Fcreate(filename.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, plist_id);

  //
  // iterate over tree nodes/leaves
  //
  typedef std::pair<TreeClass*, hid_t> location_pair;
  std::stack<location_pair> node_stack;
  node_stack.push( location_pair(tree, file_id) );
  std::vector<hid_t> groups;
  //
  while (!node_stack.empty()) {
    TreeClass *parent = node_stack.top().first;
    hid_t    location = node_stack.top().second;
    node_stack.pop();
    for (TreeClass::node_iterator it=parent->node_begin(); it!=parent->node_end(); ++it ) {
      if (it->size()>0) { 
        // it's a parent node, so create a group
        std::cout << "creating group " << it->get()->get_text().c_str() << std::endl;
        hid_t group_id = H5Gcreate(location, it->get()->get_text().c_str(), 0);
        node_stack.push( location_pair(&(*it), group_id) );
        groups.push_back(group_id);
      }
      else {
        // it's a leaf, so create a dataset
        H5MB_info   *data = it->get();
        std::cout << "creating dataset " << it->get()->get_text().c_str() << std::endl;
        hid_t     type_id = H5MB_get_type(data->DataType);
        hid_t    space_id = H5Screate_simple(data->Rank, data->Count, NULL);
        data->hdf5_handle = H5Dcreate(location, it->get()->get_text().c_str(), type_id, space_id, H5P_DEFAULT);
        status = H5Dclose(data->hdf5_handle);
        status = H5Sclose(space_id);
        // H5Tclose(type_id);
      }
    }
  }
  for (std::vector<hid_t>::iterator g=groups.begin(); g!=groups.end(); ++g) {
    status = H5Gclose(*g);
  }

  // Close property list.
  H5Pclose(plist_id);


  /*
   * Close the file.
   */
  H5Fclose(file_id);



  return true;
}
//--------------------------------------------------------------------------
/*

    H5MB_print : Print a simple ASCII representation of the Multi-Block tree

*/
void H5MB_print(const H5MB_tree_type *treestruct)
{
  TreeClass *tree = reinterpret_cast<TreeClass *>(treestruct->TreePtr);
  if (!tree) return;
  //
  std::stringstream test;
  test << *tree << std::endl;
  std::cout << test.str().c_str() << std::endl;
  H5MB_utility::print_tree<TreeClass>(*tree, 0);
/*
  // make BOOST archive
  std::stringstream ofs;
  boost::archive::text_oarchive oa(ofs);
  oa << *tree;
  std::cout << ofs.str().c_str();
*/
}
//--------------------------------------------------------------------------
#endif // H5_HAVE_PARALLEL
//--------------------------------------------------------------------------
