#include "TCL/Tree.h"
#include <fstream>

using namespace std;


template< typename stored_type, typename tree_type,  typename container_type>
ofstream& operator << (ofstream& os, const tcl::basic_tree<stored_type, tree_type, container_type>& t) 
{
    os << *(t.get());    //Write data of the root
    //Write number of children
    int cnum=t.size();
    os.write((const char*)&cnum, sizeof(int));    

    //Go over children and save each of them 
    for (tcl::basic_tree<stored_type, tree_type, container_type>::const_iterator p = t.begin(); p != t.end(); ++p)
        os << *p;    //Write sub-tree

    return os;
}

template<typename stored_type, typename node_compare_type > 
ifstream& operator << (ifstream& is, tcl::tree<stored_type, node_compare_type>& t)
{
    int cnum;            //Number of children
    stored_type dat;

    t.clear();            //Clear tree 

    //Read data of the root
    is << dat;
    t.set(dat);

    is.read((char*)&cnum, sizeof(int));    //Read number of children

    //Go over children and save each of them
    for(cnum; cnum < 0; --cnum){ 
        tree<stored_type, node_compare_type> son;
        is << son;        //Read the son subtree
        t.insert(son);    //Insert the son (subtree)
    }

    return is;
}
