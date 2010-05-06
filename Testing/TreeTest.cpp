                                                 //#include "stdafx.h"
#include "TCL/Tree.h"
#include "TCL/multitree.h"
#include "TCL/unique_tree.h"
#include <cassert>
#include <string>
#include <iostream>

namespace utility
{
	template<typename T> void load_tree(T& alpha_tree );
	template<typename T> void traverse_tree(T& alpha_tree);

	template<typename T> void print_tree(T& alpha_tree, const int depth)
	{
		std::cout << alpha_tree.get()->get_text() << std::endl;

		typename T::const_iterator it = alpha_tree.begin(), it_end = alpha_tree.end();
		for ( ; it != it_end; ++it ) {
			for ( int i = 0; i < depth; ++i ) {
				T* parent = &alpha_tree;
				for ( int j = depth - 1; j > i; --j )
					parent = parent->parent();

				std::cout <<  (is_last_child(parent) ? " " : "|");

				std::cout << std::string(2, ' ');
			}
			std::cout << "|" << std::endl;
			std::cout << std::string(depth * 3 + 1, ' ') << "- ";
			print_tree(*it.node(), depth + 1);
		}
	}

	template<typename T> bool is_last_child(const T* node)
	{
		const T* parent = node->parent();
		typename T::const_iterator it = parent->end();
		--it;
		return it->get_text() == node->get()->get_text();
	}
}

class CAlpha
{
public:
	CAlpha()  {}
	CAlpha(const std::string& text_ ) : text(text_) {}
	CAlpha(const char* text_ ) : text(text_) {}
	~CAlpha() {}

	friend bool operator < (const CAlpha& lhs, const CAlpha& rhs) 
		{ return lhs.get_text() < rhs.get_text(); }
	std::string get_text() const { return text; }

private:
	std::string text;
};

int main(int argc, char* argv[])
{
	// load and print tree<>
	tcl::tree<CAlpha> alpha_tree(CAlpha("tree<>"));
	
	utility::load_tree( alpha_tree );
	utility::print_tree(alpha_tree, 0);
	utility::traverse_tree(alpha_tree);
	std::cout << std::endl << std::endl << std::endl;

	return 0;
}

template<typename T> void utility::load_tree(T& alpha_tree)
{
	// create a child iterator
	typename T::iterator it;  

	// insert first node, A, and keep an iterator to it's node
	it = alpha_tree.insert(CAlpha("A"));
	assert(it != alpha_tree.end() && it->get_text() == "A" );
	// try to insert another A.  Should fail for tree & unique_tree
	it = alpha_tree.insert(CAlpha("A"));
	if ( it == alpha_tree.end() )
		std::cout << alpha_tree.get()->get_text() << ": Couldn't insert second A." << std::endl;

	// insert D and E under A
	it = alpha_tree.begin();
	assert(it != alpha_tree.end() && it->get_text() == "A");
	typename T::iterator child_it = it.node()->insert(CAlpha("D"));
	it.node()->insert(CAlpha("E"));
	assert(child_it != it.node()->end() && child_it->get_text() == "D");

	it = child_it;
	// insert  J under D
	it.node()->insert(CAlpha("J"));
	// insert K under D and remember inserted node
	child_it = it.node()->insert(CAlpha("K"));
	assert(child_it != it.node()->end() && child_it->get_text() == "K");
	// insert R and S under K
	child_it.node()->insert(CAlpha("R"));
	child_it.node()->insert(CAlpha("S"));

	// increment it (now at D) to point to E
	++it;
	// insert L under E
	it.node()->insert(CAlpha("L"));

	it = alpha_tree.insert(CAlpha("B"));
	// insert second E and F under B
	child_it = it.node()->insert(CAlpha("E"));  // should fail for unique_tree
	if ( child_it == it.node()->end() )
		std::cout << alpha_tree.get()->get_text() << ": Couldn't insert second E." << std::endl;
	child_it = it.node()->insert(CAlpha("F"));
	// insert M under F
	it = child_it;
	child_it = it.node()->insert(CAlpha("M"));
	// insert T and U under M
	child_it.node()->insert(CAlpha("T"));
	child_it.node()->insert(CAlpha("U"));

	// insert N under F  (it still points to F)
	child_it = it.node()->insert(CAlpha("N"));
	// insert second U and V under N
	if ( child_it.node()->insert(CAlpha("U")) == child_it.node()->end() ) // should fail for unique_tree
		std::cout << alpha_tree.get()->get_text() << ": Couldn't insert second U." << std::endl;

	child_it.node()->insert(CAlpha("V"));
	
	it = alpha_tree.insert(CAlpha("C"));
	// insert G and H under C
	it.node()->insert(CAlpha("G"));
	child_it = it.node()->insert(CAlpha("H"));
	// insert O under H
	it = child_it;
	child_it = it.node()->insert(CAlpha("O"));
	// try to insert another O
	child_it = it.node()->insert(CAlpha("O")); // should fail for tree/unique_tree
	if ( child_it == it.node()->end() )
		std::cout << alpha_tree.get()->get_text() << ": Couldn't insert second O." << std::endl;

	child_it = it.node()->begin();
	assert(child_it != it.node()->end() && child_it->get_text() == "O");
	// insert W under O
	child_it.node()->insert(CAlpha("W"));
	// insert P under H
	it.node()->insert(CAlpha("P"));

	// insert I under C using parent of H (which is C)
	child_it = it.node()->parent()->insert(CAlpha("I"));
	assert(child_it != it.node()->parent()->end() && child_it->get_text() == "I");
	// insert second I under I
	it = child_it;
	child_it = it.node()->insert(CAlpha("I"));  // should fail for unique tree
	if ( child_it == it.node()->end() )
		std::cout << alpha_tree.get()->get_text() << ": Couldn't insert second I." << std::endl;

	// insert Q under original I
	child_it = it.node()->insert(CAlpha("Q"));
	// insert X under Q
	it = child_it;
	child_it = it.node()->insert(CAlpha("X"));
	// insert Y and Z under X
	child_it.node()->insert(CAlpha("Y"));
	child_it.node()->insert(CAlpha("Z"));

	std::cout << std::endl << std::endl;
}

template<typename T> void utility::traverse_tree(T& alpha_tree)
{
	std::cout << std::endl;
	
	std::cout << alpha_tree.get()->get_text() << "::pre_order_iterator" << std::endl;
	typename T::pre_order_iterator pre_it = alpha_tree.pre_order_begin();
	for ( ; pre_it != alpha_tree.pre_order_end(); ++pre_it ) {
		std::cout << pre_it->get_text() << " ";	
	}
	std::cout << std::endl << std::endl;
	
	std::cout << alpha_tree.get()->get_text() << "::post_order_iterator" << std::endl;
	typename T::const_post_order_iterator post_it = alpha_tree.post_order_begin();
	for ( ; post_it != alpha_tree.post_order_end(); ++post_it ) {
		std::cout << post_it->get_text() << " ";	
	}
	std::cout << std::endl << std::endl;
	
	std::cout << alpha_tree.get()->get_text() << "::level_order_iterator" << std::endl;
	typename T::const_level_order_iterator level_it = alpha_tree.level_order_begin();
	for ( ; level_it != alpha_tree.level_order_end(); ++level_it ) {
		std::cout << level_it->get_text() << " ";	
	}
	
	std::cout << std::endl;
}


