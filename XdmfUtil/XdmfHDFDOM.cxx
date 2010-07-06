/*=========================================================================

  Project                 : vtkCSCS
  Module                  : XdmfHDFDOM.h

  Copyright (C) CSCS - Swiss National Supercomputing Centre.
  You may use modify and and distribute this code freely providing
  1) This copyright notice appears on all copies of source code
  2) An acknowledgment appears with any substantial usage of the code
  3) If this code is contributed to any other open source project, it
  must not be reformatted such that the indentation, bracketing or
  overall style is modified significantly.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

=========================================================================*/
#include "XdmfHDFDOM.h"

#include <libxml/globals.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xinclude.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
//----------------------------------------------------------------------------
static XdmfXmlNode
XdmfGetNextElement(XdmfXmlNode Node) {

  XdmfXmlNode NextElement = Node->next;
  while (NextElement && (NextElement->type != XML_ELEMENT_NODE)) {
    NextElement = NextElement->next;
  }
  return(NextElement);
}
//----------------------------------------------------------------------------
XdmfHDFDOM::XdmfHDFDOM()
{
  this->Tree = NULL;
  this->Input = &cin;
  this->Doc = NULL;
  this->SetFileName("stdin");
  this->SetWorkingDirectory("");
  // Allow Indenting on Serialization
  xmlIndentTreeOutput = 1;
  xmlKeepBlanksDefault(0);
  // Set Default Options
  this->ParserOptions = XML_PARSE_NOENT | XML_PARSE_XINCLUDE | XML_PARSE_NONET;
}
//----------------------------------------------------------------------------
XdmfHDFDOM::~XdmfHDFDOM()
{
  XdmfDebug("Destroying DOM");
  if (this->Input != &cin) {
    XdmfDebug("Deleting Input");
    ifstream *OldInput = (ifstream *)this->Input;
    OldInput->close();
    delete this->Input;
    this->Input = &cin;
  }
  if (this->Doc) xmlFreeDoc((xmlDoc *)this->Doc);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfHDFDOM::GetNumberOfAttributes(XdmfXmlNode Node)
{
  XdmfInt32  NumberOfAttributes = 0;
  xmlAttr *attr;

  if (!Node) return(0);
  attr = Node->properties;
  while (attr) {
    attr = attr->next;
    NumberOfAttributes++;
  }
  return(NumberOfAttributes);
}
//----------------------------------------------------------------------------
XdmfConstString
XdmfHDFDOM::GetAttributeName(XdmfXmlNode Node, XdmfInt32 Index)
{
  XdmfInt32  EIndex = 0;
  xmlAttr *attr;

  if (!Node) return(0);
  attr = Node->properties;
  while (attr && (EIndex < Index)) {
    attr = attr->next;
    EIndex++;
  }
  if (attr) {
    return((XdmfConstString)attr->name);
  }
  return(NULL);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfHDFDOM::IsChild(XdmfXmlNode ChildToCheck, XdmfXmlNode Node)
{
  XdmfXmlNode child;

  // Check All Children
  for(child=Node->xmlChildrenNode; child ; child=child->next) {
    if (child->type == XML_ELEMENT_NODE) {
      // Is this it?
      if (child == ChildToCheck) {
        return(XDMF_SUCCESS);
      }
      // Check Its children
      if (this->IsChild(ChildToCheck, child) == XDMF_SUCCESS) {
        return(XDMF_SUCCESS);
      }
    }
  }
  return(XDMF_FAIL);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfHDFDOM::SetInputFileName(XdmfConstString Filename)
{
  if (this->Input != &cin) {
    ifstream *OldInput = (ifstream *)this->Input;
    OldInput->close();
    delete this->Input;
    this->Input = &cin;
  }
  if (XDMF_WORD_CMP(Filename, "stdin")) {
    this->Input = &cin;
  } else {
    ifstream        *NewInput = new ifstream(Filename);
    if (!NewInput) {
      XdmfErrorMessage("Can't Open Input File " << Filename);
      return(XDMF_FAIL);
    }
    this->Input = NewInput;
  }
  this->SetFileName(Filename);
  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfConstString
XdmfHDFDOM::Serialize(XdmfXmlNode Node)
{
  int buflen;
  xmlBufferPtr bufp;

  if (!Node) Node = this->Tree;
  if (!Node) return(NULL);
  bufp = xmlBufferCreate();
  buflen = xmlNodeDump(bufp, this->Doc, Node, 0, 1);
  return(this->DupBuffer(bufp));
}
//----------------------------------------------------------------------------
XdmfXmlNode
XdmfHDFDOM::__Parse(XdmfConstString inxml, XdmfXmlDoc *DocPtr)
{
  XdmfXmlNode Root = NULL;
  XdmfXmlDoc  pDoc;
  int parserOptions;

  parserOptions = this->ParserOptions;
  if (inxml) {
    // Is  this XML or a File Name
    if (inxml[0] == '<') {
      // It's XML
      pDoc = xmlReadMemory(inxml, strlen(inxml), NULL, NULL, parserOptions);
    } else {
      // It's a File Name
      this->SetInputFileName(inxml);
      pDoc = xmlReadFile(this->GetInputFileName(), NULL, parserOptions);
    }
  } else {
    pDoc = xmlReadFile(this->GetInputFileName(), NULL, parserOptions);
  }
  if (pDoc) {
    if (parserOptions & XML_PARSE_XINCLUDE) {
      if (xmlXIncludeProcess(pDoc) < 0) {
        xmlFreeDoc(pDoc);
        pDoc = NULL;
      }
    }
    Root = xmlDocGetRootElement(pDoc);
  }
  if (DocPtr) *DocPtr = pDoc;
  return(Root);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfHDFDOM::Parse(XdmfConstString inxml)
{
  XdmfXmlNode Root;

  // Remove Previous Data
  if (this->Doc) xmlFreeDoc((xmlDoc *)this->Doc);
  this->Tree = NULL;

  Root = this->__Parse(inxml, &this->Doc);
  if (Root) {
    this->Tree = Root;
  } else {
    return(XDMF_FAIL);
  }
  return(XDMF_SUCCESS);
}
//----------------------------------------------------------------------------
XdmfXmlNode
XdmfHDFDOM::GetChild(XdmfInt64 Index, XdmfXmlNode Node)
{
  XdmfXmlNode child;

  if (!Node) {
    Node = this->Tree;
  }
  if (!Node) return(0);
  child = Node->children;
  if (Index == 0) {
    if (child->type != XML_ELEMENT_NODE) {
      child = XdmfGetNextElement(child);
    }
  }
  while (child && Index) {
    child = XdmfGetNextElement(child);
    Index--;
  }
  return(child);
}
//----------------------------------------------------------------------------
XdmfInt64
XdmfHDFDOM::GetNumberOfChildren(XdmfXmlNode Node)
{
  XdmfInt64 Index = 0;
  XdmfXmlNode child;

  if (!Node) {
    Node = this->Tree;
  }
  if (!Node) return(0);
  child = Node->children;
  while (child) {
    if (child->type == XML_ELEMENT_NODE) Index++;
    child = XdmfGetNextElement(child);
  }
  return(Index);
}
//----------------------------------------------------------------------------
XdmfXmlNode
XdmfHDFDOM::GetRoot(void)
{
  return(this->Tree);
}
//----------------------------------------------------------------------------
XdmfXmlNode
XdmfHDFDOM::FindElement(XdmfConstString TagName, XdmfInt32 Index, XdmfXmlNode Node, XdmfInt32 IgnoreInfo)
{
  XdmfString type = (XdmfString)TagName;
  XdmfXmlNode child;

  // this->SetDebug(1);
  if (TagName) {
    XdmfDebug("FindElement " << TagName << " Index = " << Index);
  } else {
    XdmfDebug("FindElement NULL Index = " << Index);
  }
  if (!Node) {
    if (!this->Tree) return(NULL);
    Node = this->Tree;
  }
  child = Node->children;
  if (!child) return(NULL);
  if (type) {
    if (STRNCASECMP(type, "NULL", 4) == 0) type = NULL;
  }
  if (!type) {
    if (IgnoreInfo) {
      while (child) {
        if (XDMF_WORD_CMP("Information", (const char *)(child)->name) == 0) {
          if (Index <= 0) {
            return(child);
          }
          Index--;
        }
        child = XdmfGetNextElement(child);
      }
    } else {
      return(this->GetChild(Index, Node));
    }
  } else {
    while (child) {
      if (IgnoreInfo && XDMF_WORD_CMP("Information", (const char *)(child)->name)) {
        child = XdmfGetNextElement(child);
      } else {
        if (XDMF_WORD_CMP((const char *)type, (const char *)(child)->name)) {
          if (Index <= 0) {
            return(child);
          }
          Index--;
        }
        child = XdmfGetNextElement(child);
      }
    }
  }
  return(NULL);
}
//----------------------------------------------------------------------------
XdmfXmlNode
XdmfHDFDOM::FindNextElement(XdmfConstString TagName, XdmfXmlNode Node, XdmfInt32 IgnoreInfo)
{
  XdmfString type = (XdmfString)TagName;
  XdmfXmlNode child;

  // this->SetDebug(1);
  if (TagName) {
    XdmfDebug("FindNextElement" << TagName);
  } else {
    XdmfDebug("FindNextElement NULL");
  }
  if (!Node) {
    if (!this->Tree) return(NULL);
    Node = this->Tree->children;
  }
  if (!Node) return(NULL);
  if (type) {
    if (STRNCASECMP(type, "NULL", 4) == 0) type = NULL;
  }

  child = XdmfGetNextElement(Node);
  while (child) {
    if (IgnoreInfo && XDMF_WORD_CMP("Information", (const char *)(child)->name)) {
      // skip Information elements.
    } else {
      if (!type ||
          (XDMF_WORD_CMP((const char *)type, (const char *)(child)->name))) {
        return child;
      }
    }
    child = XdmfGetNextElement(child);
  }
  return (NULL);
}
//----------------------------------------------------------------------------
XdmfXmlNode
XdmfHDFDOM::FindElementByAttribute(XdmfConstString Attribute,
    XdmfConstString Value, XdmfInt32 Index, XdmfXmlNode Node)
{
  XdmfXmlNode child;

  if (!Node) {
    Node = this->Tree;
  }
  if (!Node) return(NULL);
  child = Node->children;
  while (child) {
    xmlChar *txt = xmlGetProp(child, (xmlChar *)Attribute);

    if (XDMF_WORD_CMP((const char *)txt, (const char *)Value)) {
      if (Index <= 0) {
        xmlFree(txt);
        return(child);
      }
      xmlFree(txt);
      Index--;
    }
    child = XdmfGetNextElement(child);
  }
  return(NULL);
}
//----------------------------------------------------------------------------
XdmfXmlNode
XdmfHDFDOM::FindElementByPath(XdmfConstString Path)
{
  // Use an XPath expression to return a Node
  xmlXPathContextPtr xpathCtx;
  xmlXPathObjectPtr xpathObj;
  xmlNodeSetPtr nodes;
  XdmfXmlNode child = NULL;
  int i;

  if (!this->Doc) {
    XdmfErrorMessage("XML must be parsed before XPath is available");
    return(NULL);
  }
  // Create the context
  xpathCtx = xmlXPathNewContext(this->Doc);
  if (xpathCtx == NULL) {
    XdmfErrorMessage("Can't Create XPath Context");
    return(NULL);
  }
  xpathObj = xmlXPathEvalExpression((const xmlChar *)Path, xpathCtx);
  if (xpathObj == NULL) {
    XdmfErrorMessage("Can't evaluate XPath : " << Path);
    return(NULL);
  }
  // Return the first XML_ELEMENT_NODE
  nodes = xpathObj->nodesetval;
  if (!nodes) {
    XdmfErrorMessage("No Elements Match XPath Expression : " << Path);
    return(NULL);
  }
  XdmfDebug("Found " << nodes->nodeNr << " Element that match XPath expression " << Path);
  for(i=0 ; i < nodes->nodeNr ; i++) {
    child = nodes->nodeTab[i];
    if (child->type == XML_ELEMENT_NODE) {
      // this is it
      xmlXPathFreeObject(xpathObj);
      xmlXPathFreeContext(xpathCtx);
      return(child);
    }
  }
  xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(xpathCtx);
  return(NULL);
}
//----------------------------------------------------------------------------
XdmfConstString
XdmfHDFDOM::GetPath(XdmfXmlNode Node)
{
  char *txt;

  if (!Node) {
    XdmfErrorMessage("Node == NULL");
    return((XdmfConstString)NULL);
  }
  txt = (char *)xmlGetNodePath(Node);
  return(this->DupChars(txt));
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfHDFDOM::FindNumberOfElements(XdmfConstString TagName, XdmfXmlNode Node)
{
  XdmfXmlNode child;
  XdmfInt32 Index = 0;

  if (!Node) {
    if (!this->Tree) return(XDMF_FAIL);
    Node = this->Tree;
  }
  child = Node->children;
  if (!child) return(0);
  while (child) {
    if (XDMF_WORD_CMP(TagName, (const char *)child->name)) {
      Index++;
    }
    child = XdmfGetNextElement(child);
  }
  return(Index);
}
//----------------------------------------------------------------------------
XdmfInt32
XdmfHDFDOM::FindNumberOfElementsByAttribute(XdmfConstString Attribute,
    XdmfConstString Value, XdmfXmlNode Node)
{
  XdmfInt32 NElements = 0;
  XdmfXmlNode child;

  if (!Node) {
    Node = this->Tree;
  }
  if (!Node) return(0);
  child = Node->children;
  while (child) {
    xmlChar *txt;
    txt = xmlGetProp(child, (xmlChar *)Attribute);
    if (XDMF_WORD_CMP((const char *)txt, (const char *)Value)) {
      NElements++;
    }
    xmlFree(txt);
    child = XdmfGetNextElement(child);
  }
  return(0);
}
//----------------------------------------------------------------------------
XdmfConstString
XdmfHDFDOM::GetElementName(XdmfXmlNode Node)
{
 return (XdmfConstString)Node->name;
}
//----------------------------------------------------------------------------
XdmfConstString
XdmfHDFDOM::GetAttribute(XdmfXmlNode Node, XdmfConstString Attribute)
{
  if (!Node) {
    Node = this->Tree;
  }
  if (!Node) return(0);
  return((XdmfConstString)xmlGetProp(Node, (xmlChar *)Attribute));
}
//----------------------------------------------------------------------------
XdmfConstString
XdmfHDFDOM::GetCData(XdmfXmlNode Node)
{
  char    *txt;

  if (!Node) {
    Node = this->Tree;
  }
  if (!Node) return(0);
  txt = (char *)xmlNodeListGetString(this->Doc, Node->xmlChildrenNode, 1);
  return(this->DupChars(txt));
}
//----------------------------------------------------------------------------
