#include "pqDSMViewerPanel.h"

// Qt includes
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariant>
#include <QLabel>
#include <QComboBox>
#include <QTableWidget>
#include <QMessageBox>
#include <QProgressDialog>
#include <QTimer>
#include <QInputDialog>
#include <QFileDialog>
#include <QUrl>
#include <QDesktopServices>

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

  this->PublishNameDialog = NULL;
  this->PublishNameTimer = NULL;
  this->PublishNameSteps = 0;
  this->PublishedNameFound = false;
  this->ConnectionFound = false;
  this->DSMContentTree = NULL;
  this->DSMCommType = 0;

  //
  // Link GUI object events to callbacks
  //
  // XDMF XML Commands
  this->connect(this->UI->browseFile,
      SIGNAL(clicked()), this, SLOT(onBrowseFile()));


  // DSM Commands
  this->connect(this->UI->createDSM,
    SIGNAL(clicked()), this, SLOT(onCreateDSM()));

  this->connect(this->UI->destroyDSM,
    SIGNAL(clicked()), this, SLOT(onDestroyDSM()));

  this->connect(this->UI->clearDSM,
    SIGNAL(clicked()), this, SLOT(onClearDSM()));

  this->connect(this->UI->connectDSM,
     SIGNAL(clicked()), this, SLOT(onConnectDSM()));

  this->connect(this->UI->disconnectDSM,
      SIGNAL(clicked()), this, SLOT(onDisconnectDSM()));

  this->connect(this->UI->publishDSM,
     SIGNAL(clicked()), this, SLOT(onPublishDSM()));

  this->connect(this->UI->unpublishDSM,
      SIGNAL(clicked()), this, SLOT(onUnpublishDSM()));

  this->connect(this->UI->h5Dump,
    SIGNAL(clicked()), this, SLOT(onH5Dump()));

  this->connect(this->UI->testDSM,
    SIGNAL(clicked()), this, SLOT(onTestDSM()));

  this->connect(this->UI->displayDSM,
      SIGNAL(clicked()), this, SLOT(onDisplayDSM()));

  //
  // Link paraview events to callbacks
  //
  pqServerManagerModel* smModel =
    pqApplicationCore::instance()->getServerManagerModel();

  this->connect(smModel, SIGNAL(serverAdded(pqServer*)),
    this, SLOT(onServerAdded(pqServer*)));

  this->connect(smModel, SIGNAL(aboutToRemoveServer(pqServer *)),
      this, SLOT(StartRemovingServer(pqServer *)));

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
  // pqTree connect SLOT // useless
  this->connect(this->UI->dsmContents,
      SIGNAL(itemChanged(QTreeWidgetItem *, int)),
      this, SLOT(FillDSMContents(QTreeWidgetItem *, int)));
}
//----------------------------------------------------------------------------
pqDSMViewerPanel::~pqDSMViewerPanel()
{
}
//----------------------------------------------------------------------------
void pqDSMViewerPanel::onServerAdded(pqServer *server)
{
//  this->ProxyReady();
}
//----------------------------------------------------------------------------
void pqDSMViewerPanel::StartRemovingServer(pqServer *server)
{
  if (this->UI->ProxyCreated()) {
    this->UI->DSMProxy->InvokeCommand("DestroyDSM");
    this->UI->DSMProxy = NULL;
    this->UI->DSMInitialized = 0;
  }
}
//----------------------------------------------------------------------------
void pqDSMViewerPanel::LinkServerManagerProperties()
{
  // TBD
}
//---------------------------------------------------------------------------
bool pqDSMViewerPanel::ProxyReady()
{
  if (!this->UI->ProxyCreated()) {
    this->UI->CreateProxy();
    this->LinkServerManagerProperties();
    return this->UI->ProxyCreated();
  }
  return true;
}
//---------------------------------------------------------------------------
bool pqDSMViewerPanel::DSMReady()
{
  if (!this->ProxyReady()) return 0;
  if (!this->UI->DSMInitialized) {
    if (this->UI->xdmfCommTypeComboBox->currentText() == QString("MPI")) {
      this->DSMCommType = XDMF_DSM_COMM_MPI;
      pqSMAdaptor::setElementProperty(
                this->UI->DSMProxy->GetProperty("DsmCommType"),
                this->DSMCommType);
    }
    else if (this->UI->xdmfCommTypeComboBox->currentText() == QString("Sockets")) {
      this->DSMCommType = XDMF_DSM_COMM_SOCKET;
      pqSMAdaptor::setElementProperty(
                this->UI->DSMProxy->GetProperty("DsmCommType"),
                this->DSMCommType);
    }
    if (this->UI->dsmIsServer->isChecked()) {
      pqSMAdaptor::setElementProperty(
          this->UI->DSMProxy->GetProperty("DsmIsServer"),
          1);
    } else {
      pqSMAdaptor::setElementProperty(
          this->UI->DSMProxy->GetProperty("DsmIsServer"),
          0);
    }
    this->UI->DSMProxy->UpdateVTKObjects();
    this->UI->DSMProxy->InvokeCommand("CreateDSM");
    this->UI->DSMInitialized = 1;
  }
  return this->UI->DSMInitialized;
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onBrowseFile()
{
    QList<QUrl> urls;
    urls << QUrl::fromLocalFile(QDesktopServices::storageLocation(QDesktopServices::HomeLocation));

    QFileDialog dialog;
    dialog.setSidebarUrls(urls);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setFileMode(QFileDialog::AnyFile);
    if(dialog.exec()) {
        QString fileName = dialog.selectedFiles().at(0);
        this->UI->xdmfFilePathLineEdit->clear();
        this->UI->xdmfFilePathLineEdit->insert(fileName);
    }
}
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

    if (this->PublishNameTimer) {
      delete this->PublishNameTimer;
      this->PublishNameTimer = NULL;
    }
    if (this->PublishNameDialog) {
      delete this->PublishNameDialog;
      this->PublishNameDialog = NULL;
    }
    if(this->DSMContentTree) {
      delete this->DSMContentTree;
      this->DSMContentTree = NULL;
    }
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onClearDSM()
{
  if (this->DSMReady()) {
    this->UI->DSMProxy->InvokeCommand("ClearDSM");
    if(this->DSMContentTree) {
      delete this->DSMContentTree;
      this->DSMContentTree = NULL;
    }
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onConnectDSM()
{
  if (this->DSMReady()) {
    if (this->DSMCommType == XDMF_DSM_COMM_MPI) {
      QString ipDialog = QInputDialog::getText(this, tr("QInputDialog::getText()"),
          tr("Please enter the MPI port name you want to connect to:"), QLineEdit::Normal);
      if (!ipDialog.isEmpty()) {
        pqSMAdaptor::setElementProperty(
            this->UI->DSMProxy->GetProperty("ServerHostName"),
            ipDialog.toStdString().c_str());
      }
    }
    else if (this->DSMCommType == XDMF_DSM_COMM_SOCKET) {
      QString ipDialog = QInputDialog::getText(this, tr("QInputDialog::getText()"),
          tr("Please enter the hostname/ip address you want to connect to:"), QLineEdit::Normal);

      QString portDialog = QInputDialog::getText(this, tr("QInputDialog::getText()"),
          tr("Please enter the server port number:"), QLineEdit::Normal);

      if (!ipDialog.isEmpty() && !portDialog.isEmpty()) {
        pqSMAdaptor::setElementProperty(
            this->UI->DSMProxy->GetProperty("ServerHostName"),
            ipDialog.toStdString().c_str());
        pqSMAdaptor::setElementProperty(
            this->UI->DSMProxy->GetProperty("ServerPort"),
            portDialog.toStdString().c_str());
      }
    }
    this->UI->DSMProxy->UpdateVTKObjects();
    this->UI->DSMProxy->InvokeCommand("ConnectDSM");
  }

  // TODO set connection found

}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onDisconnectDSM()
{
  if (this->DSMReady()) {
    this->UI->DSMProxy->InvokeCommand("DisconnectDSM");

    // TODO reset connection found
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onPublishDSM()
{
  if (this->DSMReady()) {
    if (!this->PublishedNameFound && !this->ConnectionFound) {
      this->PublishNameSteps = 0;
      this->CreatePublishNameDialog();
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
    if (this->PublishedNameFound == false) {
      QMessageBox msgBox(this);
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setText("No DSM published name found!");
      msgBox.exec();
    } else {
      QMessageBox msgBox(this);
      this->UI->DSMProxy->InvokeCommand("UnpublishDSM");
      this->PublishedNameFound = false;
      this->ConnectionFound = false;
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
    // No longer needed since done automatically on create from the driver itself
    // this->UI->DSMProxy->InvokeCommand("ClearDSM");

    if (!this->UI->ActiveSourceProxy) {
      vtkGenericWarningMacro(<<"Nothing to Write");
      return;
    }
    // Is it still necessary?
  //  if (!this->UI->DSMInitialized) {
  //    vtkGenericWarningMacro(<<"Creating DSM before calling Test");
  //    this->onCreateDSM();
  //  }

    //
    vtkSMProxyManager* pm = vtkSMProxy::GetProxyManager();
    vtkSmartPointer<vtkSMSourceProxy> XdmfWriter =
        vtkSMSourceProxy::SafeDownCast(pm->NewProxy("icarus_helpers", "XdmfWriter4"));
    if (XdmfWriter->GetReferenceCount()>1) {
      // std::cout << " Decrementing the ref count " << std::endl;
      XdmfWriter->Delete();
    }

    pqSMAdaptor::setProxyProperty(
        XdmfWriter->GetProperty("DSMManager"),
        this->UI->DSMProxy
    );

    pqSMAdaptor::setElementProperty(
        XdmfWriter->GetProperty("FileName"),
        "stdin"
/*
#ifndef WIN32
        "/home/soumagne/test"
#else
        "d:/test"
#endif
*/
    );

    pqSMAdaptor::setInputProperty(
        XdmfWriter->GetProperty("Input"),
        this->UI->ActiveSourceProxy,
        this->UI->ActiveSourcePort
    );

    XdmfWriter->UpdateVTKObjects();
    XdmfWriter->UpdatePipeline();

    if(!this->UI->xdmfFilePathLineEdit->text().isEmpty() &&
        (this->UI->xdmfFileTypeComboBox->currentText() == QString("Full description"))
        & (this->ConnectionFound)) {
        pqSMAdaptor::setElementProperty(
                this->UI->DSMProxy->GetProperty("XMFDescriptionFilePath"),
                this->UI->xdmfFilePathLineEdit->text().toStdString().c_str());

        this->UI->DSMProxy->UpdateVTKObjects();
        this->UI->DSMProxy->InvokeCommand("SendDSMXML");
    }
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

    if(this->UI->xdmfFilePathLineEdit->text().isEmpty()) {
        pqSMAdaptor::setElementProperty(
                XdmfReader->GetProperty("FileName"),
#ifndef WIN32
                "/home/soumagne/test.xmf"
#else
                "d:/test.xmf"
#endif
        );
    } else if (this->UI->xdmfFileTypeComboBox->currentText() == QString("Full description")) {
        pqSMAdaptor::setElementProperty(
                XdmfReader->GetProperty("FileName"),
                this->UI->xdmfFilePathLineEdit->text().toStdString().c_str());
    } else if (this->UI->xdmfFileTypeComboBox->currentText() == QString("Pseudo description")) {
        // TODO Call H5Dump / Get back new XML generated file
    }

    XdmfReader->UpdatePropertyInformation();

    XdmfReader->UpdateVTKObjects();
    XdmfReader->UpdatePipeline();
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onH5Dump()
{
  if (this->DSMReady()) {
    //this->UI->DSMProxy->InvokeCommand("H5DumpXML");
    this->UI->DSMProxy->InvokeCommand("H5DumpLight");
    //this->UI->DSMProxy->InvokeCommand("H5Dump");
  }
  if (this->UI->xdmfFileTypeComboBox->currentText() == QString("Full description")) {
      // Do nothing
  }
  else if ((this->UI->xdmfFileTypeComboBox->currentText() == QString("Pseudo description"))
      & (!this->UI->xdmfFilePathLineEdit->text().isEmpty())) {
    // Tell to generate XML file
    pqSMAdaptor::setElementProperty(
        this->UI->DSMProxy->GetProperty("XMFDescriptionFilePath"),
        this->UI->xdmfFilePathLineEdit->text().toStdString().c_str());

    this->UI->DSMProxy->UpdateVTKObjects();
    this->UI->DSMProxy->InvokeCommand("GenerateXMFDescription");
  }

  this->UI->dsmContents->setColumnCount(1);
  if (this->DSMContentTree) delete this->DSMContentTree;
  this->DSMContentTree = new QTreeWidgetItem((QTreeWidget*)0,
      QStringList(QString("Group: \" / \"")));
  this->FillDSMContents(this->DSMContentTree, 0);
  for (int i = 0; i < 10; ++i) {
    this->FillDSMContents(new QTreeWidgetItem(this->DSMContentTree,
        QStringList(QString("Group: 00000%1").arg(i))), 0);
  }
  this->UI->dsmContents->expandItem(this->DSMContentTree);
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
      this->UI->infoTextOutput->clear();
      this->UI->infoTextOutput->insertPlainText(
        this->UI->ActiveSourceProxy->GetVTKClassName()
        );
    }
    else {
      this->UI->ActiveSourceProxy = NULL;
      this->UI->infoTextOutput->clear();
    }
  }
  else {
    this->UI->ActiveSourceProxy = NULL;
    this->UI->infoTextOutput->clear();
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::FillDSMContents(QTreeWidgetItem *item, int node)
{
  this->UI->dsmContents->insertTopLevelItem(node, item);
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::CreatePublishNameDialog()
{
  this->PublishNameDialog = new QProgressDialog("Publishing name, awaiting connection...", "Cancel", 0, 100);
  connect(this->PublishNameDialog, SIGNAL(canceled()), this, SLOT(CancelPublishNameDialog()));
  this->PublishNameTimer = new QTimer(this);
  this->PublishNameTimer->setInterval(800);
  connect(this->PublishNameTimer, SIGNAL(timeout()), this, SLOT(TimeoutPublishName()));
  this->PublishNameTimer->start();
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::TimeoutPublishName()
{
  // try to get the published name from the server
  if (!this->PublishedNameFound) {
    vtkSMStringVectorProperty *pshn = vtkSMStringVectorProperty::SafeDownCast(
        this->UI->DSMProxy->GetProperty("PublishedServerHostName"));
    this->UI->DSMProxy->UpdatePropertyInformation(pshn);
    const char* name = pshn->GetElement(0);
    int port = 0;
    if (this->DSMCommType == XDMF_DSM_COMM_SOCKET) {
      vtkSMIntVectorProperty *psp = vtkSMIntVectorProperty::SafeDownCast(
          this->UI->DSMProxy->GetProperty("PublishedServerPort"));
      this->UI->DSMProxy->UpdatePropertyInformation(psp);
      port = psp->GetElement(0);
    }
    if (QString(name)!="") {
      QString text = "Publishing Name:\n" + QString(name);
      if (this->DSMCommType == XDMF_DSM_COMM_SOCKET) {
        char portString[64];
        sprintf(portString, "%d", port);
        text += "\nOn Port:\n" + QString(portString);
      }
      text += "\nawaiting connection...";
      this->PublishNameDialog->setLabelText(text);
      this->PublishedNameFound = true;
    }
  }

  // try to see if the connection is established
  if (!this->ConnectionFound) {
    vtkSMIntVectorProperty *ac = vtkSMIntVectorProperty::SafeDownCast(
      this->UI->DSMProxy->GetProperty("AcceptedConnection"));
    this->UI->DSMProxy->UpdatePropertyInformation(ac);
    int accepted = ac->GetElement(0);
    if (accepted != 0) {
      this->ConnectionFound = true;
      this->PublishNameSteps = this->PublishNameDialog->maximum();
    }
  }

  this->PublishNameDialog->setValue(this->PublishNameSteps);
  this->PublishNameSteps++;

  if (this->PublishNameSteps > this->PublishNameDialog->maximum()) {
    this->CancelPublishNameDialog();
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::CancelPublishNameDialog()
{
  this->PublishNameTimer->stop();
  if (this->PublishedNameFound && !this->ConnectionFound) {
    this->onUnpublishDSM(); // automatically unpublish DSM
  }
}
