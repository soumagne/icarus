/*=========================================================================

  Project                 : Icarus
  Module                  : pqH5FDdsmPanel.cxx

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
#include "pqH5FDdsmPanel.h"

// Qt includes
#include <QInputDialog>
#include <QFileDialog>
#include <QUrl>
#include <QDesktopServices>
#include <QTcpServer>
#include <QTcpSocket>

// ParaView Server Manager includes
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxyManager.h"
#include "vtkSMProxyProperty.h"
#include "vtkSMNewWidgetRepresentationProxy.h"
#include "vtkSMPropertyLink.h"
#include "vtkSMOutputPort.h"
#include "vtkSMCompoundSourceProxy.h"

#include "vtkPVDataInformation.h"
#include "vtkPVCompositeDataInformation.h"
#include "vtkBoxWidget2.h"

// ParaView includes
#include "pqApplicationCore.h"
#include "pqSettings.h"
#include "pqServerManagerModel.h"
#include "pqSMAdaptor.h"
#include "pqRenderView.h"
#include "pqActiveView.h"
#include "pqActiveObjects.h"
#include "pqDisplayPolicy.h"
#include "pqAnimationScene.h"
#include "pqTimeKeeper.h"
#include "pqAnimationViewWidget.h"
#include "pqSaveScreenshotReaction.h"
#include "pqPipelineSource.h"
  //
#include "pqDataExportWidget.h"
//
#include "pqCoreUtilities.h"
#include "pq3DWidget.h"
//
#include "ui_pqH5FDdsmPanel.h"
//
#include "pqDsmObjectInspector.h"
#include "vtkHDF5DsmManager.h"
#include "H5FDdsm.h"
#include "XdmfSteeringParser.h"
#include "vtkCustomPipelineHelper.h"
#include "IcarusConfig.h"
//
#include <vtksys/SystemTools.hxx>

#include "H5FDdsmTools.h" // h5fd_DSM_DUMP()
//----------------------------------------------------------------------------
#define XML_USE_TEMPLATE 1
#define XML_USE_ORIGINAL 0
#define XML_USE_SENT     2
//----------------------------------------------------------------------------
//
typedef vtkSmartPointer<vtkCustomPipelineHelper> CustomPipeline;
typedef std::vector< CustomPipeline > PipelineList;
//----------------------------------------------------------------------------
class pqH5FDdsmPanel::pqInternals : public QObject, public Ui::H5FDdsmPanel
{
public:
  pqInternals(pqH5FDdsmPanel* p) : QObject(p)
  {
    this->DsmInitialized      = false;
    this->DsmListening        = false;
    this->PauseRequested      = false;
    this->DsmInterCommType    = 0;
    this->DsmDistributionType = 0;
    this->ActiveSourcePort    = 0;
    this->ActiveSourceProxy   = NULL;
  };

  //
  virtual ~pqInternals() {
    this->DsmProxyHelper     = NULL;
    this->SteeringWriter     = NULL;

    if (this->DsmProxyCreated()) {
      this->DsmProxy->InvokeCommand("Destroy");
    }
    this->DsmProxy           = NULL;
  }
  //
  pqServer *getActiveServer() {
    pqApplicationCore *app = pqApplicationCore::instance();
    pqServerManagerModel *smModel = app->getServerManagerModel();
    pqServer *server = smModel->getItemAtIndex<pqServer*>(0);
    return server;
  }
  void CreateDsmProxy() {
    vtkSMProxyManager *pm = vtkSMProxyManager::GetProxyManager();
    this->DsmProxy.TakeReference(pm->NewProxy("icarus_helpers", "HDF5DsmManager"));
    this->DsmProxy->UpdatePropertyInformation();
  }
  // 
  void CreateDsmHelperProxy() {
    if (!this->DsmProxyCreated()) {
      this->CreateDsmProxy();
    }
    //
    vtkSMProxyManager *pm = vtkSMProxyManager::GetProxyManager();
    this->DsmProxyHelper.TakeReference(pm->NewProxy("icarus_helpers", "DsmProxyHelper"));
    this->DsmProxyHelper->UpdatePropertyInformation();
    this->DsmProxyHelper->UpdateVTKObjects();
  }
  //
  bool DsmProxyCreated() { return this->DsmProxy!=NULL; }
  bool HelperProxyCreated() { return this->DsmProxyHelper!=NULL; }

  // ---------------------------------------------------------------
  // Variables for DSM management
  // ---------------------------------------------------------------
  int                                       DsmInitialized;
  vtkSmartPointer<vtkSMProxy>               DsmProxy;
  vtkSmartPointer<vtkSMProxy>               DsmProxyHelper;
  bool                                      DsmListening;
  bool                                      PauseRequested;
  int                                       DsmInterCommType;
  int                                       DsmDistributionType;
  QTcpServer*                               TcpNotificationServer;
  QTcpSocket*                               TcpNotificationSocket;
  vtkSmartPointer<vtkSMSourceProxy>         SteeringWriter;
  // ---------------------------------------------------------------
  // Experimental, writing to Xdmf 
  // ---------------------------------------------------------------
  vtkSmartPointer<vtkSMProxy>               ActiveSourceProxy;
  int                                       ActiveSourcePort;
};
//----------------------------------------------------------------------------
pqH5FDdsmPanel::pqH5FDdsmPanel(QWidget* p) : QWidget(p)
{
  this->Internals = new pqInternals(this);
  this->Internals->setupUi(this);

  this->initCallback = nullptr;

  // Create a new notification socket
  this->Internals->TcpNotificationServer = new QTcpServer(this);
  this->connect(this->Internals->TcpNotificationServer, 
    SIGNAL(newConnection()), SLOT(onNewNotificationSocket()));
  this->Internals->TcpNotificationServer->listen(QHostAddress::Any,
      VTK_DSM_MANAGER_DEFAULT_NOTIFICATION_PORT);

  //
  // Link GUI object events to callbacks
  //
  
  // DSM Commands
  this->connect(this->Internals->addDsmServer,
      SIGNAL(clicked()), this, SLOT(onAddDsmServer()));

  this->connect(this->Internals->writeToDSM,
      SIGNAL(clicked()), this, SLOT(onWriteDataToDSM()));

  //
  // Link paraview server manager events to callbacks
  //
  pqServerManagerModel* smModel =
    pqApplicationCore::instance()->getServerManagerModel();

  this->connect(smModel, SIGNAL(aboutToRemoveServer(pqServer *)),
    this, SLOT(onStartRemovingServer(pqServer *)));

  this->connect(smModel, SIGNAL(serverAdded(pqServer *)),
    this, SLOT(onServerAdded(pqServer *)));

  //
  // keep self up to date whenever a new source becomes the active one
  //
  this->connect(&pqActiveObjects::instance(), 
    SIGNAL(sourceChanged(pqPipelineSource*)),
    this, SLOT(TrackSource()));

  this->connect(smModel,
    SIGNAL(sourceAdded(pqPipelineSource*)),
    this, SLOT(TrackSource()));

  this->connect(smModel,
    SIGNAL(sourceRemoved(pqPipelineSource*)),
    this, SLOT(TrackSource()));

  //
  // Lock settings so that users cannot break anything
  //
  this->connect(this->Internals->lockSettings,
      SIGNAL(stateChanged(int)), this, SLOT(onLockSettings(int)));
}
//----------------------------------------------------------------------------
pqH5FDdsmPanel::~pqH5FDdsmPanel()
{
  // Close TCP notification socket
  if (this->Internals->TcpNotificationServer) {
    delete this->Internals->TcpNotificationServer;
  }
  this->Internals->TcpNotificationServer = NULL;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkSMProxy> pqH5FDdsmPanel::CreateDsmProxy() {
  this->Internals->CreateDsmProxy();
  return this->Internals->DsmProxy;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkSMProxy> pqH5FDdsmPanel::CreateDsmHelperProxy() {
  this->Internals->CreateDsmHelperProxy();
  return this->Internals->DsmProxyHelper;
}

  //----------------------------------------------------------------------------
bool pqH5FDdsmPanel::DsmListening() {
  return this->Internals->DsmListening;
}

//----------------------------------------------------------------------------
void pqH5FDdsmPanel::LoadSettings()
{
  pqSettings *settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("DsmManager");
  int size = settings->beginReadArray("Servers");
  if (size>0) {
    for (int i=0; i<size; ++i) {
      settings->setArrayIndex(i);
      QString server = settings->value("server").toString();
      if (this->Internals->dsmServerName->findText(server) < 0) {
        this->Internals->dsmServerName->addItem(server);
      }
    }
  }
  settings->endArray();
  // Active server
  this->Internals->dsmServerName->setCurrentIndex(settings->value("Selected", 0).toInt());
  // Size
  this->Internals->dsmSize->setValue(settings->value("Size", 0).toInt());
  // Distribution Type
  this->Internals->dsmDistributionType->setCurrentIndex(settings->value("DistributionType", 0).toInt());
  // Block Size
  this->Internals->dsmBlockLength->setValue(settings->value("BlockLength", 0).toInt());
  // Method
  this->Internals->dsmInterCommType->setCurrentIndex(settings->value("Communication", 0).toInt());
  // Static Communicator
  this->Internals->staticInterCommBox->setChecked(settings->value("StaticCommunicator", 0).toBool());
  // Port
  this->Internals->dsmInterCommPort->setValue(settings->value("Port", 0).toInt());
  // Client/Server/Standalone
  this->Internals->dsmIsServer->setChecked(settings->value("dsmServer", 0).toBool());
  this->Internals->dsmIsClient->setChecked(settings->value("dsmClient", 0).toBool());
  this->Internals->dsmIsStandalone->setChecked(settings->value("dsmStandalone", 0).toBool());
  settings->endGroup();
}
//----------------------------------------------------------------------------
void pqH5FDdsmPanel::SaveSettings()
{
  pqSettings *settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("DsmManager");
  // servers
  settings->beginWriteArray("Servers");
  for (int i=0; i<this->Internals->dsmServerName->model()->rowCount(); i++) {
    settings->setArrayIndex(i);
    settings->setValue("server", this->Internals->dsmServerName->itemText(i));
  }
  settings->endArray();
  // Active server
  settings->setValue("Selected", this->Internals->dsmServerName->currentIndex());
  // Size
  settings->setValue("Size", this->Internals->dsmSize->value());
  // Distribution Type
  settings->setValue("DistributionType", this->Internals->dsmDistributionType->currentIndex());
  // Distribution Type
  settings->setValue("BlockLength", this->Internals->dsmBlockLength->value());
  // Method
  settings->setValue("Communication", this->Internals->dsmInterCommType->currentIndex());
  // Static Communicator
  settings->setValue("StaticCommunicator", this->Internals->staticInterCommBox->isChecked());
  // Port
  settings->setValue("Port", this->Internals->dsmInterCommPort->value());
  // Client/Server/Standalone
  settings->setValue("dsmServer", this->Internals->dsmIsServer->isChecked());
  settings->setValue("dsmClient", this->Internals->dsmIsClient->isChecked());
  settings->setValue("dsmStandalone", this->Internals->dsmIsStandalone->isChecked());
  //
  settings->endGroup();
}

//----------------------------------------------------------------------------
void pqH5FDdsmPanel::onServerAdded(pqServer *server)
{
  // for static communicator
  if (this->Internals->staticInterCommBox->isChecked()) {
    this->Internals->CreateDsmProxy();
    // Force the DSM to be recreated 
    if (this->DsmReady()) {
      // don't do anything here as we need to let notification socket
      // tell the gui that a connection has been made
    }
    else {
      vtkGenericWarningMacro(<<"Static communicator error");      
    }
  }
}
//----------------------------------------------------------------------------
void pqH5FDdsmPanel::onStartRemovingServer(pqServer *server)
{
  if (this->Internals->DsmProxyCreated()) {
    this->Internals->DsmProxy->InvokeCommand("Destroy");
    this->Internals->DsmProxy = NULL;
    this->Internals->DsmInitialized = 0;
  }
}
//---------------------------------------------------------------------------
bool pqH5FDdsmPanel::DsmProxyReady()
{
  if (!this->Internals->DsmProxyCreated()) {
    this->Internals->CreateDsmProxy();
    return this->Internals->DsmProxyCreated();
  }
  return true;
}
//---------------------------------------------------------------------------
bool pqH5FDdsmPanel::DsmReady()
{
  if (!this->DsmProxyReady()) return 0;
  //
  if (!this->Internals->DsmInitialized) {
    //
    bool server = (this->Internals->dsmIsServer->isChecked() || this->Internals->dsmIsStandalone->isChecked());
    bool client = (this->Internals->dsmIsClient->isChecked() || this->Internals->dsmIsStandalone->isChecked());
    pqSMAdaptor::setElementProperty(
      this->Internals->DsmProxy->GetProperty("IsServer"), server);
    //
    if (server) {
      if (this->Internals->dsmInterCommType->currentText() == QString("MPI")) {
        this->Internals->DsmInterCommType = H5FD_DSM_COMM_MPI;
      }
      else if (this->Internals->dsmInterCommType->currentText() == QString("Sockets")) {
        this->Internals->DsmInterCommType = H5FD_DSM_COMM_SOCKET;
      }
      else if (this->Internals->dsmInterCommType->currentText() == QString("MPI_RMA")) {
        this->Internals->DsmInterCommType = H5FD_DSM_COMM_MPI_RMA;
      }
      else if (this->Internals->dsmInterCommType->currentText() == QString("DMAPP")) {
        this->Internals->DsmInterCommType = H5FD_DSM_COMM_DMAPP;
      }
      pqSMAdaptor::setElementProperty(
        this->Internals->DsmProxy->GetProperty("InterCommType"),
        this->Internals->DsmInterCommType);
      //
      pqSMAdaptor::setElementProperty(
        this->Internals->DsmProxy->GetProperty("UseStaticInterComm"),
        this->Internals->staticInterCommBox->isChecked());
      //
      if (this->Internals->dsmDistributionType->currentText() == QString("Contiguous")) {
        this->Internals->DsmDistributionType = H5FD_DSM_TYPE_UNIFORM;
      }
      else if (this->Internals->dsmDistributionType->currentText() == QString("Block Cyclic")) {
        this->Internals->DsmDistributionType = H5FD_DSM_TYPE_BLOCK_CYCLIC;
      }
      else if (this->Internals->dsmDistributionType->currentText() == QString("Random Block Cyclic")) {
        this->Internals->DsmDistributionType = H5FD_DSM_TYPE_BLOCK_RANDOM;
      }
      pqSMAdaptor::setElementProperty(
        this->Internals->DsmProxy->GetProperty("DistributionType"),
        this->Internals->DsmDistributionType);
      //
      pqSMAdaptor::setElementProperty(
        this->Internals->DsmProxy->GetProperty("BlockLength"),
        this->Internals->dsmBlockLength->value());
      //
      pqSMAdaptor::setElementProperty(
        this->Internals->DsmProxy->GetProperty("LocalBufferSizeMBytes"),
        this->Internals->dsmSize->value());
      //
      this->Internals->DsmProxy->UpdateVTKObjects();
      this->Internals->DsmProxy->InvokeCommand("Create");
      this->Internals->DsmInitialized = 1;
    }
    else if (client) {
      this->Internals->DsmProxy->InvokeCommand("ReadConfigFile");
      this->Internals->DsmProxy->InvokeCommand("Create");
      this->Internals->DsmProxy->InvokeCommand("Connect");
      this->Internals->DsmInitialized = 1;
    }
    if (this->initCallback) this->initCallback();
  }
  return this->Internals->DsmInitialized;
}
//-----------------------------------------------------------------------------
void pqH5FDdsmPanel::onLockSettings(int state)
{
  bool locked = (state == Qt::Checked) ? true : false;
  this->Internals->dsmSettingBox->setEnabled(!locked);
  this->Internals->dsmTypeBox->setEnabled(!locked);
}
//-----------------------------------------------------------------------------
void pqH5FDdsmPanel::onAddDsmServer()
{
  QString servername = QInputDialog::getText(this, tr("Add DSM Server"),
            tr("Please enter the host name or IP address of a DSM server you want to add/remove:"), QLineEdit::Normal);
  if ((this->Internals->dsmServerName->findText(servername) < 0) && !servername.isEmpty()) {
    this->Internals->dsmServerName->addItem(servername);
  } else {
    this->Internals->dsmServerName->removeItem(this->Internals->dsmServerName->findText(servername));
  }
}
//-----------------------------------------------------------------------------
void pqH5FDdsmPanel::onPublish()
{
  if (this->DsmReady() && !this->Internals->DsmListening) {
    if (this->Internals->DsmInterCommType == H5FD_DSM_COMM_SOCKET) {
      QString hostname = this->Internals->dsmServerName->currentText();
      pqSMAdaptor::setElementProperty(
          this->Internals->DsmProxy->GetProperty("ServerHostName"),
          hostname.toLatin1().data());

      pqSMAdaptor::setElementProperty(
          this->Internals->DsmProxy->GetProperty("ServerPort"),
          this->Internals->dsmInterCommPort->value());
    }
    this->Internals->DsmProxy->UpdateVTKObjects();
    this->Internals->DsmProxy->InvokeCommand("Publish");
    this->Internals->DsmListening = true;
  }
}
//-----------------------------------------------------------------------------
void pqH5FDdsmPanel::onUnpublish()
{
  if (this->DsmReady() && this->Internals->DsmListening) {
    this->Internals->DsmProxy->InvokeCommand("Unpublish");
    this->Internals->DsmListening = false;
  }
}

//-----------------------------------------------------------------------------
void pqH5FDdsmPanel::onWriteDataToDSM()
{
  if (this->DsmReady()) {
    if (!this->Internals->ActiveSourceProxy) {
      vtkGenericWarningMacro(<<"Nothing to Write");
      return;
    }
    //
    vtkSMProxyManager* pm = vtkSMProxyManager::GetProxyManager();
    vtkSmartPointer<vtkSMSourceProxy> XdmfWriter;
    XdmfWriter.TakeReference(vtkSMSourceProxy::SafeDownCast(pm->NewProxy("icarus_helpers", "XdmfWriter4")));

    pqSMAdaptor::setProxyProperty(
      XdmfWriter->GetProperty("DsmManager"), this->Internals->DsmProxy);

    pqSMAdaptor::setElementProperty(
      XdmfWriter->GetProperty("FileName"), "stdin");

    pqSMAdaptor::setInputProperty(
      XdmfWriter->GetProperty("Input"),
      this->Internals->ActiveSourceProxy,
      this->Internals->ActiveSourcePort
      );

    XdmfWriter->UpdateVTKObjects();
    XdmfWriter->UpdatePipeline();
  }
}


//-----------------------------------------------------------------------------
void pqH5FDdsmPanel::TrackSource()
{
  //find the active filter
  pqPipelineSource* item = pqActiveObjects::instance().activeSource();
  if (item)
  {
    pqOutputPort* port = qobject_cast<pqOutputPort*>(item);
    this->Internals->ActiveSourcePort = port ? port->getPortNumber() : 0;
    pqPipelineSource *source = port ? port->getSource() : 
      qobject_cast<pqPipelineSource*>(item);
    if (source)
    {
      this->Internals->ActiveSourceProxy = source->getProxy();
      this->Internals->infoTextOutput->clear();
      this->Internals->infoTextOutput->insert(
        this->Internals->ActiveSourceProxy->GetVTKClassName()
        );
      
      //
      // make sure the field arrays are propagated to our panel
      //
      if (this->Internals->DsmProxyHelper) {
        vtkSMProperty *ip = this->Internals->DsmProxyHelper->GetProperty("Input");
        pqSMAdaptor::setInputProperty(
          ip,
          this->Internals->ActiveSourceProxy,
          this->Internals->ActiveSourcePort
        );
      }
/*
      // Obsolete : Steering Writer is now driven in pre-post Accept
      // make sure the Steering Writer knows where to get data from
      //
      if (this->Internals->SteeringWriter) {
        vtkSMProperty *ip = this->Internals->SteeringWriter->GetProperty("Input");
        pqSMAdaptor::setInputProperty(
          ip,
          this->Internals->ActiveSourceProxy,
          this->Internals->ActiveSourcePort
        );
      }
*/
    }
    else {
      this->Internals->ActiveSourceProxy = NULL;
      this->Internals->infoTextOutput->clear();
    }
  }
  else {
    this->Internals->ActiveSourceProxy = NULL;
    this->Internals->infoTextOutput->clear();
  }
}
//-----------------------------------------------------------------------------
void pqH5FDdsmPanel::onNewNotificationSocket()
{
  this->Internals->TcpNotificationSocket =
      this->Internals->TcpNotificationServer->nextPendingConnection();

  if (this->Internals->TcpNotificationSocket) {
    this->connect(this->Internals->TcpNotificationSocket, 
      SIGNAL(readyRead()), SLOT(onNotified()), Qt::QueuedConnection);
    this->Internals->TcpNotificationServer->close();
  }

  if (this->Internals->staticInterCommBox->isChecked()) {
    this->onPublish();
  }
}

//-----------------------------------------------------------------------------
void pqH5FDdsmPanel::onNotified()
{
  int error = 0;
  unsigned int notificationCode;
  int bytes = -1;
  //
  while (this->Internals->TcpNotificationSocket->size() > 0) {
    bytes = this->Internals->TcpNotificationSocket->read(
        reinterpret_cast<char*>(&notificationCode), sizeof(notificationCode));
    if (bytes != sizeof(notificationCode)) {
      error = 1;
      std::cerr << "Error when reading from notification socket" << std::endl;
      return;
    }
#ifdef ENABLE_TIMERS
    double ts_notif, te_notif;
    ts_notif = vtksys::SystemTools::GetTime ();
#endif
    if (notificationCode == H5FD_DSM_NOTIFY_CONNECTED) {
      std::cout << "New DSM connection established" << std::endl;
    } else {
      if (this->Internals->DsmProxyCreated() && this->Internals->DsmInitialized) {
        std::cout << "Received notification ";
        switch (notificationCode) {
          case H5FD_DSM_NOTIFY_DATA:
            std::cout << "\"New Data\"...";
            emit this->UpdateData();
            break;
          case H5FD_DSM_NOTIFY_INFORMATION:
            std::cout << "\"New Information\"...";
            this->UpdateInformation();
            break;
          case H5FD_DSM_NOTIFY_NONE:
            std::cout << "\"NONE : ignoring unlock \"...";
            break;
          case H5FD_DSM_NOTIFY_WAIT:
            std::cout << "\"Wait\"...";
            this->onPause();
            this->Internals->PauseRequested = false;
            emit this->UpdateStatus("paused");

            break;
          default:
            error = 1;
            std::cout << "Notification " << notificationCode <<
                " not yet supported, please check simulation code " << std::endl;
            break;
        }
        if (!error) std::cout << "Updated" << std::endl;
        // TODO steered objects are not updated for now
        // this->Internals->DsmProxy->InvokeCommand("UpdateSteeredObjects");
        this->Internals->DsmProxy->InvokeCommand("SignalUpdated");
      }
    }
  }
#ifdef ENABLE_TIMERS
  te_notif = vtksys::SystemTools::GetTime ();
  std::cout << "Notification processed in " << te_notif - ts_notif << " s" << std::endl;
#endif
}

//-----------------------------------------------------------------------------
void pqH5FDdsmPanel::onPreAccept()
{
  // We must always open the DSM in parallel before doing reads or writes
  this->DSMLocked.lock();
  this->Internals->DsmProxy->InvokeCommand("OpenCollectiveRW");

  // But writing steering commands is only done on one process
  // so switch to serial mode before 'accept' updates values
  std::cout << " Setting Serial mode " << std::endl;
  pqSMAdaptor::setElementProperty(this->Internals->DsmProxy->GetProperty("SerialMode"), 1);
  //
}
//-----------------------------------------------------------------------------
void pqH5FDdsmPanel::onPostAccept()
{
  // Steering data has been written, switch back to parallel mode
  // for safety and before steering array exports are done in parallel
  std::cout << " Clearing Serial mode " << std::endl;         
  pqSMAdaptor::setElementProperty(this->Internals->DsmProxy->GetProperty("SerialMode"), 0);
  //
  this->Internals->DsmProxy->InvokeCommand("CloseCollective");
  this->DSMLocked.unlock();
}

//-----------------------------------------------------------------------------
void pqH5FDdsmPanel::onPause()
{
  if (this->DsmReady() && !this->Internals->PauseRequested) {
    const char *steeringCmd = "pause";
    pqSMAdaptor::setElementProperty(
        this->Internals->DsmProxy->GetProperty("SteeringCommand"),
        steeringCmd);
    this->Internals->PauseRequested = true;
    emit this->UpdateStatus("pause requested");
    this->Internals->DsmProxy->UpdateVTKObjects();
  }
}

//-----------------------------------------------------------------------------
void pqH5FDdsmPanel::onPlay()
{
  if (this->DsmReady()) {
    const char *steeringCmd = "play";
    pqSMAdaptor::setElementProperty(
        this->Internals->DsmProxy->GetProperty("SteeringCommand"),
        "Modified");
    pqSMAdaptor::setElementProperty(
        this->Internals->DsmProxy->GetProperty("SteeringCommand"),
        steeringCmd);
    emit this->UpdateStatus("resumed");
    this->Internals->DsmProxy->UpdateVTKObjects();
  }
}
