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
#include <QInputDialog>

// VTK includes

// ParaView Server Manager includes
#include "vtkSMInputProperty.h"
#include "vtkSMProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMStringVectorProperty.h"
#include "vtkSMIntVectorProperty.h"
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

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
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

  this->publishedNameFound = false;
  this->connectionFound = false;

  //
  // Link GUI object events to callbacks
  //
  this->connect(this->UI->CreateDSM,
    SIGNAL(clicked()), this, SLOT(onCreateDSM()));

  this->connect(this->UI->DestroyDSM,
    SIGNAL(clicked()), this, SLOT(onDestroyDSM()));

  this->connect(this->UI->ConnectDSM,
     SIGNAL(clicked()), this, SLOT(onConnectDSM()));

  this->connect(this->UI->DisconnectDSM,
      SIGNAL(clicked()), this, SLOT(onDisconnectDSM()));

  this->connect(this->UI->PublishDSM,
     SIGNAL(clicked()), this, SLOT(onPublishDSM()));

  this->connect(this->UI->UnpublishDSM,
      SIGNAL(clicked()), this, SLOT(onUnpublishDSM()));

  this->connect(this->UI->H5Dump,
    SIGNAL(clicked()), this, SLOT(onH5Dump()));

  this->connect(this->UI->TestDSM,
    SIGNAL(clicked()), this, SLOT(onTestDSM()));

  this->connect(this->UI->DisplayDSM,
      SIGNAL(clicked()), this, SLOT(onDisplayDSM()));

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
  // TBD
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
//---------------------------------------------------------------------------
bool pqDSMViewerPanel::DSMReady()
{
  if (!this->ProxyReady()) return 0;
  if (!this->UI->DSMInitialized) {
    this->UI->DSMProxy->InvokeCommand("CreateDSM");
    this->UI->DSMInitialized = 1;
//    QMessageBox msgBox(this);
//    msgBox.setIcon(QMessageBox::Information);
//    msgBox.setText("DSM created");
//    msgBox.exec();
  }
  return this->UI->DSMInitialized;
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onCreateDSM()
{
  this->DSMReady();
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onDestroyDSM()
{
  if (this->DSMReady()) {
    this->UI->DSMProxy->InvokeCommand("DestroyDSM");
    this->UI->DSMInitialized = 0;
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setText("DSM destroyed");
    msgBox.exec();

    if (this->publishNameTimer) {
      delete this->publishNameTimer;
      this->publishNameTimer = NULL;
    }
    if (this->publishNameDialog) {
      delete this->publishNameDialog;
      this->publishNameDialog = NULL;
    }
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onConnectDSM()
{
  if (this->DSMReady()) {
    QString text = QInputDialog::getText(this, tr("QInputDialog::getText()"),
        tr("Enter the connection MPI port:"), QLineEdit::Normal);

    if(!text.isEmpty()) {
      pqSMAdaptor::setElementProperty(
          this->UI->DSMProxy->GetProperty("MPIport"),
          text.toStdString().c_str());

      this->UI->DSMProxy->UpdateVTKObjects();
      this->UI->DSMProxy->InvokeCommand("ConnectDSM");
    }
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onDisconnectDSM()
{
  if (this->DSMReady()) {
    this->UI->DSMProxy->InvokeCommand("DisconnectDSM");
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onPublishDSM()
{
  if (this->DSMReady()) {
    if (!this->publishedNameFound && !this->connectionFound) {
      this->publishNameSteps = 0;
      this->createPublishNameDialog();
      this->UI->DSMProxy->InvokeCommand("PublishDSM");
    } else {
      QMessageBox msgBox(this);
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setText("DSM already connected!");
      msgBox.exec();
    }
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onUnpublishDSM()
{
  if (this->DSMReady()) {
    if (this->publishedNameFound == false) {
      QMessageBox msgBox(this);
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setText("No DSM published name found!");
      msgBox.exec();
    } else {
      QMessageBox msgBox(this);
      this->UI->DSMProxy->InvokeCommand("UnpublishDSM");
      this->publishedNameFound = false;
      this->connectionFound = false;
      msgBox.setIcon(QMessageBox::Information);
      msgBox.setText("DSM name unpublished");
      msgBox.exec();
    }
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onTestDSM()
{
  if (this->DSMReady()) {
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

    this->UI->DSMProxy->InvokeCommand("DisconnectDSM");
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onDisplayDSM()
{
  if (this->DSMReady()) {
    //
    vtkSMProxyManager* pm = vtkSMProxy::GetProxyManager();
    vtkSmartPointer<vtkSMSourceProxy> XdmfReader =
        vtkSMSourceProxy::SafeDownCast(pm->NewProxy("icarus_helpers", "XdmfReader3"));

    pqSMAdaptor::setProxyProperty(
        XdmfReader->GetProperty("DSMManager"),
        this->UI->DSMProxy
    );

    pqSMAdaptor::setElementProperty(
        XdmfReader->GetProperty("FileName"),
#ifndef WIN32
        "/home/soumagne/test.xmf"
#else
        "d:/test.xmf"
#endif
    );

    XdmfReader->InvokeCommand("AllGrids");
    XdmfReader->InvokeCommand("AllArrays");

    XdmfReader->UpdateVTKObjects();
    XdmfReader->UpdatePipeline();
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onH5Dump()
{
  if (this->DSMReady()) {
    this->UI->DSMProxy->InvokeCommand("H5DumpLight");
    //this->UI->DSMProxy->InvokeCommand("H5Dump");
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
  if (this->DSMReady()) {
    this->UI->DSMContents->clear();
    pqTreeWidgetItem *item = new pqTreeWidgetItem(this->UI->DSMContents);
    item->setText(0, "test");
    item->setExpanded(true);
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::createPublishNameDialog()
{
  this->publishNameDialog = new QProgressDialog("Publishing name, awaiting connection...", "Cancel", 0, 100);
  connect(this->publishNameDialog, SIGNAL(canceled()), this, SLOT(cancelPublishNameDialog()));
  this->publishNameTimer = new QTimer(this);
  this->publishNameTimer->setInterval(800);
  connect(this->publishNameTimer, SIGNAL(timeout()), this, SLOT(timeoutPublishName()));
  this->publishNameTimer->start();
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

  // try to see if the connection is established
  if (!this->connectionFound) {
    vtkSMIntVectorProperty *pn = vtkSMIntVectorProperty::SafeDownCast(
      this->UI->DSMProxy->GetProperty("AcceptedConnection"));
    this->UI->DSMProxy->UpdatePropertyInformation(pn);
    int accepted = pn->GetElement(0);
    if (accepted != 0) {
//      QMessageBox msgBox(this);
//      msgBox.setIcon(QMessageBox::Information);
//      msgBox.setText("Connection established");
//      msgBox.exec();
      this->connectionFound = true;
      this->publishNameSteps = this->publishNameDialog->maximum();
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
  if (this->publishedNameFound && !this->connectionFound) {
    this->onUnpublishDSM(); // automatically unpublish DSM
  }
}
