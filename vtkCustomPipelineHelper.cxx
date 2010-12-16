/*=========================================================================

  Project                 : Icarus
  Module                  : vtkCustomPipelineHelper.cxx

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
#include "vtkCustomPipelineHelper.h"

#include <XdmfDOM.h>

// VTK includes

// ParaView Server Manager includes
#include "vtkObjectFactory.h"
#include "vtkSMInputProperty.h"
#include "vtkSMProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMCompoundSourceProxy.h"
#include "vtkSMProxyProperty.h"
#include "vtkSMViewProxy.h"
#include "vtkSMRepresentationProxy.h"
#include "vtkSMPropertyIterator.h"
#include "vtkSMPropertyLink.h"
#include "vtkSMOutputPort.h"
#include "vtkPVDataInformation.h"
#include "vtkProcessModule.h"
#include "vtkPVXMLElement.h"
#include "vtkPVXMLParser.h"
#include "vtkSMObject.h"
#include "vtkSMProxyManager.h"
//
#include "pqSMAdaptor.h"
#include "pqActiveObjects.h"
#include "pqServer.h"
//
#include <sstream>
//----------------------------------------------------------------------------
extern const char *CustomFilter_TransformBlock;
extern const char *CustomFilter_XdmfReaderBlock;
//----------------------------------------------------------------------------
namespace vtkCustomPipelineHelperSpace {
  typedef std::list< std::pair< std::string, std::string > > stringpairlist;
  std::list< std::pair< std::string, std::string > > RegisteredFilters;
};
//----------------------------------------------------------------------------
vtkCustomPipelineHelper::vtkCustomPipelineHelper(const char *name, const char *group)
{
  vtkSMProxyManager *pm = vtkSMProxyManager::GetProxyManager();
  this->Pipeline = vtkSMCompoundSourceProxy::SafeDownCast(pm->NewProxy(name, group));
  this->Pipeline->SetConnectionID(pqActiveObjects::instance().activeServer()->GetConnectionID());
}
//----------------------------------------------------------------------------
vtkCustomPipelineHelper::~vtkCustomPipelineHelper()
{
  this->Pipeline = NULL;
  this->UnRegisterCustomFilters();
}
//----------------------------------------------------------------------------
void vtkCustomPipelineHelper::UnRegisterCustomFilters()
{
  static bool once = true;
  if (once) {
    vtkSMProxyManager *proxyManager = vtkSMProxyManager::GetProxyManager();
    for (vtkCustomPipelineHelperSpace::stringpairlist::iterator it=vtkCustomPipelineHelperSpace::RegisteredFilters.begin();
      it!=vtkCustomPipelineHelperSpace::RegisteredFilters.end(); ++it) 
    {
      proxyManager->UnRegisterCustomProxyDefinition(it->first.c_str(),it->second.c_str());
    }
    once = false;
  }
}
//----------------------------------------------------------------------------
void vtkCustomPipelineHelper::RegisterCustomFilters()
{
  static bool once = true;
  if (once) {
    vtkCustomPipelineHelper::RegisterCustomFilter(CustomFilter_TransformBlock);
    vtkCustomPipelineHelper::RegisterCustomFilter(CustomFilter_XdmfReaderBlock);
    once = false;
  }
}
//----------------------------------------------------------------------------
void vtkCustomPipelineHelper::RegisterCustomFilter(const char *xml)
{
  vtkSMProxyManager *proxyManager = vtkSMProxyManager::GetProxyManager();
  static int num = 5000;
  // Make sure name is unique among filters
  vtkSmartPointer<vtkPVXMLParser> parser = vtkSmartPointer<vtkPVXMLParser>::New();
  parser->Parse(xml);
  vtkPVXMLElement *root = parser->GetRootElement();
  if (root) {
    unsigned int numElems = root->GetNumberOfNestedElements();
    for (unsigned int i=0; i<numElems; i++) {
      vtkPVXMLElement* currentElement = root->GetNestedElement(i);
      if (currentElement->GetName() &&
          strcmp(currentElement->GetName(), "CustomProxyDefinition") == 0)
      {
        const char* name = currentElement->GetAttribute("name");
        const char* group = currentElement->GetAttribute("group");
        if (name && group) {
          vtkCustomPipelineHelperSpace::RegisteredFilters.push_back(
            std::pair<std::string, std::string>(group,name)
          );
//          std::stringstream newname;
//          newname << name << group << num << std::ends;
//          currentElement->SetAttribute("name",newname.str().c_str());
        }
      }
    }
    // Load the custom proxy definitions using the server manager.
    // This should trigger some register events, which will update the
    // list of custom filters.
    proxyManager->LoadCustomProxyDefinitions(root);
  }
};
//----------------------------------------------------------------------------
void vtkCustomPipelineHelper::AddToRenderView(
  vtkSMViewProxy *viewModuleProxy, bool visible) 
{
  // Create a representation proxy for the pipeline
  vtkSMProxy *reprProxy = viewModuleProxy->CreateDefaultRepresentation(this->Pipeline, 0);
  reprProxy->SetConnectionID(pqActiveObjects::instance().activeServer()->GetConnectionID());
  pqSMAdaptor::setInputProperty(reprProxy->GetProperty("Input"), this->Pipeline, 0);
  pqSMAdaptor::setElementProperty(reprProxy->GetProperty("Visibility"), visible);
  reprProxy->UpdateVTKObjects();
  // 
  pqSMAdaptor::addProxyProperty(viewModuleProxy->GetProperty("Representations"), reprProxy);
  viewModuleProxy->UpdateVTKObjects();
}
//----------------------------------------------------------------------------
void vtkCustomPipelineHelper::SetInput(vtkSMSourceProxy *proxy, int outport)
{
  pqSMAdaptor::setInputProperty(this->Pipeline->GetProperty("Input"), proxy, outport);
}
//----------------------------------------------------------------------------
vtkSMOutputPort *vtkCustomPipelineHelper::GetOutputPort(unsigned int port)
{
  return this->Pipeline->GetOutputPort(port);
}
//----------------------------------------------------------------------------
vtkSMCompoundSourceProxy *vtkCustomPipelineHelper::GetCompoundPipeline()
{
  return vtkSMCompoundSourceProxy::SafeDownCast(this->Pipeline);
}
//----------------------------------------------------------------------------
void vtkCustomPipelineHelper::UpdateAll()
{
  if (this->PipelineEnd) {
    this->PipelineEnd->UpdateVTKObjects();
    this->PipelineEnd->UpdatePipeline();
  }
  else {
    this->Pipeline->UpdateVTKObjects();
    this->Pipeline->UpdatePipeline();
  }

  // A nasty side effect of using a compound proxy is that inputs might not be exposed
  // but their GUI panels still show components. After updating, we should trigger
  // an update of domains, so that TreeDomains etc are refreshed.

  vtkSMCompoundSourceProxy *csp = this->GetCompoundPipeline();
  if (this->PipelineEnd) {
    vtkSMProperty* inputProp = this->PipelineEnd->GetProperty("Input");
    if (inputProp) {
      inputProp->UpdateDependentDomains();
    }
  }
  else if (csp) {
    for (int i=0; i<csp->GetNumberOfProxies(); i++) {
      vtkSMSourceProxy *sp = vtkSMSourceProxy::SafeDownCast(csp->GetProxy(i));
      if (sp) {
        sp->UpdatePipelineInformation();
        vtkSMProperty *inputProp = sp->GetProperty("Input");
        if (inputProp) {
          inputProp->UpdateDependentDomains();
        }
      }
    }
  }

}
//----------------------------------------------------------------------------
