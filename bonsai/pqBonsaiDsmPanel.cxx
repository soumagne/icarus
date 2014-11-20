/*=========================================================================

  Project                 : Icarus
  Module                  : pqBonsaiDsmPanel.cxx

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
#include "pqBonsaiDsmPanel.h"

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
#include "pqPropertyLinks.h"
//
#include "pqCoreUtilities.h"
#include "pq3DWidget.h"
//
#include "ui_pqBonsaiDsmPanel.h"
//
#include "pqDsmObjectInspector.h"
#include "vtkBonsaiDsmManager.h"
#include "XdmfSteeringParser.h"
#include "vtkCustomPipelineHelper.h"
#include "IcarusConfig.h"
//
#include <vtksys/SystemTools.hxx>
//----------------------------------------------------------------------------
#define XML_USE_TEMPLATE 1
#define XML_USE_ORIGINAL 0
#define XML_USE_SENT     2
//----------------------------------------------------------------------------
#define XML_USE_TEMPLATE 1
#define XML_USE_ORIGINAL 0
#define XML_USE_SENT     2
//----------------------------------------------------------------------------
//
typedef vtkSmartPointer<vtkCustomPipelineHelper> CustomPipeline;
typedef std::vector< CustomPipeline > PipelineList;
//----------------------------------------------------------------------------
class pqBonsaiDsmPanel::pqInternals : public QObject, public Ui::BonsaiDsmPanel
{
public:
  pqInternals(pqBonsaiDsmPanel* p) : QObject(p)
  {
    this->DsmInitialized      = false;
    this->PauseRequested      = false;
    this->DsmListening        = false;
  };

  //
  virtual ~pqInternals() {
    this->DsmProxyHelper     = NULL;

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
    this->DsmProxy.TakeReference(pm->NewProxy("icarus_helpers", "BonsaiDsmManager"));
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
  int                                       PauseRequested;
  int                                       DsmListening;
  vtkSmartPointer<vtkSMProxy>               DsmProxy;
  vtkSmartPointer<vtkSMProxy>               DsmProxyHelper;
  QTcpServer*                               TcpNotificationServer;
  QTcpSocket*                               TcpNotificationSocket;
  pqPropertyLinks Links;
};
//----------------------------------------------------------------------------
pqBonsaiDsmPanel::pqBonsaiDsmPanel(QWidget* p) : QWidget(p)
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
  // Link paraview server manager events to callbacks
  //
  pqServerManagerModel* smModel =
    pqApplicationCore::instance()->getServerManagerModel();

  this->connect(smModel, SIGNAL(aboutToRemoveServer(pqServer *)),
    this, SLOT(onStartRemovingServer(pqServer *)));

  this->connect(smModel, SIGNAL(serverAdded(pqServer *)),
    this, SLOT(onServerAdded(pqServer *)));

  this->connect(this->Internals->quicksync,
    SIGNAL(toggled(bool)), this, SIGNAL(onQuickSync(bool)));
}

//----------------------------------------------------------------------------
//void pqBonsaiDsmPanel::onQuickSync(bool b)
//{
//  if
//}

//----------------------------------------------------------------------------
pqBonsaiDsmPanel::~pqBonsaiDsmPanel()
{
  // Close TCP notification socket
  if (this->Internals->TcpNotificationServer) {
    delete this->Internals->TcpNotificationServer;
  }
  this->Internals->TcpNotificationServer = NULL;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkSMProxy> pqBonsaiDsmPanel::CreateDsmProxy() {
  this->Internals->CreateDsmProxy();

  QObject::connect(&this->Internals->Links,
    SIGNAL(qtWidgetChanged()), this, SLOT(setModified()));

  this->Internals->Links.addPropertyLink(
      this->Internals->samplerate,
      "value",
      SIGNAL(valueChanged(int)),
      this->Internals->DsmProxy,
      this->Internals->DsmProxy->GetProperty("SampleRate"),
      0);

  this->Internals->Links.addPropertyLink(
      this->Internals->quicksync,
      "checked",
      SIGNAL(toggled(bool)),
      this->Internals->DsmProxy,
      this->Internals->DsmProxy->GetProperty("QuickSync"),
      0);

  return this->Internals->DsmProxy;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkSMProxy> pqBonsaiDsmPanel::CreateDsmHelperProxy() {
  this->Internals->CreateDsmHelperProxy();
  return this->Internals->DsmProxyHelper;
}

//----------------------------------------------------------------------------
bool pqBonsaiDsmPanel::DsmListening() {
  return true;
}

//----------------------------------------------------------------------------
void pqBonsaiDsmPanel::LoadSettings()
{
  pqSettings *settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("DsmManager");
  // Active server
  settings->endGroup();
}
//----------------------------------------------------------------------------
void pqBonsaiDsmPanel::SaveSettings()
{
  pqSettings *settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("DsmManager");
  //
  settings->endGroup();
}

//----------------------------------------------------------------------------
void pqBonsaiDsmPanel::onServerAdded(pqServer *server)
{
}
//----------------------------------------------------------------------------
void pqBonsaiDsmPanel::onStartRemovingServer(pqServer *server)
{
  if (this->Internals->DsmProxyCreated()) {
    this->Internals->DsmProxy->InvokeCommand("Destroy");
    this->Internals->DsmProxy = NULL;
    this->Internals->DsmInitialized = 0;
  }
}
//---------------------------------------------------------------------------
bool pqBonsaiDsmPanel::DsmProxyReady()
{
  if (!this->Internals->DsmProxyCreated()) {
    this->Internals->CreateDsmProxy();
    return this->Internals->DsmProxyCreated();
  }
  return true;
}
//---------------------------------------------------------------------------
bool pqBonsaiDsmPanel::DsmReady()
{
  if (!this->DsmProxyReady()) return 0;
  //
  if (!this->Internals->DsmInitialized) {
    //
    this->Internals->DsmProxy->UpdateVTKObjects();
    this->Internals->DsmProxy->InvokeCommand("Create");
    this->Internals->DsmInitialized = 1;
    if (this->initCallback) this->initCallback();
  }
  return this->Internals->DsmInitialized;
}
//-----------------------------------------------------------------------------
void pqBonsaiDsmPanel::onPublish()
{
  if (this->DsmReady() && !this->Internals->DsmListening) {
    this->Internals->DsmProxy->UpdateVTKObjects();
    this->Internals->DsmProxy->InvokeCommand("Publish");
    this->Internals->DsmListening = true;
  }
}
//-----------------------------------------------------------------------------
void pqBonsaiDsmPanel::onUnpublish()
{
  if (this->DsmReady()) {
    this->Internals->DsmProxy->InvokeCommand("Unpublish");
  }
}

//-----------------------------------------------------------------------------
void pqBonsaiDsmPanel::onNewNotificationSocket()
{
  this->Internals->TcpNotificationSocket =
      this->Internals->TcpNotificationServer->nextPendingConnection();

  if (this->Internals->TcpNotificationSocket) {
    this->connect(this->Internals->TcpNotificationSocket, 
      SIGNAL(readyRead()), SLOT(onNotified()), Qt::QueuedConnection);
    this->Internals->TcpNotificationServer->close();
  }

  this->onPublish();
}

//-----------------------------------------------------------------------------
void pqBonsaiDsmPanel::onNotified()
{
  std::cout << "Received a notification " << std::endl;
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
    if (notificationCode == DSM_NOTIFY_CONNECTED) {
      std::cout << "New DSM connection established" << std::endl;
      this->Internals->DsmProxy->InvokeCommand("SignalUpdated");
    } else {
      if (this->Internals->DsmProxyCreated() && this->Internals->DsmInitialized) {
        std::cout << "Received notification ";
        switch (notificationCode) {
          case DSM_NOTIFY_DATA:
            std::cout << "\"New Data\"..." << std::endl;
            emit this->UpdateData();
            break;
          case DSM_NOTIFY_INFORMATION:
            std::cout << "\"New Information\"..." << std::endl;
            this->UpdateInformation();
            break;
          case DSM_NOTIFY_NONE:
            std::cout << "\"NONE : ignoring unlock \"..." << std::endl;
            break;
          case DSM_NOTIFY_WAIT:
            std::cout << "\"Wait\"..." << std::endl;
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
void pqBonsaiDsmPanel::onPreAccept()
{
  // We must always open the DSM in parallel before doing reads or writes
  this->DSMLocked.lock();
}

//-----------------------------------------------------------------------------
void pqBonsaiDsmPanel::onPostAccept()
{
  this->DSMLocked.unlock();
}

//-----------------------------------------------------------------------------
void pqBonsaiDsmPanel::onPause()
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
void pqBonsaiDsmPanel::onPlay()
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
