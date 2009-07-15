#include "pqDSMViewerPanel.h"

// Qt includes
#include <QTreeWidget>
#include <QVariant>
#include <QLabel>
#include <QComboBox>
#include <QTableWidget>

// VTK includes

// ParaView Server Manager includes
#include "vtkSMInputProperty.h"
#include "vtkSMProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMStringVectorProperty.h"
#include "vtkSMArraySelectionDomain.h"
#include "vtkSMProxyProperty.h"

// ParaView includes
#include "pqActiveServer.h"
#include "pqApplicationCore.h"
#include "pqOutputPort.h"
#include "pqPipelineSource.h"
#include "pqPropertyLinks.h"
#include "pqProxy.h"
#include "pqServer.h"
#include "pqServerManagerSelectionModel.h"
#include "pqServerManagerModelItem.h"
#include "pqServerManagerModel.h"
#include "pqSMAdaptor.h"
#include "pqTreeWidgetCheckHelper.h"
#include "pqTreeWidgetItemObject.h"
//
#include "ui_pqDSMViewerPanel.h"

class pqDSMViewerPanel::pqUI : public QObject, public Ui::DSMViewerPanel {
public:
  pqUI(pqDSMViewerPanel* p) : QObject(p)
  {
    this->Links = new pqPropertyLinks;
  }
  //
  ~pqUI() {
    delete this->Links;
  }
  //
  void CreateProxy() {
    vtkSMProxyManager* pm = vtkSMProxy::GetProxyManager();
    DSMProxy = pm->NewProxy("icarus_helpers", "DSMManager");
    this->DSMProxy->UpdatePropertyInformation();
  }
  //
  bool ProxyCreated() { return this->DSMProxy!=NULL; }
  pqPropertyLinks            *Links;
  vtkSmartPointer<vtkSMProxy> DSMProxy;
  vtkSmartPointer<vtkSMProxy> ActiveSourceProxy;
  int                         ActiveSourcePort;
};

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
pqDSMViewerPanel::pqDSMViewerPanel(QWidget* p) :
  QDockWidget("DSM Manager", p)
{
  this->UI = new pqUI(this);
  this->UI->setupUi(this);

  //
  // Link GUI object events to callbacks
  //
  this->connect(this->UI->CreateDSM,
    SIGNAL(clicked()), this, SLOT(onCreateDSM()));

  this->connect(this->UI->DestroyDSM,
    SIGNAL(clicked()), this, SLOT(onDestroyDSM()));

  this->connect(this->UI->Query,
    SIGNAL(clicked()), this, SLOT(onQueryDSM()));

  this->connect(this->UI->TestDSM,
    SIGNAL(clicked()), this, SLOT(onTestDSM()));

  //
  // Link paraview events to callbacks
  //
  pqServerManagerModel* smModel =
    pqApplicationCore::instance()->getServerManagerModel();

  this->connect(smModel, SIGNAL(serverAdded(pqServer*)),
    this, SLOT(onserverAdded(pqServer*)));

  this->connect(smModel, SIGNAL(aboutToRemoveServer(pqServer *)),
      this, SLOT(startRemovingServer(pqServer *)));

  //
  //
  //
  ////////////
  //keep self up to date whenever a new source becomes the active one
  pqServerManagerSelectionModel *selection =
      pqApplicationCore::instance()->getSelectionModel();

  this->connect(selection, 
                   SIGNAL(currentChanged(pqServerManagerModelItem*)), 
                   this, SLOT(TrackSource())
                   );

  this->connect(smModel,
                   SIGNAL(sourceAdded(pqPipelineSource*)),
                   this, SLOT(TrackSource()));
  this->connect(smModel,
                   SIGNAL(sourceRemoved(pqPipelineSource*)),
                   this, SLOT(TrackSource()));

}
//----------------------------------------------------------------------------
pqDSMViewerPanel::~pqDSMViewerPanel()
{
}
//----------------------------------------------------------------------------
void pqDSMViewerPanel::onserverAdded(pqServer *server)
{
  if (!this->UI->ProxyCreated()) {
    this->UI->CreateProxy();
    this->linkServerManagerProperties();
  }
}
//----------------------------------------------------------------------------
void pqDSMViewerPanel::startRemovingServer(pqServer *server)
{
  if (this->UI->ProxyCreated()) {
    this->UI->DSMProxy->InvokeCommand("DestroyDSM");
    this->UI->DSMProxy = NULL;
  }
}
//----------------------------------------------------------------------------
void pqDSMViewerPanel::linkServerManagerProperties()
{
  // parent class hooks up some of our widgets in the ui
  //  pqNamedObjectPanel::linkServerManagerProperties();

  //this->UI->Links->addPropertyLink(this->UI->CreateDSM,
  //  "clicked", SIGNAL(clicked()),
  //  this->UI->DSMProxy, this->UI->DSMProxy->GetProperty("CreateDSM")); 

}
//---------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onCreateDSM()
{
  if (this->UI->ProxyCreated()) {
    this->UI->DSMProxy->InvokeCommand("CreateDSM");
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onDestroyDSM()
{
  if (this->UI->ProxyCreated()) {
    this->UI->DSMProxy->InvokeCommand("DestroyDSM");
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onTestDSM()
{
  if (!this->UI->ActiveSourceProxy) {
    return;
  }
  //
  vtkSMProxyManager* pm = vtkSMProxy::GetProxyManager();
  vtkSmartPointer<vtkSMSourceProxy> XdmfWriter = 
    vtkSMSourceProxy::SafeDownCast(pm->NewProxy("icarus_helpers", "XdmfWriter2"));

  pqSMAdaptor::setElementProperty(
    XdmfWriter->GetProperty("FileName"), 
    "d:/test.xdmf"
  );

  pqSMAdaptor::setInputProperty(
    XdmfWriter->GetProperty("Input"),
    this->UI->ActiveSourceProxy, 
    this->UI->ActiveSourcePort
  );

  XdmfWriter->UpdateVTKObjects();
  XdmfWriter->UpdatePipeline();
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onQueryDSM()
{
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::TrackSource()
{
  //find the active filter
  pqServerManagerModelItem *item =
    pqApplicationCore::instance()->getSelectionModel()->currentItem();
  if (item)
  {
    pqOutputPort* port = qobject_cast<pqOutputPort*>(item);
    this->UI->ActiveSourcePort = port ? port->getPortNumber() : 0;
    pqPipelineSource *source = port ? port->getSource() : 
      qobject_cast<pqPipelineSource*>(item);
    if (source)
    {
      this->UI->ActiveSourceProxy = source->getProxy();
      this->UI->TextOutput->clear();
      this->UI->TextOutput->insertPlainText(
        this->UI->ActiveSourceProxy->GetVTKClassName()
        );
    }
    else {
      this->UI->ActiveSourceProxy = NULL;
      this->UI->TextOutput->clear();
    }
  }
  else {
    this->UI->ActiveSourceProxy = NULL;
    this->UI->TextOutput->clear();
  }
}
