/*=========================================================================

  Project                 : XdmfUtil
  Module                  : XdmfHDFDOM.h

  Authors:
     John Biddiscombe     Jerome Soumagne
     biddisco@cscs.ch     soumagne@cscs.ch

  Copyright (C) CSCS - Swiss National Supercomputing Centre.
  You may use modify and and distribute this code freely providing
  1) This copyright notice appears on all copies of source code
  2) An acknowledgment appears with any substantial usage of the code
  3) If this code is contributed to any other open source project, it
  must not be reformatted such that the indentation, bracketing or
  overall style is modified significantly.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  This work has received funding from the European Community's Seventh
  Framework Programme (FP7/2007-2013) under grant agreement 225967 “NextMuSE”

=========================================================================*/

/*=========================================================================
  This code is derived from an earlier work and is distributed
  with permission from, and thanks to ...
=========================================================================*/

/*******************************************************************/
/*                               XDMF                              */
/*                   eXtensible Data Model and Format              */
/*                                                                 */
/*                                                                 */
/*  Author:                                                        */
/*     Jerry A. Clarke                                             */
/*     clarke@arl.army.mil                                         */
/*     US Army Research Laboratory                                 */
/*     Aberdeen Proving Ground, MD                                 */
/*                                                                 */
/*     Copyright @ 2007 US Army Research Laboratory                */
/*     All Rights Reserved                                         */
/*     See Copyright.txt or http://www.arl.hpc.mil/ice for details */
/*                                                                 */
/*     This software is distributed WITHOUT ANY WARRANTY; without  */
/*     even the implied warranty of MERCHANTABILITY or FITNESS     */
/*     FOR A PARTICULAR PURPOSE.  See the above copyright notice   */
/*     for more information.                                       */
/*                                                                 */
/*******************************************************************/

#ifndef XDMFHDFDOM_H
#define XDMFHDFDOM_H

#include "XdmfLightData.h"

class XdmfHDFDOM : public XdmfLightData {

public :

  XdmfHDFDOM();
  ~XdmfHDFDOM();

  XdmfConstString GetClassName() { return("XdmfHDFDOM"); } ;

  // Set the FileName of the XML Description : stdin or Filename
  XdmfInt32 SetInputFileName(XdmfConstString Filename);

  // Get the FileName of the XML Description
  inline XdmfConstString GetInputFileName() { return(this->GetFileName()); };

  // Set Parser Options. See libxml documentation for values
  // Default = XML_PARSE_NONET | XML_PARSE_XINCLUDE
  XdmfSetValueMacro(ParserOptions, XdmfInt32);

  // Get the XML destination
  XdmfGetValueMacro(Input, istream *);

  // Set the XML destination
  XdmfSetValueMacro(Input, istream *);

  // Return the Low Level root of the tree
  XdmfXmlNode GetTree(void) {return(this->Tree);} ;

  // Parse XML without re-initializing entire DOM
  XdmfXmlNode __Parse(XdmfConstString xml, XdmfXmlDoc *Doc=NULL);

  // Re-Initialize and Parse
  XdmfInt32 Parse(XdmfConstString xml = NULL);

  // Get the Root Node
  XdmfXmlNode GetRoot();

  // Get the Number of immediate Children
  XdmfInt64 GetNumberOfChildren(XdmfXmlNode node = NULL);

  // Get The N'th Child
  XdmfXmlNode GetChild(XdmfInt64 Index , XdmfXmlNode Node);

  // Get Number of Attribute in a Node
  XdmfInt32 GetNumberOfAttributes(XdmfXmlNode Node);

  // Get Attribute Name by Index
  XdmfConstString GetAttributeName(XdmfXmlNode Node, XdmfInt32 Index);

  // Is the XdmfXmlNode a child of "Start" in this DOM
  XdmfInt32  IsChild(XdmfXmlNode ChildToCheck, XdmfXmlNode Node = NULL);

  // Convert DOM to XML String
  XdmfConstString Serialize(XdmfXmlNode node = NULL);

  // Find the n'th occurance of a certain node type
  // Walk the tree and find the first element that is of a certain type.
  // Index ( 0 based ) can be used to find the n'th node that satisfies the criteria.
  // The search can also tree starting at a particular node. IgnoreInfo allows
  // the "Information" Elements not to be counted against Index.
  XdmfXmlNode FindElement(XdmfConstString TagName,
      XdmfInt32 Index= 0,
      XdmfXmlNode Node = NULL,
      XdmfInt32 IgnoreInfo=1);

  // Find the next sibling for the node that is of a certain type. IgnoreInfo allows
  // the "Information" elements to be skipped.
  XdmfXmlNode FindNextElement(XdmfConstString TagName,
      XdmfXmlNode Node,
      XdmfInt32 IgnoreInfo=1);

  // Find the Node that has Attribute="Value" (same tree level)
  XdmfXmlNode FindElementByAttribute(XdmfConstString Attribute,
      XdmfConstString Value,
      XdmfInt32 Index= 0,
      XdmfXmlNode Node = NULL);

  // Find a node using XPath syntax
  XdmfXmlNode FindElementByPath(XdmfConstString Path);

  // Find the number of nodes of a certain type
  XdmfInt32 FindNumberOfElements(XdmfConstString TagName, XdmfXmlNode Node = NULL);

  // Find the number of nodes having Attribute="Value"
  XdmfInt32 FindNumberOfElementsByAttribute(XdmfConstString Attribute,
      XdmfConstString Value,
      XdmfXmlNode Node = NULL);

  // Get XPath of a node
  XdmfConstString GetPath(XdmfXmlNode Node);

  // Get Element Name
  XdmfConstString GetElementName(XdmfXmlNode Node);

  // Get an Attribute. Does not check for CDATA so it's faster
  XdmfConstString  GetAttribute(XdmfXmlNode Node, XdmfConstString Attribute);

  // Get the CDATA of a Node
  XdmfConstString  GetCData(XdmfXmlNode Node);

protected :

  istream        *Input;
  XdmfXmlDoc      Doc;
  XdmfXmlNode     Tree;
  XdmfInt32       ParserOptions;
};

#endif /* XDMFHDFDOM_H */
