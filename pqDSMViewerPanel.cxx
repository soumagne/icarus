/*=========================================================================

  Project                 : Icarus
  Module                  : pqDSMViewerPanel.cxx

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
#include "pqDSMViewerPanel.h"

// Qt includes
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariant>
#include <QSpinBox>
#include <QDoubleSpinBox>
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
#include "vtkSMPropertyIterator.h"
#include "vtkProcessModule.h"
#include "vtkProcessModuleConnectionManager.h"
#include "vtkPVXMLElement.h"

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
#include "pqObjectInspectorWidget.h"
#include "pqNamedWidgets.h"
#include "pq3DWidget.h"
//
#include "pq3DWidgetInterface.h"
#include "pqBoxWidget.h"
#include "pqDistanceWidget.h"
#include "pqImplicitPlaneWidget.h"
#include "pqLineSourceWidget.h"
#include "pqPointSourceWidget.h"
#include "pqSphereWidget.h"
#include "pqSplineWidget.h"
//
#include "ui_pqDSMViewerPanel.h"
//
#include "H5FDdsmComm.h"
#include "XdmfSteeringParser.h"
//
#include <vtksys/SystemTools.hxx>

using std::vector;

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
class pqDSMViewerPanel::pqInternals : public QObject, public Ui::DSMViewerPanel 
{
public:
  pqInternals(pqDSMViewerPanel* p) : QObject(p)
  {
    this->DSMInitialized     = false;
    this->DSMListening       = false;
    this->ActiveSourcePort   = 0;
    this->ActiveServer       = NULL;
    this->ActiveView         = NULL;
    this->pqObjectInspector  = NULL;
    this->pqDSMProxyHelper   = NULL;
    this->pqWidget3D         = NULL;
    this->SteeringParser     = NULL;
    this->CurrentTimeStep    = 0;
    this->DSMCommType        = 0;
  }
  //
  ~pqInternals() {
    if (this->DSMProxyCreated()) {
      this->DSMProxy->InvokeCommand("DestroyDSM");
    }
    this->DSMProxy           = NULL;
    this->DSMProxyHelper     = NULL;
    this->ActiveSourceProxy  = NULL;
    this->XdmfReader         = NULL;
    this->XdmfRepresentation = NULL;
    this->Widget3DProxy      = NULL;
    if (this->SteeringParser) {
      delete this->SteeringParser;
    }
  }
  //
  void CreateDSMProxy() {
    vtkSMProxyManager *pm = vtkSMProxy::GetProxyManager();
    this->DSMProxy = pm->NewProxy("icarus_helpers", "DSMManager");
    this->DSMProxy->UpdatePropertyInformation();
  }
  // 
  void CreateDSMHelperProxy() {
    if (!this->DSMProxyCreated()) {
      this->CreateDSMProxy();
    }
    //
    vtkSMProxyManager *pm = vtkSMProxy::GetProxyManager();
    this->DSMProxyHelper = pm->NewProxy("icarus_helpers", "DSMProxyHelper");
    //
    // Set the DSM manager it uses for communication, (warning: updates all properties)
    pqSMAdaptor::setProxyProperty(this->DSMProxyHelper->GetProperty("DSMManager"), this->DSMProxy);
    this->DSMProxyHelper->UpdateVTKObjects();
    //
    // wrap the DSMProxyHelper object in a pqProxy so that we can use it in our object inspector
    if (this->pqDSMProxyHelper) {
      delete this->pqDSMProxyHelper;
    }
    this->pqDSMProxyHelper = new pqProxy("icarus_helpers", "DSMProxyHelper", this->DSMProxyHelper, this->ActiveServer); 
  }
  //
  bool DSMProxyCreated() { return this->DSMProxy!=NULL; }
  bool HelperProxyCreated() { return this->DSMProxyHelper!=NULL; }

  //
  // Internal variables maintained by the panel
  //
  int                                       DSMInitialized;
  vtkSmartPointer<vtkSMProxy>               DSMProxy;
  vtkSmartPointer<vtkSMProxy>               DSMProxyHelper;
  vtkSmartPointer<vtkSMProxy>               ActiveSourceProxy;
  vtkSmartPointer<vtkSMSourceProxy>         XdmfReader;
  vtkSmartPointer<vtkSMRepresentationProxy> XdmfRepresentation;
  vtkSmartPointer<vtkSMProxy>               Widget3DProxy;
  pq3DWidget                               *pqWidget3D;
  pqObjectInspectorWidget                  *pqObjectInspector;
  pqProxy                                  *pqDSMProxyHelper;
  int                                       ActiveSourcePort;
  pqServer                                 *ActiveServer;
  pqRenderView                             *ActiveView;
  XdmfSteeringParser                       *SteeringParser;
  //
  bool                                      DSMListening;
  int              DSMCommType;
  int                                       CurrentTimeStep;

};
//-----------------------------------------------------------------------------
class dsmStandardWidgets : public pq3DWidgetInterface
{
public:
  pq3DWidget* newWidget(const QString& name,
    vtkSMProxy* referenceProxy,
    vtkSMProxy* controlledProxy)
    {
    pq3DWidget *widget = 0;
    if (name == "Plane")
      {
      widget = new pqImplicitPlaneWidget(referenceProxy, controlledProxy, 0);
      }
    else if (name == "Box")
      {
      widget = new pqBoxWidget(referenceProxy, controlledProxy, 0);
      }
    else if (name == "Handle")
      {
      widget = new pqHandleWidget(referenceProxy, controlledProxy, 0);
      }
    else if (name == "PointSource")
      {
      widget = new pqPointSourceWidget(referenceProxy, controlledProxy, 0);
      }
    else if (name == "LineSource")
      {
      widget = new pqLineSourceWidget(referenceProxy, controlledProxy, 0);
      }
    else if (name == "Line")
      {
      widget = new pqLineWidget(referenceProxy, controlledProxy, 0);
      }
    else if (name == "Distance")
      {
      widget = new pqDistanceWidget(referenceProxy, controlledProxy, 0);
      }
    else if (name == "Sphere")
      {
      widget = new pqSphereWidget(referenceProxy, controlledProxy, 0);
      }
    else if (name == "Spline")
      {
      widget = new pqSplineWidget(referenceProxy, controlledProxy, 0);
      }
    return widget;
    }
};
//-----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
pqDSMViewerPanel::pqDSMViewerPanel(QWidget* p) :
QDockWidget("DSM Manager", p)
{
  this->Internals = new pqInternals(this);
  this->Internals->setupUi(this);
  //
  this->UpdateTimer = new QTimer(this);
  this->UpdateTimer->setInterval(10);
  connect(this->UpdateTimer, SIGNAL(timeout()), this, SLOT(onUpdateTimeout()));
  this->UpdateTimer->start();

  //
  // Link GUI object events to callbacks
  //

  // XDMF XML Commands
  this->connect(this->Internals->browseFile,
    SIGNAL(clicked()), this, SLOT(onBrowseFile()));

  // Auto Save image
  this->connect(this->Internals->browseFileImage,
    SIGNAL(clicked()), this, SLOT(onBrowseFileImage()));

  this->connect(this->Internals->autoSaveImage,
    SIGNAL(stateChanged(int)), this, SLOT(onautoSaveImageChecked(int)));

  // 3D widget
//  this->connect(this->Internals->displayWidget,
//    SIGNAL(stateChanged(int)), this, SLOT(toggleHandleWidget(int)));
  
  // DSM Commands
  this->connect(this->Internals->addServerDSM,
      SIGNAL(clicked()), this, SLOT(onAddServerDSM()));

  this->connect(this->Internals->publishDSM,
    SIGNAL(clicked()), this, SLOT(onPublishDSM()));

  this->connect(this->Internals->unpublishDSM,
    SIGNAL(clicked()), this, SLOT(onUnpublishDSM()));

  // Steering commands
  this->connect(this->Internals->scRestart,
      SIGNAL(clicked()), this, SLOT(onSCRestart()));

  this->connect(this->Internals->scPause,
      SIGNAL(clicked()), this, SLOT(onSCPause()));

  this->connect(this->Internals->scPlay,
      SIGNAL(clicked()), this, SLOT(onSCPlay()));

  this->connect(this->Internals->dsmWriteDisk,
      SIGNAL(clicked()), this, SLOT(onSCWriteDisk()));

  this->connect(this->Internals->dsmArrayTreeWidget,
      SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(onArrayItemChanged(QTreeWidgetItem*, int)));

  this->connect(this->Internals->writeToDSM,
      SIGNAL(clicked()), this, SLOT(onWriteDataToDSM()));

//  this->connect(this->Internals->advancedControlUpdate,
//        SIGNAL(clicked()), this, SLOT(onAdvancedControlUpdate()));
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

  this->DeleteSteeringWidgets();

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
      if (this->Internals->dsmServerName->findText(server) < 0) {
        this->Internals->dsmServerName->addItem(server);
      }
    }
  }
  settings->endArray();
  // Active server
  this->Internals->dsmServerName->setCurrentIndex(settings->value("Selected", 0).toInt());
  // Size
  this->Internals->dsmSizeSpinBox->setValue(settings->value("Size", 0).toInt());
  // Method
  this->Internals->xdmfCommTypeComboBox->setCurrentIndex(settings->value("Communication", 0).toInt());
  // Port
  this->Internals->xdmfCommPort->setValue(settings->value("Port", 0).toInt());
  // Client/Server/Standalone
  this->Internals->dsmIsServer->setChecked(settings->value("dsmServer", 0).toBool());
  this->Internals->dsmIsClient->setChecked(settings->value("dsmClient", 0).toBool());
  this->Internals->dsmIsStandalone->setChecked(settings->value("dsmStandalone", 0).toBool());
  // Description file type
  this->Internals->xdmfFileTypeComboBox->setCurrentIndex(settings->value("DescriptionFileType", 0).toInt());
  // Description file path
  QString descFilePath = settings->value("DescriptionFilePath").toString();
  if(!descFilePath.isEmpty()) {
    this->Internals->xdmfFilePathLineEdit->insert(descFilePath);
    this->ParseXMLTemplate(descFilePath.toStdString().c_str());
  }
  // Force XDMF Generation
  this->Internals->forceXdmfGeneration->setChecked(settings->value("ForceXDMFGeneration", 0).toBool());
  // Image Save
  this->Internals->autoSaveImage->setChecked(settings->value("AutoSaveImage", 0).toBool());
  QString imageFilePath = settings->value("ImageFilePath").toString();
  if(!imageFilePath.isEmpty()) {
    this->Internals->imageFilePath->insert(imageFilePath);
  }
  settings->endGroup();
}
//----------------------------------------------------------------------------
void pqDSMViewerPanel::SaveSettings()
{
  pqSettings *settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("DSMManager");
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
  settings->setValue("Size", this->Internals->dsmSizeSpinBox->value());
  // Method
  settings->setValue("Communication", this->Internals->xdmfCommTypeComboBox->currentIndex());
  // Port
  settings->setValue("Port", this->Internals->xdmfCommPort->value());
  // Client/Server/Standalone
  settings->setValue("dsmServer", this->Internals->dsmIsServer->isChecked());
  settings->setValue("dsmClient", this->Internals->dsmIsClient->isChecked());
  settings->setValue("dsmStandalone", this->Internals->dsmIsStandalone->isChecked());
  // Description file type
  settings->setValue("DescriptionFileType", this->Internals->xdmfFileTypeComboBox->currentIndex());
  // Description file path
  settings->setValue("DescriptionFilePath", this->Internals->xdmfFilePathLineEdit->text());
  // Force XDMF Generation
  settings->setValue("ForceXDMFGeneration", this->Internals->forceXdmfGeneration->isChecked());
  // Image Save
  settings->setValue("AutoSaveImage", this->Internals->autoSaveImage->isChecked());
  settings->setValue("ImageFilePath", this->Internals->imageFilePath->text());
  //
  settings->endGroup();
}
//----------------------------------------------------------------------------
void pqDSMViewerPanel::DeleteSteeringWidgets()
{
  // Delete old tree
  for(int i = 0; i < this->Internals->dsmArrayTreeWidget->topLevelItemCount(); i++) {
    QTreeWidgetItem *gridItem;
    gridItem = this->Internals->dsmArrayTreeWidget->topLevelItem(i);
    for (int j = 0; j < gridItem->childCount(); j++) {
      QTreeWidgetItem *attributeItem = gridItem->child(j);
      gridItem->removeChild(attributeItem);
      if (attributeItem) delete attributeItem;
      attributeItem = NULL;
    }
    this->Internals->dsmArrayTreeWidget->removeItemWidget(gridItem, 0);
    if (gridItem) delete gridItem;
    gridItem = NULL;
  }

  // clear out auto generated controls
  delete this->Internals->pqObjectInspector;
  delete this->Internals->pqDSMProxyHelper;
  this->Internals->pqObjectInspector = NULL;
  this->Internals->pqDSMProxyHelper  = NULL;
}
//----------------------------------------------------------------------------
void pqDSMViewerPanel::ParseXMLTemplate(const char *filepath)
{
  //
  // Clean up anything from a previous creation
  //
  this->DeleteSteeringWidgets();
  //
  // Parse XML template and create proxies, 
  // We must parse XML first, otherwise the DSMHelperProxy is empty
  //
  if (this->Internals->SteeringParser) delete this->Internals->SteeringParser;
  this->Internals->SteeringParser = new XdmfSteeringParser();
  this->Internals->SteeringParser->Parse(filepath);
  this->Internals->CreateDSMHelperProxy();
  //
  // populate GUI with controls for grids
  //
  GridMap &steeringConfig = this->Internals->SteeringParser->GetSteeringConfig();
  //
  this->Internals->dsmArrayTreeWidget->setColumnCount(1);
  QList<QTreeWidgetItem *> gridItems;
  for (GridMap::iterator git=steeringConfig.begin(); git!=steeringConfig.end(); ++git) {
    QTreeWidgetItem *gridItem;
    QList<QTreeWidgetItem *> attributeItems;
    gridItem = new QTreeWidgetItem((QTreeWidget*)0, QStringList(QString(git->first.c_str())));
    // Do not make grids selectable
    gridItem->setCheckState(0, Qt::Checked);
    gridItems.append(gridItem);
    AttributeMap &amap = git->second.attributeConfig;
    for (AttributeMap::iterator ait=amap.begin(); ait!=amap.end(); ++ait) {
      QTreeWidgetItem *attributeItem = new QTreeWidgetItem(gridItem, QStringList(QString(ait->first.c_str())));
      attributeItem->setCheckState(0, Qt::Checked);
      attributeItem->setData(0, 1, QVariant(git->first.c_str()));
      gridItems.append(attributeItem);
    }
  }
  this->Internals->dsmArrayTreeWidget->clear();
  this->Internals->dsmArrayTreeWidget->insertTopLevelItems(0, gridItems);
  this->Internals->dsmArrayTreeWidget->expandAll();

  //
  // The active view is very important once we start generating proxies to control
  // widgets etc. Make sure it is valid.
  // 
  if (this->Internals->ActiveView==NULL && pqActiveView::instance().current()) {
    this->onActiveViewChanged(pqActiveView::instance().current());
  }
  //
  // create an object inspector to manage the settings
  //
  this->Internals->pqObjectInspector = new pqObjectInspectorWidget(this->Internals->devTab);
  this->Internals->pqObjectInspector->setView(this->Internals->ActiveView);
  this->Internals->pqObjectInspector->setProxy(this->Internals->pqDSMProxyHelper);
  this->Internals->pqObjectInspector->setDeleteButtonVisibility(false);
  this->Internals->pqObjectInspector->setHelpButtonVisibility(false);
  this->Internals->generatedLayout->addWidget(this->Internals->pqObjectInspector);
  //
  std::cout << "#######################################\n";
  vtkSMPropertyIterator *iter=this->Internals->DSMProxyHelper->NewPropertyIterator();
  while (!iter->IsAtEnd()) {
    vtkPVXMLElement *h = iter->GetProperty()->GetHints();
    if (h) {
      h->PrintXML(std::cout,vtkIndent(0));
      std::cout << "##########\n";
      for (unsigned int i=0; i<h->GetNumberOfNestedElements(); i++) {
        vtkPVXMLElement *n = h->GetNestedElement(i);
        n->PrintXML(std::cout,vtkIndent(2));
        std::cout << "#####\n";
      }
    }
    iter->Next();
  }
  iter->Delete();
  std::cout << "#######################################\n\n\n";

}
//----------------------------------------------------------------------------
void pqDSMViewerPanel::onServerAdded(pqServer *server)
{
  this->Internals->ActiveServer = server;
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onActiveServerChanged(pqServer* server)
{
  this->Internals->ActiveServer = server;
}
//----------------------------------------------------------------------------
void pqDSMViewerPanel::StartRemovingServer(pqServer *server)
{
  if (this->Internals->DSMProxyCreated()) {
    this->Internals->DSMProxy->InvokeCommand("DestroyDSM");
    this->Internals->DSMProxy = NULL;
    this->Internals->DSMInitialized = 0;
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onActiveViewChanged(pqView* view)
{
  pqRenderView* renView = qobject_cast<pqRenderView*>(view);
  this->Internals->ActiveView = renView;
  if (this->Internals->pqWidget3D && this->Internals->ActiveView) {
    this->Internals->pqWidget3D->setView(this->Internals->ActiveView);
  }
}
//---------------------------------------------------------------------------
bool pqDSMViewerPanel::DSMProxyReady()
{
  if (!this->Internals->DSMProxyCreated()) {
    this->Internals->CreateDSMProxy();
    return this->Internals->DSMProxyCreated();
  }
  return true;
}
//---------------------------------------------------------------------------
bool pqDSMViewerPanel::DSMReady()
{
  if (!this->DSMProxyReady()) return 0;
  //
  if (!this->Internals->DSMInitialized) {
    //
    bool server = (this->Internals->dsmIsServer->isChecked() || this->Internals->dsmIsStandalone->isChecked());
    bool client = (this->Internals->dsmIsClient->isChecked() || this->Internals->dsmIsStandalone->isChecked());
    pqSMAdaptor::setElementProperty(
      this->Internals->DSMProxy->GetProperty("DsmIsServer"), server);
    //
    if (server) {
      if (this->Internals->xdmfCommTypeComboBox->currentText() == QString("MPI")) {
        this->Internals->DSMCommType = H5FD_DSM_COMM_MPI;
      }
      else if (this->Internals->xdmfCommTypeComboBox->currentText() == QString("Sockets")) {
        this->Internals->DSMCommType = H5FD_DSM_COMM_SOCKET;
      }
      else if (this->Internals->xdmfCommTypeComboBox->currentText() == QString("MPI_RMA")) {
        this->Internals->DSMCommType = H5FD_DSM_COMM_MPI_RMA;
      }
      pqSMAdaptor::setElementProperty(
        this->Internals->DSMProxy->GetProperty("DsmCommType"),
        this->Internals->DSMCommType);
      //
      pqSMAdaptor::setElementProperty(
        this->Internals->DSMProxy->GetProperty("DsmLocalBufferSize"),
        this->Internals->dsmSizeSpinBox->value());
      //
      this->Internals->DSMProxy->UpdateVTKObjects();
      this->Internals->DSMProxy->InvokeCommand("CreateDSM");
      this->Internals->DSMInitialized = 1;
    }
    else if (client) {
      this->Internals->DSMProxy->InvokeCommand("ReadDSMConfigFile");
      this->Internals->DSMProxy->InvokeCommand("CreateDSM");
      this->Internals->DSMProxy->InvokeCommand("ConnectDSM");
      this->Internals->DSMInitialized = 1;
    }
    // 
    if (server && client) {
//      this->onPublishDSM();
//      this->Internals->DSMProxy->InvokeCommand("ConnectDSM");
    }
  }
  return this->Internals->DSMInitialized;
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onAddServerDSM()
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
void pqDSMViewerPanel::onBrowseFile()
{
  QList<QUrl> urls;
  urls << QUrl::fromLocalFile(QDesktopServices::storageLocation(QDesktopServices::HomeLocation));

  QFileDialog dialog;
  dialog.setSidebarUrls(urls);
  dialog.setViewMode(QFileDialog::Detail);
  dialog.setFileMode(QFileDialog::ExistingFile);
  if(dialog.exec()) {
    QString fileName = dialog.selectedFiles().at(0);
    this->Internals->xdmfFilePathLineEdit->clear();
    this->Internals->xdmfFilePathLineEdit->insert(fileName);
    this->ParseXMLTemplate(fileName.toStdString().c_str());
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onBrowseFileImage()
{
  QList<QUrl> urls;
  urls << QUrl::fromLocalFile(QDesktopServices::storageLocation(QDesktopServices::HomeLocation));

  QFileDialog dialog;
  dialog.setSidebarUrls(urls);
  dialog.setViewMode(QFileDialog::Detail);
  dialog.setFileMode(QFileDialog::AnyFile);
  if(dialog.exec()) {
    std::string fileName = dialog.selectedFiles().at(0).toStdString();
    this->Internals->imageFilePath->clear();
    fileName = vtksys::SystemTools::GetFilenamePath(fileName) + "/" +
      vtksys::SystemTools::GetFilenameWithoutLastExtension(fileName) + ".xxxxx.png";
    this->Internals->imageFilePath->insert(QString(fileName.c_str()));
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onPublishDSM()
{
  if (this->DSMReady() && !this->Internals->DSMListening) {
    if (this->Internals->DSMCommType == H5FD_DSM_COMM_SOCKET) {
      QString hostname = this->Internals->dsmServerName->currentText();
      pqSMAdaptor::setElementProperty(
          this->Internals->DSMProxy->GetProperty("ServerHostName"),
          hostname.toStdString().c_str());

      pqSMAdaptor::setElementProperty(
          this->Internals->DSMProxy->GetProperty("ServerPort"),
          this->Internals->xdmfCommPort->value());
    }
    this->Internals->DSMProxy->UpdateVTKObjects();
    this->Internals->DSMProxy->InvokeCommand("PublishDSM");
    this->Internals->DSMListening = true;
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onUnpublishDSM()
{
  if (this->DSMReady() && this->Internals->DSMListening) {
    this->Internals->DSMProxy->InvokeCommand("UnpublishDSM");
    this->Internals->DSMListening = false;
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onArrayItemChanged(QTreeWidgetItem *item, int)
{
  this->ChangeItemState(item);

  // Refresh list of send-able arrays
  // TODO Enable it when set up in the Manager
  GridMap &steeringConfig = this->Internals->SteeringParser->GetSteeringConfig();
  for (GridMap::iterator git = steeringConfig.begin(); git != steeringConfig.end(); git++) {
    for (AttributeMap::iterator ait = git->second.attributeConfig.begin();
        ait != git->second.attributeConfig.end();
        ait ++) {
//      if (ait->second.isEnabled) {
//        pqSMAdaptor::setElementProperty(
//            this->Internals->DSMProxy->GetProperty("SteerableObject"),
//            ait->second.hdfPath.c_str());
//        this->Internals->DSMProxy->UpdateVTKObjects();
//      }
//      if (ait->second.isEnabled)
//      cerr << ait->second.hdfPath.c_str() << endl;
    }
//    if (git->second.isEnabled) {
//      pqSMAdaptor::setElementProperty(
//          this->Internals->DSMProxy->GetProperty("SteerableObject"),
//          git->first.c_str());
//      this->Internals->DSMProxy->UpdateVTKObjects();
//    }
//    if (git->second.isEnabled)
//      cerr << git->first.c_str() << endl;
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::ChangeItemState(QTreeWidgetItem *item)
{
  if (!item)
    return;

  GridMap &steeringConfig = this->Internals->SteeringParser->GetSteeringConfig();
  std::string gridname = item->data(0, 1).toString().toStdString();
  std::string attrname = item->text(0).toStdString();

  if (!item->parent()) {
    steeringConfig[attrname].isEnabled = (item->checkState(0) == Qt::Checked);
  } else {
    steeringConfig[gridname].attributeConfig[attrname].isEnabled = (item->checkState(0) == Qt::Checked);
  }

  for (int i = 0; i < item->childCount(); i++) {
    QTreeWidgetItem *child = item->child(i);
    child->setCheckState(0, item->checkState(0));
    this->ChangeItemState(child);
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onSCPause()
{
  if (this->DSMReady()) {
    const char *steeringCmd = "pause";
    pqSMAdaptor::setElementProperty(
        this->Internals->DSMProxy->GetProperty("SteeringCommand"),
        steeringCmd);
    this->Internals->infoCurrentSteeringCommand->clear();
    this->Internals->infoCurrentSteeringCommand->insert(steeringCmd);
    this->Internals->DSMProxy->UpdateVTKObjects();
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onSCPlay()
{
  if (this->DSMReady()) {
    const char *steeringCmd = "play";
    pqSMAdaptor::setElementProperty(
        this->Internals->DSMProxy->GetProperty("SteeringCommand"),
        steeringCmd);
    this->Internals->infoCurrentSteeringCommand->clear();
    this->Internals->infoCurrentSteeringCommand->insert(steeringCmd);
    this->Internals->DSMProxy->UpdateVTKObjects();
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onSCRestart()
{
  if (this->DSMReady()) {
    const char *steeringCmd = "restart";
    pqSMAdaptor::setElementProperty(
        this->Internals->DSMProxy->GetProperty("SteeringCommand"),
        steeringCmd);
    this->Internals->infoCurrentSteeringCommand->clear();
    this->Internals->infoCurrentSteeringCommand->insert(steeringCmd);
    this->Internals->DSMProxy->UpdateVTKObjects();
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onSCWriteDisk()
{
  if (this->DSMReady()) {
    // workaround to be able to click multiple times on the disk button
    const char *steeringCmdNone = "none";
    const char *steeringCmd = "disk";

    pqSMAdaptor::setElementProperty(this->Internals->DSMProxy->GetProperty("SteeringCommand"),
        steeringCmdNone);
    this->Internals->DSMProxy->UpdateVTKObjects();

    pqSMAdaptor::setElementProperty(this->Internals->DSMProxy->GetProperty("SteeringCommand"),
        steeringCmd);
    this->Internals->infoCurrentSteeringCommand->clear();
    this->Internals->infoCurrentSteeringCommand->insert(steeringCmd);
    this->Internals->DSMProxy->UpdateVTKObjects();
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onWriteDataToDSM() 
{
  if (this->DSMReady()) {
    if (!this->Internals->ActiveSourceProxy) {
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
      XdmfWriter->GetProperty("DSMManager"), this->Internals->DSMProxy);

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
void pqDSMViewerPanel::SaveSnapshot() {
  std::string pngname = this->Internals->imageFilePath->text().toStdString();
  vtksys::SystemTools::ReplaceString(pngname, "xxxxx", "%05i");
  char buffer[1024];
  sprintf(buffer, pngname.c_str(), this->Internals->CurrentTimeStep);  
  pqActiveObjects::instance().activeView()->saveImage(0, 0, QString(buffer));
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onautoSaveImageChecked(int checked)
{
  if (checked) {
    SaveSnapshot();
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onDisplayDSM()
{
  bool force_generate     = false;
  static bool first_time  = true;
  static int current_time = 0;
  static std::string xdmf_description_file_path = this->Internals->xdmfFilePathLineEdit->text().toStdString();


  if (this->Internals->DSMProxyCreated() && this->Internals->DSMInitialized && this->DSMReady()) {
    vtkSMProxyManager *pm = vtkSMProxy::GetProxyManager();

#ifdef DISABLE_DISPLAY
    if (this->DSMReady()) {
      this->Internals->DSMProxy->InvokeCommand("H5DumpLight");
    }
#else
    if (!this->Internals->XdmfReader || this->Internals->storeDSMContents->isChecked()) {
      //
      // Create a new Reader proxy and register it with the system
      //
      char proxyName[256];

      // When creating reader, make sure first_time is true
      if (!first_time) first_time = true;
      if (this->Internals->XdmfReader) this->Internals->XdmfReader.New();
      this->Internals->XdmfReader.TakeReference(vtkSMSourceProxy::SafeDownCast(pm->NewProxy("icarus_helpers", "XdmfReader4")));
      this->Internals->XdmfReader->SetConnectionID(pqActiveObjects::instance().activeServer()->GetConnectionID());
      this->Internals->XdmfReader->SetServers(vtkProcessModule::DATA_SERVER);
      sprintf(proxyName, "DSMDataSource%d", current_time);
      pm->RegisterProxy("sources", proxyName, this->Internals->XdmfReader);

      //
      // Connect our DSM manager to the reader
      //
      pqSMAdaptor::setProxyProperty(
          this->Internals->XdmfReader->GetProperty("DSMManager"), this->Internals->DSMProxy
        );
    }

    //
    // If we are using an Xdmf XML file supplied manually or generated, get it
    //
    if (this->Internals->xdmfFileTypeComboBox->currentIndex() != 2) { //if not use sent XML
      if (!this->Internals->xdmfFilePathLineEdit->text().isEmpty()) {
        if (this->Internals->xdmfFileTypeComboBox->currentIndex() == 0) { // Original XDMF File
          pqSMAdaptor::setElementProperty(
              this->Internals->XdmfReader->GetProperty("FileName"),
              this->Internals->xdmfFilePathLineEdit->text().toStdString().c_str());
        }
        if (this->Internals->xdmfFileTypeComboBox->currentIndex() == 1) { // XDMF Template File
          force_generate = (this->Internals->forceXdmfGeneration->isChecked()) ? true : false;
          // Only re-generate if the description file path has changed or if force is set to true
          if ((xdmf_description_file_path != this->Internals->xdmfFilePathLineEdit->text().toStdString()) || first_time || force_generate) {
            xdmf_description_file_path = this->Internals->xdmfFilePathLineEdit->text().toStdString();
            // Generate xdmf file for reading
            // Send the whole string representing the DOM and not the file path so that the template file
            // does not to be present on the server anymore
            pqSMAdaptor::setElementProperty(
                this->Internals->DSMProxy->GetProperty("XMFDescriptionFilePath"),
                this->Internals->SteeringParser->GetConfigDOM()->Serialize());

            this->Internals->DSMProxy->UpdateVTKObjects();
            this->Internals->DSMProxy->InvokeCommand("GenerateXMFDescription");
          }
        }
      }
    }
    else {
      pqSMAdaptor::setElementProperty(
        this->Internals->XdmfReader->GetProperty("FileName"), "stdin");
    }

    QTime dieTime = QTime::currentTime().addMSecs(10);
    while( QTime::currentTime() < dieTime ) {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }

    //
    // Update before setting up representation to ensure correct 'type' is created
    // Remember that Xdmf supports many data types Regular/Unstructured/etc
    //
    this->Internals->XdmfReader->InvokeCommand("Modified");
    this->Internals->XdmfReader->UpdatePropertyInformation();
    this->Internals->XdmfReader->UpdateVTKObjects();
    this->Internals->XdmfReader->UpdatePipeline();

    //
    // Create a representation if we need one : First time or if storing multiple datasets
    //
    if (!this->Internals->XdmfRepresentation || this->Internals->storeDSMContents->isChecked()) {
      this->Internals->XdmfRepresentation = pqActiveObjects::instance().activeView()->getViewProxy()->CreateDefaultRepresentation(this->Internals->XdmfReader, 0);
    }

    //
    // Create a pipeline source to appear in the GUI, 
    // findItem creates a new one initially, thenafter returns the same object
    // Mark the item as Unmodified since we manually updated the pipeline ourselves
    //
    pqPipelineSource* source = pqApplicationCore::instance()->
      getServerManagerModel()->findItem<pqPipelineSource*>(this->Internals->XdmfReader);
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
      this->Internals->CurrentTimeStep = current_time;
    }

    dieTime = QTime::currentTime().addMSecs(10);
    while( QTime::currentTime() < dieTime )
	    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    //
    // Trigger a render : if changed, everything should update as usual
    if (pqActiveObjects::instance().activeView())
    {
      pqActiveObjects::instance().activeView()->render();
      if (this->Internals->autoSaveImage->isChecked()) {
        this->SaveSnapshot();
      }
    }
#endif //DISABLE_DISPLAY
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
void pqDSMViewerPanel::onUpdateTimeout()
{
  // make sure we only get one message at a time.
  this->UpdateTimer->stop();

  // we don't want to create anything just to inspect it, so test flags only
  if (this->Internals->DSMProxyCreated() && this->Internals->DSMInitialized) {
    if (this->DSMReady()) {
      vtkSMIntVectorProperty *ur = vtkSMIntVectorProperty::SafeDownCast(
        this->Internals->DSMProxy->GetProperty("DsmUpdateReady"));
      this->Internals->DSMProxy->UpdatePropertyInformation(ur);
      int ready = ur->GetElement(0);

      vtkSMIntVectorProperty *ud = vtkSMIntVectorProperty::SafeDownCast(
        this->Internals->DSMProxy->GetProperty("DsmUpdateDisplay"));
      this->Internals->DSMProxy->UpdatePropertyInformation(ud);
      int updateDisplay = ud->GetElement(0);

      if (ready != 0) {
        this->Internals->DSMProxy->InvokeCommand("ClearDsmUpdateReady");
        if (this->Internals->autoDisplayDSM->isChecked() && updateDisplay) {
          this->onDisplayDSM();
          this->Internals->DSMProxy->InvokeCommand("ClearDsmUpdateDisplay");
        }
        // TODO If the XdmfWriter has to write something back to the DSM, it's here
        if (!this->Internals->dsmIsStandalone->isChecked()) {
          this->Internals->DSMProxy->InvokeCommand("RequestRemoteChannel");
        }
      }
    }
  }

  // restart the timer before we exit
  this->UpdateTimer->start();
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::toggleHandleWidget(int state)
{
  if (this->Internals->ActiveView==NULL && pqActiveView::instance().current()) {
    this->onActiveViewChanged(pqActiveView::instance().current());
  }

  if (!this->Internals->XdmfReader) {
    return;
  }

  if (!this->Internals->Widget3DProxy) {
    vtkSMProxyManager *pm = vtkSMProxy::GetProxyManager();


//    this->Internals->Widget3DProxy = pm->NewProxy("extended_sources", "Transform3");
    this->Internals->Widget3DProxy = pm->NewProxy("3d_widgets", "HandleWidget");
    this->Internals->Widget3DProxy->SetConnectionID(pqActiveObjects::instance().activeServer()->GetConnectionID());
    this->Internals->Widget3DProxy->UpdatePropertyInformation();

    dsmStandardWidgets standardWidgets;
    this->Internals->pqWidget3D = standardWidgets.newWidget("Distance", this->Internals->XdmfReader, this->Internals->Widget3DProxy);

    this->Internals->pqWidget3D->resetBounds();
    this->Internals->pqWidget3D->reset();

    QGridLayout* l = qobject_cast<QGridLayout*>(this->Internals->devTab->layout());
    l->addWidget(this->Internals->pqWidget3D, 1, 0, 1, 2);
    if (this->Internals->ActiveView==NULL && pqActiveView::instance().current()) {
      this->onActiveViewChanged(pqActiveView::instance().current());
    }
    this->Internals->pqWidget3D->setView(this->Internals->ActiveView);
    this->Internals->pqWidget3D->show();
  }

  this->Internals->pqWidget3D->select();
  this->Internals->pqWidget3D->showWidget();
}
//-----------------------------------------------------------------------------
