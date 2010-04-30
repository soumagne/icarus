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
#include <QThread>

// VTK includes

// ParaView Server Manager includes
#include "vtkSMInputProperty.h"
#include "vtkSMProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMStringVectorProperty.h"
#include "vtkSMIntVectorProperty.h"
#include "vtkSMArraySelectionDomain.h"
#include "vtkSMProxyProperty.h"
#include "vtkSMViewProxy.h"
#include "vtkSMRepresentationProxy.h"
#include "vtkProcessModule.h"
#include "vtkProcessModuleConnectionManager.h"

// ParaView includes
#include "pqActiveServer.h"
#include "pqApplicationCore.h"
#include "pqSettings.h"
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
#include "pqView.h"
#include "pqRenderView.h"
#include "pqActiveView.h"
#include "pqDataRepresentation.h"
#include "pqActiveObjects.h"
#include "pqDisplayPolicy.h"
#include "pqAnimationScene.h"

//
#include "ui_pqDSMViewerPanel.h"

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
class pqDSMViewerPanel::pqUI : public QObject, public Ui::DSMViewerPanel 
{
public:
  pqUI(pqDSMViewerPanel* p) : QObject(p)
  {
    this->Links = new pqPropertyLinks;
    this->DSMInitialized   = 0;
    this->ActiveSourcePort = 0;
    this->ActiveServer     = 0;
    this->ActiveView       = 0;
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
  pqServer                   *ActiveServer;
  pqRenderView               *ActiveView;
};
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
pqDSMViewerPanel::pqDSMViewerPanel(QWidget* p) :
QDockWidget("DSM Manager", p)
{
  this->UI = new pqUI(this);
  this->UI->setupUi(this);

  //  this->PublishNameDialog = NULL;
  //  this->PublishNameTimer = NULL;
  //  this->PublishNameSteps = 0;
  this->Connected = false;
  //  this->DSMContentTree = NULL;
  this->DSMCommType = 0;
  this->UpdateTimer = new QTimer(this);
  this->UpdateTimer->setInterval(0);
  connect(this->UpdateTimer, SIGNAL(timeout()), this, SLOT(onUpdateTimeout()));
  this->UpdateTimer->start();

  //
  // Link GUI object events to callbacks
  //

  // XDMF XML Commands
  this->connect(this->UI->browseFile,
    SIGNAL(clicked()), this, SLOT(onBrowseFile()));


  // DSM Commands
  this->connect(this->UI->addServerDSM,
      SIGNAL(clicked()), this, SLOT(onAddServerDSM()));

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

  this->connect(&pqActiveObjects::instance(),
    SIGNAL(serverChanged(pqServer*)),
    this, SLOT(onActiveServerChanged(pqServer*)));

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

  // Track the active view so we can display contents in it
  QObject::connect(&pqActiveView::instance(),
    SIGNAL(changed(pqView*)),
    this, SLOT(onActiveViewChanged(pqView*)));


  //
  ////////////
  // pqTree connect SLOT // useless
  this->connect(this->UI->dsmContents,
    SIGNAL(itemChanged(QTreeWidgetItem *, int)),
    this, SLOT(FillDSMContents(QTreeWidgetItem *, int)));

  //
  // Button Group for Standalone/Server/Client buttons
  //
  this->DSMServerGroup = new QButtonGroup(this->UI);
  this->DSMServerGroup->addButton(this->UI->dsmIsStandalone);
  this->DSMServerGroup->addButton(this->UI->dsmIsServer);
  this->DSMServerGroup->addButton(this->UI->dsmIsClient);
  this->DSMServerGroup->setId(this->UI->dsmIsStandalone,0);
  this->DSMServerGroup->setId(this->UI->dsmIsServer,1);
  this->DSMServerGroup->setId(this->UI->dsmIsClient,2);

  this->connect(this->UI->dsmIsStandalone,
      SIGNAL(clicked()), this, SLOT(onDsmIsStandalone()));
  this->connect(this->UI->dsmIsServer,
      SIGNAL(clicked()), this, SLOT(onDsmIsServer()));
  this->connect(this->UI->dsmIsClient,
      SIGNAL(clicked()), this, SLOT(onDsmIsClient()));

  //
  this->LoadSettings();
}
//----------------------------------------------------------------------------
pqDSMViewerPanel::~pqDSMViewerPanel()
{
  this->SaveSettings();

  if (this->UpdateTimer) delete this->UpdateTimer;
  this->UpdateTimer = NULL;
  if (this->DSMServerGroup) delete this->DSMServerGroup;
  this->DSMServerGroup = NULL;
}
//----------------------------------------------------------------------------
void pqDSMViewerPanel::LoadSettings()
{
  pqSettings *settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("DSMManager");
  int size = settings->beginReadArray("Servers");
  if (size>0) {
    for (int i=0; i<size; ++i) {
      settings->setArrayIndex(i);
      QString server = settings->value("server").toString();
      if (this->UI->dsmServerName->findText(server) < 0) {
        this->UI->dsmServerName->addItem(server);
      }
    }
  }
  settings->endArray();
  // Active server
  this->UI->dsmServerName->setCurrentIndex(settings->value("Selected", 0).toInt());
  // Size
  this->UI->dsmSizeSpinBox->setValue(settings->value("Size", 0).toInt());
  // Method
  this->UI->xdmfCommTypeComboBox->setCurrentIndex(settings->value("Communication", 0).toInt());
  // Port
  this->UI->xdmfCommPort->setValue(settings->value("Port", 0).toInt());
  // Client/Server mode
  int index = settings->value("ClientServerMode", 0).toInt();
  if (index!=-1) this->DSMServerGroup->buttons()[index]->click();
  // Description file type
  this->UI->xdmfFileTypeComboBox->setCurrentIndex(settings->value("DescriptionFileType", 0).toInt());
  // Description file path
  QString descFilePath = settings->value("DescriptionFilePath").toString();
  if(!descFilePath.isEmpty()) {
    this->UI->xdmfFilePathLineEdit->insert(descFilePath);
  }
  //
  settings->endGroup();
}
//----------------------------------------------------------------------------
void pqDSMViewerPanel::SaveSettings()
{
  pqSettings *settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("DSMManager");
  // servers
  settings->beginWriteArray("Servers");
  for (int i=0; i<this->UI->dsmServerName->model()->rowCount(); i++) {
    settings->setArrayIndex(i);
    settings->setValue("server", this->UI->dsmServerName->itemText(i));
  }
  settings->endArray();
  // Active server
  settings->setValue("Selected", this->UI->dsmServerName->currentIndex());
  // Size
  settings->setValue("Size", this->UI->dsmSizeSpinBox->value());
  // Method
  settings->setValue("Communication", this->UI->xdmfCommTypeComboBox->currentIndex());
  // Port
  settings->setValue("Port", this->UI->xdmfCommPort->value());
  // Client/Server mode
  int index = this->DSMServerGroup->checkedId();
  settings->setValue("ClientServerMode", index);
  // Description file type
  settings->setValue("DescriptionFileType", this->UI->xdmfFileTypeComboBox->currentIndex());
  // Description file path
  settings->setValue("DescriptionFilePath", this->UI->xdmfFilePathLineEdit->text());
  //
  settings->endGroup();
}
//----------------------------------------------------------------------------
void pqDSMViewerPanel::onServerAdded(pqServer *server)
{
  this->UI->ActiveServer = server;
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onActiveServerChanged(pqServer* server)
{
  this->UI->ActiveServer = server;
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
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onActiveViewChanged(pqView* view)
{
  pqRenderView* renView = qobject_cast<pqRenderView*>(view);
  this->UI->ActiveView = renView;
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
  //
  if (!this->UI->DSMInitialized) {
    //
    bool server = (this->UI->dsmIsServer->isChecked() || this->UI->dsmIsStandalone->isChecked());
    pqSMAdaptor::setElementProperty(
      this->UI->DSMProxy->GetProperty("DsmIsServer"), server);
    //
    if (this->UI->xdmfCommTypeComboBox->currentText() == QString("MPI")) {
      this->DSMCommType = XDMF_DSM_COMM_MPI;
    }
    else {
      this->DSMCommType = XDMF_DSM_COMM_SOCKET;
    }
    pqSMAdaptor::setElementProperty(
      this->UI->DSMProxy->GetProperty("DsmCommType"),
      this->DSMCommType);
    //
    pqSMAdaptor::setElementProperty(
      this->UI->DSMProxy->GetProperty("DsmLocalBufferSize"),
      this->UI->dsmSizeSpinBox->value());
    //
    this->UI->DSMProxy->UpdateVTKObjects();
    this->UI->DSMProxy->InvokeCommand("CreateDSM");
    this->UI->DSMInitialized = 1;
  }
  return this->UI->DSMInitialized;
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onDsmIsStandalone()
{
  if (this->UI->publishDSM->isEnabled()) this->UI->publishDSM->setEnabled(false);
  if (this->UI->unpublishDSM->isEnabled()) this->UI->unpublishDSM->setEnabled(false);

  if (this->UI->connectDSM->isEnabled()) this->UI->connectDSM->setEnabled(false);
  if (this->UI->disconnectDSM->isEnabled()) this->UI->disconnectDSM->setEnabled(false);

  if (this->UI->xdmfCommPort->isEnabled()) this->UI->xdmfCommPort->setEnabled(false);
  if (this->UI->dsmServerName->isEnabled()) this->UI->dsmServerName->setEnabled(false);
  if (this->UI->addServerDSM->isEnabled()) this->UI->addServerDSM->setEnabled(false);

  if (!this->UI->testDSM->isEnabled()) this->UI->testDSM->setEnabled(true);
  if (!this->UI->h5Dump->isEnabled()) this->UI->h5Dump->setEnabled(true);
  if (!this->UI->clearDSM->isEnabled()) this->UI->clearDSM->setEnabled(true);
  if (!this->UI->displayDSM->isEnabled()) this->UI->displayDSM->setEnabled(true);

  if (!this->UI->storeDSMContents->isEnabled()) this->UI->storeDSMContents->setEnabled(true);
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onDsmIsServer()
{
  if (!this->UI->publishDSM->isEnabled()) this->UI->publishDSM->setEnabled(true);
  if (!this->UI->unpublishDSM->isEnabled()) this->UI->unpublishDSM->setEnabled(true);

  if (this->UI->connectDSM->isEnabled()) this->UI->connectDSM->setEnabled(false);
  if (this->UI->disconnectDSM->isEnabled()) this->UI->disconnectDSM->setEnabled(false);

  if (!this->UI->xdmfCommPort->isEnabled()) this->UI->xdmfCommPort->setEnabled(true);
  if (!this->UI->dsmServerName->isEnabled()) this->UI->dsmServerName->setEnabled(true);
  if (!this->UI->addServerDSM->isEnabled()) this->UI->addServerDSM->setEnabled(true);

  if (this->UI->testDSM->isEnabled()) this->UI->testDSM->setEnabled(false);
  if (!this->UI->h5Dump->isEnabled()) this->UI->h5Dump->setEnabled(true);
  if (!this->UI->clearDSM->isEnabled()) this->UI->clearDSM->setEnabled(true);
  if (!this->UI->displayDSM->isEnabled()) this->UI->displayDSM->setEnabled(true);

  if (!this->UI->storeDSMContents->isEnabled()) this->UI->storeDSMContents->setEnabled(true);
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onDsmIsClient()
{
  if (this->UI->publishDSM->isEnabled()) this->UI->publishDSM->setEnabled(false);
  if (this->UI->unpublishDSM->isEnabled()) this->UI->unpublishDSM->setEnabled(false);

  if (!this->UI->connectDSM->isEnabled()) this->UI->connectDSM->setEnabled(true);
  if (!this->UI->disconnectDSM->isEnabled()) this->UI->disconnectDSM->setEnabled(true);

  if (!this->UI->xdmfCommPort->isEnabled()) this->UI->xdmfCommPort->setEnabled(true);
  if (!this->UI->dsmServerName->isEnabled()) this->UI->dsmServerName->setEnabled(true);
  if (!this->UI->addServerDSM->isEnabled()) this->UI->addServerDSM->setEnabled(true);

  if (!this->UI->testDSM->isEnabled()) this->UI->testDSM->setEnabled(true);
  if (this->UI->h5Dump->isEnabled()) this->UI->h5Dump->setEnabled(false);
  if (this->UI->clearDSM->isEnabled()) this->UI->clearDSM->setEnabled(false);
  if (this->UI->displayDSM->isEnabled()) this->UI->displayDSM->setEnabled(false);

  if (this->UI->storeDSMContents->isEnabled()) this->UI->storeDSMContents->setEnabled(false);
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onAddServerDSM()
{
  QString servername = QInputDialog::getText(this, tr("Add DSM Server"),
            tr("Please enter the host name or IP address of a DSM server you want to add/remove:"), QLineEdit::Normal);
  if (this->UI->dsmServerName->findText(servername) < 0) {
    this->UI->dsmServerName->addItem(servername);
  } else {
    this->UI->dsmServerName->removeItem(this->UI->dsmServerName->findText(servername));
  }
}
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

    //    if (this->PublishNameTimer) {
    //      delete this->PublishNameTimer;
    //      this->PublishNameTimer = NULL;
    //    }
    //    if (this->PublishNameDialog) {
    //      delete this->PublishNameDialog;
    //      this->PublishNameDialog = NULL;
    //    }
    //    if(this->DSMContentTree) {
    //      delete this->DSMContentTree;
    //      this->DSMContentTree = NULL;
    //    }
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onClearDSM()
{
  if (this->DSMReady()) {
    this->UI->DSMProxy->InvokeCommand("ClearDSM");
    //    if(this->DSMContentTree) {
    //      delete this->DSMContentTree;
    //      this->DSMContentTree = NULL;
    //    }
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onConnectDSM()
{
  if (this->DSMReady() && !this->Connected) {
    QString servername;
    if (this->DSMCommType == XDMF_DSM_COMM_MPI) {
      servername = QInputDialog::getText(this, tr("Connect to DSM using MPI"),
          tr("Please enter the MPI port name you want to connect to:"),
          QLineEdit::Normal);
    } else {
      servername = this->UI->dsmServerName->currentText();
    }
    if (!servername.isEmpty()) {
      pqSMAdaptor::setElementProperty(
          this->UI->DSMProxy->GetProperty("ServerHostName"),
          servername.toStdString().c_str());
    }
    //
    if (this->DSMCommType == XDMF_DSM_COMM_SOCKET) {
      pqSMAdaptor::setElementProperty(
        this->UI->DSMProxy->GetProperty("ServerPort"),
        this->UI->xdmfCommPort->value());
    }
    this->UI->DSMProxy->UpdateVTKObjects();
    this->UI->DSMProxy->InvokeCommand("ConnectDSM");
    this->Connected = true;
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onDisconnectDSM()
{
  if (this->DSMReady() && this->Connected) {
    this->UI->DSMProxy->InvokeCommand("DisconnectDSM");
    this->Connected = false;
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onPublishDSM()
{
  if (this->DSMReady() && !this->Connected) {
    //      this->PublishNameSteps = 0;
    //       this->CreatePublishNameDialog();
    if (this->DSMCommType == XDMF_DSM_COMM_SOCKET) {
      QString hostname = this->UI->dsmServerName->currentText();
      pqSMAdaptor::setElementProperty(
          this->UI->DSMProxy->GetProperty("ServerHostName"),
          hostname.toStdString().c_str());

      pqSMAdaptor::setElementProperty(
          this->UI->DSMProxy->GetProperty("ServerPort"),
          this->UI->xdmfCommPort->value());
    }
    this->UI->DSMProxy->UpdateVTKObjects();
    this->UI->DSMProxy->InvokeCommand("PublishDSM");
    this->Connected = true;
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onUnpublishDSM()
{
  if (this->DSMReady() && this->Connected) {
      this->UI->DSMProxy->InvokeCommand("UnpublishDSM");
      this->Connected = false;
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
    //
    vtkSMProxyManager* pm = vtkSMProxy::GetProxyManager();
    vtkSmartPointer<vtkSMSourceProxy> XdmfWriter =
      vtkSMSourceProxy::SafeDownCast(pm->NewProxy("icarus_helpers", "XdmfWriter4"));

    // Delete our reference now and let smart pointer clean up later
    XdmfWriter->UnRegister(NULL);

    pqSMAdaptor::setProxyProperty(
      XdmfWriter->GetProperty("DSMManager"), this->UI->DSMProxy);

    pqSMAdaptor::setElementProperty(
      XdmfWriter->GetProperty("FileName"), "stdin");

    pqSMAdaptor::setInputProperty(
      XdmfWriter->GetProperty("Input"),
      this->UI->ActiveSourceProxy,
      this->UI->ActiveSourcePort
      );

    XdmfWriter->UpdateVTKObjects();
    XdmfWriter->UpdatePipeline();
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onDisplayDSM()
{
  bool force_generate     = false;
  static bool first_time  = true;
  static int current_time = 0;
  static std::string xdmf_description_file_path = this->UI->xdmfFilePathLineEdit->text().toStdString();


  if (this->UI->ProxyCreated() && this->UI->DSMInitialized && this->DSMReady()) {
    vtkSMProxyManager *pm = vtkSMProxy::GetProxyManager();

    if (!this->XdmfReader || this->UI->storeDSMContents->isChecked()) {
      //
      // Create a new Reader proxy and register it with the system
      //
      char proxyName[256];

      if (!first_time) first_time = true;
      if (this->XdmfReader) this->XdmfReader.New();
      this->XdmfReader.TakeReference(vtkSMSourceProxy::SafeDownCast(pm->NewProxy("icarus_helpers", "XdmfReader4")));
      this->XdmfReader->SetConnectionID(pqActiveObjects::instance().activeServer()->GetConnectionID());
      this->XdmfReader->SetServers(vtkProcessModule::DATA_SERVER);
      sprintf(proxyName, "DSM-Contents_%d", current_time);
      pm->RegisterProxy("sources", proxyName, this->XdmfReader);

      //
      // Connect our DSM manager to the reader
      //
      pqSMAdaptor::setProxyProperty(
          this->XdmfReader->GetProperty("DSMManager"), this->UI->DSMProxy
        );
    }

    //
    // If we are using an Xdmf XML file supplied manually or generated, get it
    //
    if (!this->UI->xdmfFilePathLineEdit->text().isEmpty()) {
      if (this->UI->xdmfFileTypeComboBox->currentText() == QString("Full description")) {
        pqSMAdaptor::setElementProperty(
            this->XdmfReader->GetProperty("FileName"),
            this->UI->xdmfFilePathLineEdit->text().toStdString().c_str());
      }
      if (this->UI->xdmfFileTypeComboBox->currentText() == QString("Pseudo description")) {
        force_generate = (this->UI->forceXdmfGeneration->isChecked()) ? true : false;
        // Only re-generate if the description file path has changed or if force is set to true
        if (xdmf_description_file_path != this->UI->xdmfFilePathLineEdit->text().toStdString() || first_time || force_generate) {
          xdmf_description_file_path = this->UI->xdmfFilePathLineEdit->text().toStdString();
          // Generate xdmf file for reading
          this->UI->DSMProxy->InvokeCommand("H5DumpXML");
          pqSMAdaptor::setElementProperty(
              this->UI->DSMProxy->GetProperty("XMFDescriptionFilePath"),
              xdmf_description_file_path.c_str());

          this->UI->DSMProxy->UpdateVTKObjects();
          this->UI->DSMProxy->InvokeCommand("GenerateXMFDescription");
        }
          pqSMAdaptor::setElementProperty(
              this->XdmfReader->GetProperty("FileName"), "stdin");
      }
    } else {
      pqSMAdaptor::setElementProperty(
        this->XdmfReader->GetProperty("FileName"), "stdin");
    }

    //
    // Update before setting up representation to ensure correct 'type' is created
    // Remember that Xdmf supports many data types Regular/Unstructured/etc
    //
    this->XdmfReader->InvokeCommand("Modified");
    this->XdmfReader->UpdatePropertyInformation();
    this->XdmfReader->UpdateVTKObjects();
    this->XdmfReader->UpdatePipeline();

    //
    // Create a representation if we need one : First time or if storing multiple datasets
    //
    if (!this->XdmfRepresentation || this->UI->storeDSMContents->isChecked()) {
      this->XdmfRepresentation = pqActiveObjects::instance().activeView()->getViewProxy()->CreateDefaultRepresentation(this->XdmfReader, 0);
    }

    //
    // Create a pipeline source to appear in the GUI, 
    // findItem creates a new one initially, thenafter returns the same object
    // Mark the item as Unmodified since we manually updated the pipeline ourselves
    //
    pqPipelineSource* source = pqApplicationCore::instance()->
      getServerManagerModel()->findItem<pqPipelineSource*>(this->XdmfReader);
    source->setModifiedState(pqProxy::UNMODIFIED);

    //
    // on First creation, make the object visible.
    //
    if (first_time) {
      pqDisplayPolicy* display_policy = pqApplicationCore::instance()->getDisplayPolicy();
      pqOutputPort *port = source->getOutputPort(0);
      display_policy->setRepresentationVisibility(port, pqActiveObjects::instance().activeView(), 1);
    }

    // 
    // Increment the time as new steps appear.
    // @TODO : To be replaced with GetTimeStep from reader
    //
    QList<pqAnimationScene*> scenes = pqApplicationCore::instance()->getServerManagerModel()->findItems<pqAnimationScene *>();
    foreach (pqAnimationScene *scene, scenes) {
      scene->setAnimationTime(++current_time);
    }

    //
    // Trigger a render : if changed, everything should update as usual
    if (pqActiveObjects::instance().activeView())
    {
      pqActiveObjects::instance().activeView()->render();
    }

    //
    // To prevent deadlock, switch communicators if we are client and server
    //
    if (!this->UI->dsmIsStandalone->isChecked()) {
      this->UI->DSMProxy->InvokeCommand("RequestRemoteChannel");
    }
    //
    if (first_time) first_time = false;
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onH5Dump()
{
  if (this->DSMReady()) {
    this->UI->DSMProxy->InvokeCommand("H5DumpLight");
  }

  //  this->UI->dsmContents->setColumnCount(1);
  //  if (this->DSMContentTree) delete this->DSMContentTree;
  //  this->DSMContentTree = new QTreeWidgetItem((QTreeWidget*)0,
  //      QStringList(QString("Group: \" / \"")));
  //  this->FillDSMContents(this->DSMContentTree, 0);
  //  for (int i = 0; i < 10; ++i) {
  //    this->FillDSMContents(new QTreeWidgetItem(this->DSMContentTree,
  //        QStringList(QString("Group: 00000%1").arg(i))), 0);
  //  }
  //  this->UI->dsmContents->expandItem(this->DSMContentTree);
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
      this->UI->infoTextOutput->insert(
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
void pqDSMViewerPanel::onUpdateTimeout()
{
  // make sure we only get one message at a time.
  this->UpdateTimer->stop();

  // we don't want to create anything just to inspect it, so test flags only
  if (this->UI->ProxyCreated() && this->UI->DSMInitialized) {
    if (this->DSMReady()) {
      vtkSMIntVectorProperty *ur = vtkSMIntVectorProperty::SafeDownCast(
        this->UI->DSMProxy->GetProperty("DsmUpdateReady"));
      this->UI->DSMProxy->UpdatePropertyInformation(ur);
      int ready = ur->GetElement(0);
      if (ready != 0) {
        this->UI->DSMProxy->InvokeCommand("ClearDsmUpdateReady");
        if (this->UI->autoDisplayDSM->isChecked()) {
        this->onDisplayDSM();
        } else {
          if (!this->UI->dsmIsStandalone->isChecked()) {
            this->UI->DSMProxy->InvokeCommand("RequestRemoteChannel");
          }
        }
      }
    }
  }

  // restart the timer before we exit
  this->UpdateTimer->start();
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::DisplayDSMContents()
{
  if (this->DSMReady()) {
    if (!this->UI->ActiveSourceProxy) {
      vtkGenericWarningMacro(<<"Nothing to Write");
      return;
    }
  }
}
/*
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
*/

