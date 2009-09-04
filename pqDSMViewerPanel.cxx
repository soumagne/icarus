#include "pqDSMViewerPanel.h"

// Qt includes
#include <QTreeWidget>
#include <QVariant>
#include <QLabel>
#include <QComboBox>
#include <QTableWidget>
#include <QMessageBox>
#include <QProgressDialog>
#include <QTimer>

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
#include "pqTreeWidget.h"
#include "pqTreeWidgetItem.h"
//
#include "ui_pqDSMViewerPanel.h"
#ifdef WIN32
  #include "Windows.h"
  #define sleep ::Sleep
#endif

class pqDSMViewerPanel::pqUI : public QObject, public Ui::DSMViewerPanel {
public:
  pqUI(pqDSMViewerPanel* p) : QObject(p)
  {
    this->Links = new pqPropertyLinks;
    this->DSMInitialized   = 0;
    this->ActiveSourcePort = 0;
  }
  //
  ~pqUI() {
    delete this->Links;
  }

  // @TODO if the server plugin isn't loaded, this causes an exit
  // must find a way around this
  void CreateProxy() {
    vtkSMProxyManager *pm = vtkSMProxy::GetProxyManager();
    DSMProxy = pm->NewProxy("icarus_helpers", "DSMManager");
    this->DSMProxy->UpdatePropertyInformation();
  }
  //
  bool ProxyCreated() { return this->DSMProxy!=NULL; }
  pqPropertyLinks            *Links;
  int                         DSMInitialized;
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

  this->connect(this->UI->ConnectDSM,
     SIGNAL(clicked()), this, SLOT(onConnectDSM()));

  this->connect(this->UI->H5Dump,
    SIGNAL(clicked()), this, SLOT(onH5Dump()));

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

  //
  ////////////
  // pqTree connect SLOT

}
//----------------------------------------------------------------------------
pqDSMViewerPanel::~pqDSMViewerPanel()
{
}
//----------------------------------------------------------------------------
void pqDSMViewerPanel::onserverAdded(pqServer *server)
{
//  this->ProxyReady();
}
//----------------------------------------------------------------------------
void pqDSMViewerPanel::startRemovingServer(pqServer *server)
{
  if (this->UI->ProxyCreated()) {
    this->UI->DSMProxy->InvokeCommand("DestroyDSM");
    this->UI->DSMProxy = NULL;
    this->UI->DSMInitialized = 0;
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
bool pqDSMViewerPanel::ProxyReady()
{
  if (!this->UI->ProxyCreated()) {
    this->UI->CreateProxy();
    this->linkServerManagerProperties();
    return this->UI->ProxyCreated();
  }
  return true;
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onCreateDSM()
{
  if (this->ProxyReady()) {
    this->UI->DSMProxy->InvokeCommand("CreateDSM");
    this->UI->DSMInitialized = 1;
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onDestroyDSM()
{
  if (this->ProxyReady()) {
    this->UI->DSMProxy->InvokeCommand("DestroyDSM");
    this->UI->DSMInitialized = 0;
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::createPublishNameDialog()
{
  this->publishNameDialog = new QProgressDialog("Publishing name, awaiting connection...", "Cancel", 0, 100);
  connect(this->publishNameDialog, SIGNAL(canceled()), this, SLOT(cancelPublishNameDialog()));
  this->publishNameTimer = new QTimer(this);
  this->publishNameTimer->setInterval(500);
  connect(this->publishNameTimer, SIGNAL(timeout()), this, SLOT(timeoutPublishName()));
  this->publishNameTimer->start();
  this->timeoutPublishName(); // force an immediate update
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::timeoutPublishName()
{
  // try to get the published name from the server
  if (!this->publishedNameFound) {
    vtkSMStringVectorProperty *pn = vtkSMStringVectorProperty::SafeDownCast(
      this->UI->DSMProxy->GetProperty("PublishedPortName"));
    this->UI->DSMProxy->UpdatePropertyInformation(pn);
    const char* name = pn->GetElement(0);
    if (QString(name)!="") {
      QString text = "Publishing Name : \n" + QString(name) + "\n awaiting connection...";
      this->publishNameDialog->setLabelText(text);
      this->publishedNameFound = true;
    }
  }
  this->publishNameDialog->setValue(publishNameSteps);
  this->publishNameSteps++;
  if (this->publishNameSteps > this->publishNameDialog->maximum()) {
    this->cancelPublishNameDialog();
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::cancelPublishNameDialog()
{
  this->publishNameTimer->stop();
  delete this->publishNameTimer;
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onConnectDSM()
{
  if (this->ProxyReady()) {
    this->publishNameSteps = 0;
    this->publishedNameFound = false;
    this->createPublishNameDialog();
    this->UI->DSMProxy->InvokeCommand("ConnectDSM");
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onTestDSM()
{
  if (!this->UI->ActiveSourceProxy) {
    vtkGenericWarningMacro(<<"Nothing to Write");
    return;
  }
  if (!this->UI->DSMInitialized) {
    vtkGenericWarningMacro(<<"Creating DSM before calling Test");
    this->onCreateDSM();
  }

  //
  vtkSMProxyManager* pm = vtkSMProxy::GetProxyManager();
  vtkSmartPointer<vtkSMSourceProxy> XdmfWriter = 
    vtkSMSourceProxy::SafeDownCast(pm->NewProxy("icarus_helpers", "XdmfWriter3"));

  pqSMAdaptor::setProxyProperty(
    XdmfWriter->GetProperty("DSMManager"), 
    this->UI->DSMProxy
  );
  
  pqSMAdaptor::setElementProperty(
    XdmfWriter->GetProperty("FileName"),
#ifndef WIN32
    "/home/soumagne/test"
#else
    "d:/test"
#endif
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
void pqDSMViewerPanel::onH5Dump()
{
  if (this->UI->ProxyCreated()) {
    this->UI->DSMProxy->InvokeCommand("H5DumpLight");
  }
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
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::fillDSMContents()
{
  this->UI->DSMContents->clear();
  pqTreeWidgetItem *item = new pqTreeWidgetItem(this->UI->DSMContents);
  item->setText(0, "test");
  item->setExpanded(true);
}
