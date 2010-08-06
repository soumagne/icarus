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
#include <QTime>

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

  this->Connected = false;
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

  this->connect(this->UI->publishDSM,
    SIGNAL(clicked()), this, SLOT(onPublishDSM()));

  this->connect(this->UI->unpublishDSM,
    SIGNAL(clicked()), this, SLOT(onUnpublishDSM()));

  this->connect(this->UI->autoDisplayDSM,
      SIGNAL(clicked()), this, SLOT(onAutoDisplayDSM()));

  this->connect(this->UI->dsmWriteDisk,
      SIGNAL(clicked()), this, SLOT(onDSMWriteDisk()));

  // Steering commands
  this->connect(this->UI->scRestart,
      SIGNAL(clicked()), this, SLOT(onSCRestart()));

  this->connect(this->UI->scPause,
      SIGNAL(clicked()), this, SLOT(onSCPause()));

  this->connect(this->UI->scPlay,
      SIGNAL(clicked()), this, SLOT(onSCPlay()));

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
  this->LoadSettings();
}
//----------------------------------------------------------------------------
pqDSMViewerPanel::~pqDSMViewerPanel()
{
  this->SaveSettings();

  if (this->UpdateTimer) delete this->UpdateTimer;
  this->UpdateTimer = NULL;
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
  // Description file type
  this->UI->xdmfFileTypeComboBox->setCurrentIndex(settings->value("DescriptionFileType", 0).toInt());
  // Description file path
  QString descFilePath = settings->value("DescriptionFilePath").toString();
  if(!descFilePath.isEmpty()) {
    this->UI->xdmfFilePathLineEdit->insert(descFilePath);
  }
  // Force XDMF Generation
  this->UI->forceXdmfGeneration->setChecked(settings->value("ForceXDMFGeneration", 0).toBool());
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
  // Description file type
  settings->setValue("DescriptionFileType", this->UI->xdmfFileTypeComboBox->currentIndex());
  // Description file path
  settings->setValue("DescriptionFilePath", this->UI->xdmfFilePathLineEdit->text());
  // Force XDMF Generation
  settings->setValue("ForceXDMFGeneration", this->UI->forceXdmfGeneration->isChecked());
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
    pqSMAdaptor::setElementProperty(
      this->UI->DSMProxy->GetProperty("DsmIsServer"), true);
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
void pqDSMViewerPanel::onAddServerDSM()
{
  QString servername = QInputDialog::getText(this, tr("Add DSM Server"),
            tr("Please enter the host name or IP address of a DSM server you want to add/remove:"), QLineEdit::Normal);
  if ((this->UI->dsmServerName->findText(servername) < 0) && !servername.isEmpty()) {
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
void pqDSMViewerPanel::onPublishDSM()
{
  if (this->DSMReady() && !this->Connected) {
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
void pqDSMViewerPanel::onAutoDisplayDSM()
{
  if (this->DSMReady()) {
//    if (this->UI->autoDisplayDSM->isChecked()) {
//      if (this->UI->dsmWriteDisk->isChecked()) {
//        this->UI->dsmWriteDisk->setChecked(false);
//        this->onDSMWriteDisk();
//      }
//      if (!this->UI->storeDSMContents->isEnabled()) this->UI->storeDSMContents->setEnabled(true);
//    }
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onDSMWriteDisk()
{
  if (this->DSMReady()) {
    pqSMAdaptor::setElementProperty(this->UI->DSMProxy->GetProperty("DsmWriteDisk"), 1);
    this->UI->DSMProxy->UpdateVTKObjects();
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onSCPause()
{
  if (this->DSMReady()) {
    const char *steeringCmd = "pause";
    pqSMAdaptor::setElementProperty(
        this->UI->DSMProxy->GetProperty("SteeringCommand"),
        steeringCmd);
    this->UI->infoCurrentSteeringCommand->clear();
    this->UI->infoCurrentSteeringCommand->insert(steeringCmd);
    this->UI->DSMProxy->UpdateVTKObjects();
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onSCPlay()
{
  if (this->DSMReady()) {
    const char *steeringCmd = "play";
    pqSMAdaptor::setElementProperty(
        this->UI->DSMProxy->GetProperty("SteeringCommand"),
        steeringCmd);
    this->UI->infoCurrentSteeringCommand->clear();
    this->UI->infoCurrentSteeringCommand->insert(steeringCmd);
    this->UI->DSMProxy->UpdateVTKObjects();
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onSCRestart()
{
  if (this->DSMReady()) {
    const char *steeringCmd = "restart";
    pqSMAdaptor::setElementProperty(
        this->UI->DSMProxy->GetProperty("SteeringCommand"),
        steeringCmd);
    this->UI->infoCurrentSteeringCommand->clear();
    this->UI->infoCurrentSteeringCommand->insert(steeringCmd);
    this->UI->DSMProxy->UpdateVTKObjects();
  }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onDisplayDSM()
{
  bool force_generate     = false;
  static bool first_time  = true;
  static int current_time = 0;
  static std::string xdmf_description_file_path = this->UI->xdmfFilePathLineEdit->text().toStdString();


  if (this->UI->ProxyCreated() && this->UI->DSMInitialized && this->DSMReady()) {
    vtkSMProxyManager *pm = vtkSMProxy::GetProxyManager();

#ifdef DISABLE_DISPLAY
    if (this->DSMReady()) {
      this->UI->DSMProxy->InvokeCommand("H5DumpLight");
    }
#else
    if (!this->XdmfReader || this->UI->storeDSMContents->isChecked()) {
      //
      // Create a new Reader proxy and register it with the system
      //
      char proxyName[256];

      // When creating reader, make sure first_time is true
      if (!first_time) first_time = true;
      if (this->XdmfReader) this->XdmfReader.New();
      this->XdmfReader.TakeReference(vtkSMSourceProxy::SafeDownCast(pm->NewProxy("icarus_helpers", "XdmfReader4")));
      this->XdmfReader->SetConnectionID(pqActiveObjects::instance().activeServer()->GetConnectionID());
      this->XdmfReader->SetServers(vtkProcessModule::DATA_SERVER);
      sprintf(proxyName, "DSMDataSource%d", current_time);
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
    if (this->UI->xdmfFileTypeComboBox->currentIndex() != 2) { //if not use sent XML
      if (!this->UI->xdmfFilePathLineEdit->text().isEmpty()) {
        if (this->UI->xdmfFileTypeComboBox->currentIndex() == 0) { // Original XDMF File
          pqSMAdaptor::setElementProperty(
              this->XdmfReader->GetProperty("FileName"),
              this->UI->xdmfFilePathLineEdit->text().toStdString().c_str());
        }
        if (this->UI->xdmfFileTypeComboBox->currentIndex() == 1) { // XDMF Template File
          force_generate = (this->UI->forceXdmfGeneration->isChecked()) ? true : false;
          // Only re-generate if the description file path has changed or if force is set to true
          if (xdmf_description_file_path != this->UI->xdmfFilePathLineEdit->text().toStdString() || first_time || force_generate) {
            xdmf_description_file_path = this->UI->xdmfFilePathLineEdit->text().toStdString();
            // Generate xdmf file for reading
            // No need to do H5dump here any more since done in Generator now
            //          this->UI->DSMProxy->InvokeCommand("H5DumpXML");
            pqSMAdaptor::setElementProperty(
                this->UI->DSMProxy->GetProperty("XMFDescriptionFilePath"),
                xdmf_description_file_path.c_str());

            this->UI->DSMProxy->UpdateVTKObjects();
            this->UI->DSMProxy->InvokeCommand("GenerateXMFDescription");
          }
        }
      }
    }
    else {
      pqSMAdaptor::setElementProperty(
        this->XdmfReader->GetProperty("FileName"), "stdin");
    }

    QTime dieTime = QTime::currentTime().addMSecs(10);
    while( QTime::currentTime() < dieTime ) {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
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
      //
      first_time = false;
    }

    // 
    // Increment the time as new steps appear.
    // @TODO : To be replaced with GetTimeStep from reader
    //
    QList<pqAnimationScene*> scenes = pqApplicationCore::instance()->getServerManagerModel()->findItems<pqAnimationScene *>();
    foreach (pqAnimationScene *scene, scenes) {
      scene->setAnimationTime(++current_time);
    }

    dieTime = QTime::currentTime().addMSecs(10);
    while( QTime::currentTime() < dieTime )
	    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    //
    // Trigger a render : if changed, everything should update as usual
    if (pqActiveObjects::instance().activeView())
    {
      pqActiveObjects::instance().activeView()->render();
    }
#endif //DISABLE_DISPLAY

    //
    // To prevent deadlock, switch communicators if we are client and server
    //
    this->UI->DSMProxy->InvokeCommand("RequestRemoteChannel");
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
          this->UI->DSMProxy->InvokeCommand("RequestRemoteChannel");
        }
      }
    }
  }

  // restart the timer before we exit
  this->UpdateTimer->start();
}
//-----------------------------------------------------------------------------
