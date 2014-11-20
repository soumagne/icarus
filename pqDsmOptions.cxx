/*=========================================================================

  Project                 : Icarus
  Module                  : pqDsmOptions.cxx

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
#include "pqDsmOptions.h"

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
#include "ui_pqDsmOptions.h"
//
#include "pqDsmObjectInspector.h"
#include "vtkAbstractDsmManager.h"
#include "XdmfSteeringParser.h"
#include "vtkCustomPipelineHelper.h"
//
#include <vtksys/SystemTools.hxx>
//
#ifdef ICARUS_HAVE_H5FDDSM
 #include "pqH5FDdsmPanel.h"
#endif
#ifdef ICARUS_HAVE_BONSAI
 #include "pqBonsaiDsmPanel.h"
#endif

//----------------------------------------------------------------------------
#define XML_USE_TEMPLATE 1
#define XML_USE_ORIGINAL 0
#define XML_USE_SENT     2
//----------------------------------------------------------------------------
//
typedef vtkSmartPointer<vtkCustomPipelineHelper> CustomPipeline;
typedef std::vector< CustomPipeline > PipelineList;
//----------------------------------------------------------------------------
class pqDsmOptions::pqInternals : public QObject, public Ui::DsmOptions
{
public:
  pqInternals(pqDsmOptions* p) : QObject(p)
  {
    this->PauseRequested      = false;
    this->ActiveView          = NULL;
    this->pqObjectInspector   = NULL;
    this->pqDsmProxyPipeline  = NULL;
    this->SteeringParser      = NULL;
    this->CreatePipelines     = true;
    this->PipelineTimeRange[0]= 0.0;
    this->PipelineTimeRange[1]= 1.0;
    this->PipelineTime        = 0.0;
    this->CurrentTimeStep     = 0;
#ifdef ICARUS_HAVE_H5FDDSM
    this->pqH5FDdsmWidget     = NULL;
#endif
#ifdef ICARUS_HAVE_BONSAI
    this->pqBonsaiDsmWidget   = NULL;
#endif
  };

  //
  virtual ~pqInternals() {
    this->DsmProxyHelper     = NULL;
    this->XdmfReader         = NULL;
    this->H5PartReader       = NULL;
    this->NetCDFReader       = NULL;
    this->TableReader        = NULL;
    this->BonsaiReader       = NULL;

    if (this->SteeringParser) {
      delete this->SteeringParser;
    }
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
#ifdef ICARUS_HAVE_H5FDDSM
    this->DsmProxy = this->pqH5FDdsmWidget->CreateDsmProxy();
#elif ICARUS_HAVE_BONSAI
    this->DsmProxy = this->pqBonsaiDsmWidget->CreateDsmProxy();
#else
//  #error At least one DSM type must be specified through cmake options / compiler #defines
#endif
  }
  // 
  void CreateDsmHelperProxy() {
    if (!this->DsmProxyCreated()) {
#ifdef ICARUS_HAVE_H5FDDSM
    this->DsmProxy = this->pqH5FDdsmWidget->CreateDsmProxy();
      this->CreateDsmProxy();
    }
    this->DsmProxyHelper = this->pqH5FDdsmWidget->CreateDsmHelperProxy();
#elif ICARUS_HAVE_BONSAI
    this->DsmProxy = this->pqBonsaiDsmWidget->CreateDsmProxy();
      this->CreateDsmProxy();
    }
    this->DsmProxyHelper = this->pqBonsaiDsmWidget->CreateDsmHelperProxy();
#else
    }
//    #error At least one DSM type must be specified through cmake options / compiler #defines
#endif
    vtkSMProxyManager *pm = vtkSMProxyManager::GetProxyManager();
    // wrap the DsmProxyHelper object in a pqPipelineSource so that we can use it in our object inspector
    pm->RegisterProxy("layouts", "DsmProxyHelper", this->DsmProxyHelper);
    // this->DsmProxyHelper->FastDelete();
    this->pqDsmProxyPipeline = new pqPipelineSource("DSMProxyHelper", this->DsmProxyHelper, this->getActiveServer(), 0);

    // Set the DSM manager it uses for communication, (warning: updates all properties)
    pqSMAdaptor::setProxyProperty(this->DsmProxyHelper->GetProperty("DsmManager"), this->DsmProxy);

#ifdef USE_STEERINGWRITER
    // We will also be needing a vtkSteeringWriter, so create that now
    this->SteeringWriter.TakeReference(vtkSMSourceProxy::SafeDownCast(pm->NewProxy("icarus_helpers", "SteeringWriter")));
    pqSMAdaptor::setProxyProperty(this->SteeringWriter->GetProperty("DsmManager"), this->DsmProxy);
    pqSMAdaptor::setElementProperty(this->SteeringWriter->GetProperty("GroupPath"), "/PartType1");
    this->SteeringWriter->UpdateVTKObjects();

    // Set the DSM manager it uses for communication, (warning: updates all properties)
    pqSMAdaptor::setProxyProperty(this->DsmProxyHelper->GetProperty("SteeringWriter"), this->SteeringWriter);
#endif

    this->DsmProxyHelper->UpdateVTKObjects();

    //
    // Add DSM controls to animation view
    //
    QWidget *mainWindow = pqCoreUtilities::mainWidget();
    pqAnimationViewWidget *pqaw = mainWindow->findChild<pqAnimationViewWidget*>("animationView");
    pqaw->addCustomProxy("DSM", this->DsmProxyHelper);
  }
  //
  bool DsmProxyCreated() { return this->DsmProxy!=NULL; }
  bool HelperProxyCreated() { return this->DsmProxyHelper!=NULL; }

  // ---------------------------------------------------------------
  // Variables for DSM management
  // ---------------------------------------------------------------
  vtkSmartPointer<vtkSMProxy>               DsmProxy;
  vtkSmartPointer<vtkSMProxy>               DsmProxyHelper;
  bool                                      PauseRequested;
  // ---------------------------------------------------------------
  // Principal pipeline of Xdmf[->ExtractBlock]
  // ---------------------------------------------------------------
  vtkSmartPointer<vtkCustomPipelineHelper>  XdmfViewer;
  vtkSmartPointer<vtkSMSourceProxy>         XdmfReader;
  vtkSmartPointer<vtkSMSourceProxy>         H5PartReader;
  vtkSmartPointer<vtkSMSourceProxy>         NetCDFReader;
  vtkSmartPointer<vtkSMSourceProxy>         TableReader;
  vtkSmartPointer<vtkSMSourceProxy>         BonsaiReader;
  bool                                      CreatePipelines;
  int                                       CurrentTimeStep; // 0,1,2,3...
  double                                    PipelineTimeRange[2]; // declared at startup usually
  double                                    PipelineTime;         // should be between Timerange[0,1]

  // ---------------------------------------------------------------
  // Display of controls via object inspector panel
  // ---------------------------------------------------------------
  XdmfSteeringParser                       *SteeringParser;
  pqDsmObjectInspector                     *pqObjectInspector;
  pqPipelineSource                         *pqDsmProxyPipeline;
  // ---------------------------------------------------------------
  // Pipelines for steerable objects, one per 3D widget
  // ---------------------------------------------------------------
  PipelineList                              WidgetPipelines;
  // ---------------------------------------------------------------
  // General management 
  // ---------------------------------------------------------------
  pqRenderView                             *ActiveView;
  // ---------------------------------------------------------------
  // DataExport commands
  // ---------------------------------------------------------------
  vtkTimeStamp                              LastExportTime;
  //
  //
#ifdef ICARUS_HAVE_H5FDDSM
  pqH5FDdsmPanel                          *pqH5FDdsmWidget;
#endif
#ifdef ICARUS_HAVE_BONSAI
  pqBonsaiDsmPanel                        *pqBonsaiDsmWidget;
#endif
};

//----------------------------------------------------------------------------
pqDsmOptions::pqDsmOptions(QWidget* p) :
QDockWidget("DSM Manager", p)
{
  this->Internals = new pqInternals(this);
  this->Internals->setupUi(this);

#ifdef ICARUS_HAVE_H5FDDSM
    this->Internals->pqH5FDdsmWidget = new pqH5FDdsmPanel(this);
    this->Internals->dsmLayout->addWidget(this->Internals->pqH5FDdsmWidget); 
#endif
#ifdef ICARUS_HAVE_BONSAI
    this->Internals->pqBonsaiDsmWidget = new pqBonsaiDsmPanel(this);
    this->Internals->dsmLayout->addWidget(this->Internals->pqBonsaiDsmWidget);
#endif

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
  
  this->connect(this->Internals->publish,
    SIGNAL(clicked()), this, SLOT(onPublish()));

  this->connect(this->Internals->unpublish,
    SIGNAL(clicked()), this, SLOT(onUnpublish()));

  // Steering commands
  this->connect(this->Internals->pause,
      SIGNAL(clicked()), this, SLOT(onPause()));

  this->connect(this->Internals->play,
      SIGNAL(clicked()), this, SLOT(onPlay()));

  this->connect(this->Internals->dsmArrayTreeWidget,
      SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(onArrayItemChanged(QTreeWidgetItem*, int)));

#ifdef ICARUS_HAVE_H5FDDSM
  this->connect(this->Internals->publish,
    SIGNAL(clicked()), this->Internals->pqH5FDdsmWidget, SLOT(onPublish()));
  this->connect(this->Internals->pqH5FDdsmWidget, SIGNAL(UpdateData()), 
    this, SLOT(UpdateDsmPipeline()));
  this->connect(this->Internals->pqH5FDdsmWidget, SIGNAL(UpdateInformation()), 
    this, SLOT(UpdateDsmInformation()));
  this->connect(this->Internals->pqH5FDdsmWidget, SIGNAL(UpdateStatus(QString)), 
    this, SLOT(UpdateDsmStatus(QString)));
#endif
#ifdef ICARUS_HAVE_BONSAI
  this->connect(this->Internals->publish,
    SIGNAL(clicked()), this->Internals->pqBonsaiDsmWidget, SLOT(onPublish()));
  this->connect(this->Internals->pqBonsaiDsmWidget, SIGNAL(UpdateData()),
    this, SLOT(UpdateDsmPipeline()));
  this->connect(this->Internals->pqBonsaiDsmWidget, SIGNAL(UpdateInformation()),
    this, SLOT(UpdateDsmInformation()));
  this->connect(this->Internals->pqBonsaiDsmWidget, SIGNAL(UpdateStatus(QString)),
    this, SLOT(UpdateDsmStatus(QString)));
#endif

  //
  // Link paraview events to callbacks
  //
  pqServerManagerModel* smModel =
    pqApplicationCore::instance()->getServerManagerModel();

  this->connect(smModel, SIGNAL(aboutToRemoveServer(pqServer *)),
    this, SLOT(onStartRemovingServer(pqServer *)));

  this->connect(smModel, SIGNAL(serverAdded(pqServer *)),
    this, SLOT(onServerAdded(pqServer *)));

  //
  // Track the active view so we can display contents in it
  //
  QObject::connect(&pqActiveView::instance(),
    SIGNAL(changed(pqView*)),
    this, SLOT(onActiveViewChanged(pqView*)));

  //
  this->LoadSettings();
}

//----------------------------------------------------------------------------
void pqDsmOptions::onQuickSync(bool b)
{
  std::cout << "On QuickSync " << std::endl;
  if (!this->Internals->DsmProxyCreated()) {
    return;
  }
  pqSMAdaptor::setElementProperty(
    this->Internals->DsmProxy->GetProperty("QuickSync"), b);
}

//----------------------------------------------------------------------------
pqDsmOptions::~pqDsmOptions()
{
  this->SaveSettings();
  this->DeleteSteeringWidgets();
}

//----------------------------------------------------------------------------
void pqDsmOptions::LoadSettings()
{
#ifdef ICARUS_HAVE_H5FDDSM
  this->Internals->pqH5FDdsmWidget->LoadSettings();
#endif
#ifdef ICARUS_HAVE_BONSAI
  this->Internals->pqBonsaiDsmWidget->LoadSettings();
#endif

  pqSettings *settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("DsmManager");

  // Description file type
  this->Internals->xdmfFileTypeComboBox->setCurrentIndex(settings->value("DescriptionFileType", 0).toInt());
  // Description file path
  QString descFilePath = settings->value("DescriptionFilePath").toString();
  if(!descFilePath.isEmpty()) {
    this->Internals->xdmfFilePathLineEdit->insert(descFilePath);
    this->ParseXMLTemplate(descFilePath.toLatin1().data());
  }
  // Force XDMF Generation
  this->Internals->forceXdmfGeneration->setChecked(settings->value("ForceXDMFGeneration", 0).toBool());
  // Time Information
  this->Internals->useTimeInfo->setChecked(settings->value("TimeInformation", 0).toBool());
  // Image Save
  this->Internals->autoSaveImage->setChecked(settings->value("AutoSaveImage", 0).toBool());
  QString imageFilePath = settings->value("ImageFilePath").toString();
  if(!imageFilePath.isEmpty()) {
    this->Internals->imageFilePath->insert(imageFilePath);
  }
  settings->endGroup();
}

//----------------------------------------------------------------------------
void pqDsmOptions::SaveSettings()
{
  pqSettings *settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("DsmManager");
  // servers
#ifdef ICARUS_HAVE_H5FDDSM
  this->Internals->pqH5FDdsmWidget->SaveSettings();
#endif
#ifdef ICARUS_HAVE_BONSAI
  this->Internals->pqBonsaiDsmWidget->SaveSettings();
#endif
  // Description file type
  settings->setValue("DescriptionFileType", this->Internals->xdmfFileTypeComboBox->currentIndex());
  // Description file path
  settings->setValue("DescriptionFilePath", this->Internals->xdmfFilePathLineEdit->text());
  // Force XDMF Generation
  settings->setValue("ForceXDMFGeneration", this->Internals->forceXdmfGeneration->isChecked());
  // Time Information
  settings->setValue("TimeInformation", this->Internals->useTimeInfo->isChecked());
  // Image Save
  settings->setValue("AutoSaveImage", this->Internals->autoSaveImage->isChecked());
  settings->setValue("ImageFilePath", this->Internals->imageFilePath->text());
  //
  settings->endGroup();
}

//----------------------------------------------------------------------------
void pqDsmOptions::DeleteSteeringWidgets()
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
  delete this->Internals->pqDsmProxyPipeline;
  this->Internals->pqObjectInspector  = NULL;
  this->Internals->pqDsmProxyPipeline = NULL;
}

//----------------------------------------------------------------------------
void pqDsmOptions::ParseXMLTemplate(const char *filepath)
{
  //
  // Clean up anything from a previous creation
  //
  this->DeleteSteeringWidgets();

  //
  // Parse XML template and create proxies, 
  // We must parse XML first, otherwise the DsmHelperProxy is empty
  //
  if (this->Internals->SteeringParser) delete this->Internals->SteeringParser;
  std::cout << "Setting Steering Parser " <<std::endl;

  this->Internals->SteeringParser = new XdmfSteeringParser();
  this->Internals->SteeringParser->Parse(filepath);

  if (this->Internals->DsmProxyCreated()) {
    // Get the XML for our Helper proxy and send it to the DSM manager
    // it will register the XML with the proxy manager on the server
    std::string HelperProxyXML = this->Internals->SteeringParser->GetHelperProxyString();
    if (HelperProxyXML.size()>0) {
      // register proxy on the server
      pqSMAdaptor::setElementProperty(
        this->Internals->DsmProxy->GetProperty("HelperProxyXMLString"), HelperProxyXML.c_str());
      this->Internals->DsmProxy->UpdateProperty("HelperProxyXMLString");

      // and register on the client too 
      vtkAbstractDsmManager::RegisterHelperProxy(HelperProxyXML.c_str());
      // now create an actual proxy
      this->Internals->CreateDsmHelperProxy();
    }
  }

  //
  // populate GUI with controls for grids
  //
  GridMap &steeringConfig = this->Internals->SteeringParser->GetSteeringConfig();
  //
  this->Internals->dsmArrayTreeWidget->setColumnCount(1);
  QList<QTreeWidgetItem *> gridItems;
  for (GridMap::iterator git=steeringConfig.begin(); git!=steeringConfig.end(); ++git) {
    QTreeWidgetItem *gridItem;
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
  if (this->Internals->pqDsmProxyPipeline) {
    this->Internals->pqObjectInspector = new pqDsmObjectInspector(this->Internals->steeringTab);
    this->Internals->pqObjectInspector->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::MinimumExpanding);
    this->Internals->pqObjectInspector->updatePropertiesPanel(this->Internals->pqDsmProxyPipeline);
    this->Internals->generatedLayout->addWidget(this->Internals->pqObjectInspector); 

    //// before changes are accepted
    this->connect(this->Internals->pqObjectInspector,
      SIGNAL(preapplied()), this, SLOT(onPreAccept()));
    // after changes are accepted
    this->connect(this->Internals->pqObjectInspector,
      SIGNAL(applied()), this, SLOT(onPostAccept()));

    //
    // Get info about pqWidgets and SMProxy object that are bound to steering controls
    //
    // debug : this->Internals->pqObjectInspector->dumpObjectTree();
    SteeringGUIWidgetMap &SteeringWidgetMap = this->Internals->SteeringParser->GetSteeringWidgetMap();
    // Turn off visibility for all 3D widgets initially
    QList<pq3DWidget*> widgets = this->Internals->pqObjectInspector->findChildren<pq3DWidget*>();
    for (int i=0; i<widgets.size(); i++) {
      pq3DWidget* widget = widgets[i];
      widget->hideWidget();
      vtkSMProxy* controlledProxy = widget->getControlledProxy();
      vtkSMProxy* referenceProxy = widget->getReferenceProxy();
      for (SteeringGUIWidgetMap::iterator it=SteeringWidgetMap.begin(); it!=SteeringWidgetMap.end(); ++it) 
      {
        const char *propertyname = it->first.c_str();
        if (controlledProxy->GetProperty(propertyname)) {
          it->second.ControlledProxy = controlledProxy;
          it->second.ReferenceProxy  = referenceProxy;
          it->second.pqWidget        = widget;
          it->second.WidgetProxy     = widget->getWidgetProxy();
          it->second.AbstractWidget  = widget->getWidgetProxy()->GetWidget();
        }
      }
    }
    //
    // Once everything is setup, allow traffic to and from the DSM
    //
    //  std::cout << "Sending an UnblockTraffic Command " << std::endl;
    this->Internals->DsmProxyHelper->InvokeCommand("UnblockTraffic");
    //
    this->Internals->LastExportTime.Modified();
  }
}

//----------------------------------------------------------------------------
void pqDsmOptions::onServerAdded(pqServer *server)
{
}

//----------------------------------------------------------------------------
void pqDsmOptions::onStartRemovingServer(pqServer *server)
{
  if (this->Internals->DsmProxyCreated()) {
    this->Internals->DsmProxy->InvokeCommand("Destroy");
    this->Internals->DsmProxy = NULL;
  }
  delete this->Internals->pqObjectInspector;
  delete this->Internals->pqDsmProxyPipeline;
  this->Internals->pqObjectInspector = NULL;
  this->Internals->pqDsmProxyPipeline  = NULL;
}

//-----------------------------------------------------------------------------
void pqDsmOptions::onActiveViewChanged(pqView* view)
{
  pqRenderView* renView = qobject_cast<pqRenderView*>(view);
  this->Internals->ActiveView = renView;
}

//---------------------------------------------------------------------------
bool pqDsmOptions::DsmProxyReady()
{
  if (!this->Internals->DsmProxyCreated()) {
    this->Internals->CreateDsmProxy();
    return this->Internals->DsmProxyCreated();
  }
  return true;
}

//---------------------------------------------------------------------------
bool pqDsmOptions::DsmReady()
{
  if (!this->DsmProxyReady()) return 0;
  //
  bool initialized = false;
  icarus_std::function<void()> func = icarus_std::bind(&pqDsmOptions::DsmInitFunction, this);
#ifdef ICARUS_HAVE_H5FDDSM
  this->Internals->pqH5FDdsmWidget->setInitializationFunction(func);
  initialized = this->Internals->pqH5FDdsmWidget->DsmReady();
#endif  
#ifdef ICARUS_HAVE_BONSAI
  this->Internals->pqBonsaiDsmWidget->setInitializationFunction(func);
  initialized = this->Internals->pqBonsaiDsmWidget->DsmReady();
#endif
   return initialized;
}

//---------------------------------------------------------------------------
void pqDsmOptions::DsmInitFunction()
{
  //
  // Create GUI controls from template
  //
  if (!this->Internals->xdmfFilePathLineEdit->text().isEmpty()) {
    this->ParseXMLTemplate(this->Internals->xdmfFilePathLineEdit->text().toLatin1().data());
  }
}

//---------------------------------------------------------------------------
bool pqDsmOptions::DsmListening()
{
#ifdef ICARUS_HAVE_H5FDDSM
  return this->Internals->pqH5FDdsmWidget->DsmListening();
#endif  
#ifdef ICARUS_HAVE_BONSAI
  return true;
#endif
  return false;
}

//-----------------------------------------------------------------------------
void pqDsmOptions::onLockSettings(int state)
{
  bool locked = (state == Qt::Checked) ? true : false;
  this->Internals->descriptionFileSettingBox->setEnabled(!locked);
  this->Internals->autoDisplayDSM->setEnabled(!locked);
  this->Internals->storeDsmContents->setEnabled(!locked);
  this->Internals->imageSaveBox->setEnabled(!locked);
  this->Internals->autoExport->setEnabled(!locked);
}

//-----------------------------------------------------------------------------
void pqDsmOptions::onAddDsmServer()
{
}

//-----------------------------------------------------------------------------
void pqDsmOptions::onBrowseFile()
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
    this->ParseXMLTemplate(fileName.toLatin1().data());
  }
}

//-----------------------------------------------------------------------------
void pqDsmOptions::onBrowseFileImage()
{
  QList<QUrl> urls;
  urls << QUrl::fromLocalFile(QDesktopServices::storageLocation(QDesktopServices::HomeLocation));

  QFileDialog dialog;
  dialog.setSidebarUrls(urls);
  dialog.setViewMode(QFileDialog::Detail);
  dialog.setFileMode(QFileDialog::AnyFile);
  if(dialog.exec()) {
    std::string fileName = dialog.selectedFiles().at(0).toLatin1().data();
    this->Internals->imageFilePath->clear();
    fileName = vtksys::SystemTools::GetFilenamePath(fileName) + "/" +
      vtksys::SystemTools::GetFilenameWithoutLastExtension(fileName) + ".xxxxx.png";
    this->Internals->imageFilePath->insert(QString(fileName.c_str()));
  }
}

//-----------------------------------------------------------------------------
void pqDsmOptions::onPublish()
{
  if (this->DsmReady()) {}
}

//-----------------------------------------------------------------------------
void pqDsmOptions::onUnpublish()
{
  if (this->DsmReady() && this->DsmListening()) {
    this->Internals->DsmProxy->InvokeCommand("Unpublish");
  }
}

//-----------------------------------------------------------------------------
void pqDsmOptions::onArrayItemChanged(QTreeWidgetItem *item, int)
{
  this->ChangeItemState(item);

  GridMap &steeringConfig = this->Internals->SteeringParser->GetSteeringConfig();
  std::string gridname = item->data(0, 1).toString().toLatin1().data();
  std::string attrname = item->text(0).toLatin1().data();

  pqSMAdaptor::setElementProperty(
    this->Internals->DsmProxy->GetProperty("DisabledObject"),
    "Modified");
  if (!item->parent()) {
    pqSMAdaptor::setElementProperty(
        this->Internals->DsmProxy->GetProperty("DisabledObject"),
        attrname.c_str());
    this->Internals->DsmProxy->UpdateVTKObjects();
  } else {
    pqSMAdaptor::setElementProperty(
        this->Internals->DsmProxy->GetProperty("DisabledObject"),
        steeringConfig[gridname].attributeConfig[attrname].hdfPath.c_str());
    this->Internals->DsmProxy->UpdateVTKObjects();
  }
}

//-----------------------------------------------------------------------------
void pqDsmOptions::ChangeItemState(QTreeWidgetItem *item)
{
  if (!item) return;
  //
  for (int i = 0; i < item->childCount(); i++) {
    QTreeWidgetItem *child = item->child(i);
    child->setCheckState(0, item->checkState(0));
    this->ChangeItemState(child);
  }
}

//-----------------------------------------------------------------------------
void pqDsmOptions::onPause()
{
#ifdef ICARUS_HAVE_H5FDDSM
  this->Internals->pqH5FDdsmWidget->onPause();
#endif
#ifdef ICARUS_HAVE_BONSAI
  this->Internals->pqBonsaiDsmWidget->onPause();
#endif
}

//-----------------------------------------------------------------------------
void pqDsmOptions::onPlay()
{
#ifdef ICARUS_HAVE_H5FDDSM
  this->Internals->pqH5FDdsmWidget->onPlay();
#endif
#ifdef ICARUS_HAVE_BONSAI
  this->Internals->pqBonsaiDsmWidget->onPlay();
#endif
}

//-----------------------------------------------------------------------------
void pqDsmOptions::RunScript()
{
  std::string scriptname = this->Internals->scriptPath->text().toLatin1().data();
}

//-----------------------------------------------------------------------------
void pqDsmOptions::SaveSnapshot() {
  std::string pngname = this->Internals->imageFilePath->text().toLatin1().data();
  vtksys::SystemTools::ReplaceString(pngname, "xxxxx", "%05i");
  char buffer[1024];
  sprintf(buffer, pngname.c_str(), this->Internals->CurrentTimeStep);  

  QSize size = pqActiveObjects::instance().activeView()->getSize();
  pqSaveScreenshotReaction::saveScreenshot(QString(buffer), size, -1, true);
}

//-----------------------------------------------------------------------------
void pqDsmOptions::onautoSaveImageChecked(int checked) {
  if (checked) {
    SaveSnapshot();
  }
}

//-----------------------------------------------------------------------------
void pqDsmOptions::ShowPipelineInGUI(vtkSMSourceProxy *source, const char *name, int Id)
{
  // Registering the proxy as a source will create a pipeline source in the browser
  // temporarily disable error messages to squash one warning about Input being
  // declared but not registered with the pipeline browser.
  //
  char proxyName[256];
  sprintf(proxyName, "%s-%d", name, Id);
  vtkSMProxyManager *pm = vtkSMProxyManager::GetProxyManager();
  pqApplicationCore::instance()->disableOutputWindow();
  pm->RegisterProxy("sources", proxyName, source);
  // @TODO fix jb
//  pqApplicationCore::instance()->enableOutputWindow();

  //
  // Set status of registered pipeline source to unmodified 
  //
  pqPipelineSource* pqsource = pqApplicationCore::instance()->
    getServerManagerModel()->findItem<pqPipelineSource*>(source);
  pqsource->setModifiedState(pqProxy::UNMODIFIED);

  //
  // To prevent ParaView from changing the time ranges every time the pipelines update,
  // we deregister the sources from the timekeeper and will manage time ourselves.
  //
//  QList<pqAnimationScene*> scenes = pqApplicationCore::instance()->getServerManagerModel()->findItems<pqAnimationScene *>();
//  foreach (pqAnimationScene *scene, scenes) {
//    pqTimeKeeper* timekeeper = scene->getServer()->getTimeKeeper();
//    timekeeper->removeSource(pqsource);
//  }

  //
  // (on First creation), make the output of the pipeline source visible.
  //
  pqDisplayPolicy* display_policy = pqApplicationCore::instance()->getDisplayPolicy();
  pqOutputPort *port = pqsource->getOutputPort(0);
  display_policy->setRepresentationVisibility(port, pqActiveObjects::instance().activeView(), 1);
}

//-----------------------------------------------------------------------------
void pqDsmOptions::SetTimeAndRange(double range[2], double timenow, bool GUIupdate)
{
  this->Internals->PipelineTimeRange[0] = range[0];
  this->Internals->PipelineTimeRange[1] = range[1];
  this->Internals->PipelineTime         = timenow;
  if (GUIupdate) {
    QList<pqAnimationScene*> scenes = pqApplicationCore::instance()->getServerManagerModel()->findItems<pqAnimationScene *>();
    foreach (pqAnimationScene *scene, scenes) {
      pqTimeKeeper* timekeeper = scene->getServer()->getTimeKeeper();
      timekeeper->blockSignals(true);
      vtkSMProxy *tkp = timekeeper->getProxy();
      if (tkp && tkp->IsA("vtkSMTimeKeeperProxy")) {
        vtkSMPropertyHelper(timekeeper->getProxy(), "TimeRange").Set(this->Internals->PipelineTimeRange,2);
        vtkSMPropertyHelper(timekeeper->getProxy(), "Time").Set(this->Internals->PipelineTime);
      }
      // Force the information about time to lie in our desired ranges - this prevents the animation view
      // from resetting back to some {0,1} interval etc.
      if (this->Internals->SteeringParser->GetHasXdmf() && this->Internals->XdmfReader) {
        vtkSMPropertyHelper(this->Internals->XdmfReader, "TimeRange").Set(this->Internals->PipelineTimeRange,2);
        vtkSMPropertyHelper(this->Internals->XdmfReader, "TimestepValues").Set(this->Internals->PipelineTime);
      }
      if (this->Internals->SteeringParser->GetHasBonsai() && this->Internals->BonsaiReader) {
        vtkSMPropertyHelper(this->Internals->BonsaiReader, "TimeRange").Set(this->Internals->PipelineTimeRange,2);
        vtkSMPropertyHelper(this->Internals->BonsaiReader, "TimestepValues").Set(this->Internals->PipelineTime);
      }
      if (this->Internals->SteeringParser->GetHasH5Part() && this->Internals->H5PartReader) {
        vtkSMPropertyHelper(this->Internals->H5PartReader, "TimeRange").Set(this->Internals->PipelineTimeRange,2);
        vtkSMPropertyHelper(this->Internals->H5PartReader, "TimestepValues").Set(this->Internals->PipelineTime);
      }
      if (this->Internals->SteeringParser->GetHasNetCDF() && this->Internals->NetCDFReader) {
        vtkSMPropertyHelper(this->Internals->NetCDFReader, "TimeRange").Set(this->Internals->PipelineTimeRange,2);
        vtkSMPropertyHelper(this->Internals->NetCDFReader, "TimestepValues").Set(this->Internals->PipelineTime);
      }
      if (this->Internals->SteeringParser->GetHasTable() && this->Internals->TableReader) {
        vtkSMPropertyHelper(this->Internals->TableReader, "TimeRange").Set(this->Internals->PipelineTimeRange,2);
        vtkSMPropertyHelper(this->Internals->TableReader, "TimestepValues").Set(this->Internals->PipelineTime);
      }

      vtkSMProxy *sp = scene->getProxy();
      if (sp && sp->IsA("vtkSMAnimationSceneProxy")) {
        vtkSMPropertyHelper(scene->getProxy(), "StartTime").Set(this->Internals->PipelineTimeRange[0]);
        vtkSMPropertyHelper(scene->getProxy(), "EndTime").Set(this->Internals->PipelineTimeRange[1]);
        vtkSMPropertyHelper(scene->getProxy(), "AnimationTime").Set(this->Internals->PipelineTime);
      }
    }
  }
}

//-----------------------------------------------------------------------------
void pqDsmOptions::UpdateDsmInformation()
{  
  this->DSMLocked.lock();
  this->Internals->DsmProxy->InvokeCommand("OpenCollective");
  double range[2] = { -1.0, -1.0 };
  vtkSMPropertyHelper timerange(this->Internals->DsmProxyHelper, "TimeRangeInfo");
  timerange.UpdateValueFromServer();
  timerange.Get(range,2);
  if (range[0]!=-1.0 && range[1]!=-1.0) {
    this->SetTimeAndRange(range, this->Internals->PipelineTime);
    std::cout << "Time Range updated to {" << range[0] << "," << range[1] << "}" << std::endl;
  }
  this->Internals->DsmProxy->InvokeCommand("CloseCollective");
  this->DSMLocked.unlock();
}

//-----------------------------------------------------------------------------
void pqDsmOptions::GetPipelineTimeInformation(vtkSMSourceProxy *source)
{
  source->UpdatePipelineInformation();
  //
  if (source->GetProperty("TimestepValues")) {
    double tval[1] = { -1.0 };
    vtkSMPropertyHelper time(source, "TimestepValues");
    time.UpdateValueFromServer();
    time.Get(tval,1);
    if (tval[0]!=-1.0) {
      this->SetTimeAndRange(this->Internals->PipelineTimeRange, tval[0]);
    }
    else {
//      std::cout << "Time not present, using step number instead" << std::endl;
      // update GUI time
      //QList<pqAnimationScene*> scenes = pqApplicationCore::instance()->getServerManagerModel()->findItems<pqAnimationScene *>();
      //foreach (pqAnimationScene *scene, scenes) {
      //  scene->setAnimationTime(this->Internals->CurrentTimeStep);
      //}
    }
  }
}

//-----------------------------------------------------------------------------
void pqDsmOptions::CreateXdmfPipeline()
{
  this->Internals->XdmfViewer.TakeReference(vtkCustomPipelineHelper::New("sources", "XdmfReaderBlock"));
  // Connect our DSM manager to the reader (Xdmf)
  pqSMAdaptor::setProxyProperty(
    this->Internals->XdmfViewer->Pipeline->GetProperty("DsmManager"), this->Internals->DsmProxy
  );
  this->Internals->XdmfViewer->Pipeline->UpdateProperty("DsmManager");
  //
  vtkSMCompoundSourceProxy *pipeline1 = vtkSMCompoundSourceProxy::SafeDownCast(
    this->Internals->XdmfViewer->Pipeline);
  this->Internals->XdmfReader = vtkSMSourceProxy::SafeDownCast(pipeline1->GetProxy("XdmfReader1"));
}

//-----------------------------------------------------------------------------
void pqDsmOptions::UpdateXdmfInformation()
{
  this->GetPipelineTimeInformation(this->Internals->XdmfReader);
}

//-----------------------------------------------------------------------------
void pqDsmOptions::UpdateXdmfPipeline()
{
  // on first update
  if (this->Internals->CreatePipelines) {
    //
    // We need to know if the XdmfReader output is multiblock or not
    // first update the reader
    //
    bool multiblock = false;
    this->Internals->XdmfReader->UpdatePipeline(this->Internals->PipelineTime);
    // get the data information from the output
    vtkSMOutputPort *out = this->Internals->XdmfReader->GetOutputPort((unsigned int)0);
    multiblock = out->GetDataInformation()->GetCompositeDataInformation()->GetDataIsComposite();
    // 
    if (multiblock) {
      this->Internals->XdmfViewer->PipelineEnd = this->Internals->XdmfViewer->Pipeline;
      //
      vtkPVCompositeDataInformation *pvcdi = out->GetDataInformation()->GetCompositeDataInformation();
      SteeringGUIWidgetMap &SteeringWidgetMap = this->Internals->SteeringParser->GetSteeringWidgetMap();
      QList<QVariant> blocks;
      int N = pvcdi->GetNumberOfChildren();
      for (int i=0; i<N; i++) {
        blocks.append(i+1);
        const char *gridname = pvcdi->GetName(i);
        for (SteeringGUIWidgetMap::iterator it=SteeringWidgetMap.begin(); it!=SteeringWidgetMap.end(); ++it) {
          if (it->second.AssociatedGrid==std::string(gridname)) {
            blocks.removeOne(i+1);
            // todo : add index to GUI block selection object
            this->BindWidgetToGrid(it->first.c_str(), &it->second, i+1);
            continue;
          }
        }
      }
      pqSMAdaptor::setMultipleElementProperty(
        this->Internals->XdmfViewer->Pipeline->GetProperty("BlockIndices"), blocks);  
      this->Internals->XdmfViewer->Pipeline->UpdateProperty("BlockIndices");
      this->Internals->XdmfViewer->PipelineEnd = this->Internals->XdmfViewer->Pipeline;
    } 
    else {
      this->Internals->XdmfViewer->PipelineEnd = this->Internals->XdmfReader;
    }
  }
  else {
    this->Internals->XdmfReader->InvokeCommand("Modified");
  }

  //
  // Trigger complete update of reader/ExtractBlock/etc pipeline
  //
  this->Internals->XdmfViewer->UpdateAll();
}

//-----------------------------------------------------------------------------
// Bonsai
//-----------------------------------------------------------------------------
void pqDsmOptions::CreateBonsaiPipeline()
{
  vtkSMProxyManager *pm = vtkSMProxyManager::GetProxyManager();
  this->Internals->BonsaiReader.TakeReference(
    vtkSMSourceProxy::SafeDownCast(pm->NewProxy("sources", "BonsaiReader")));
  // Connect our DSM manager to the reader (Xdmf)
  pqSMAdaptor::setProxyProperty(
    this->Internals->BonsaiReader->GetProperty("DsmManager"), this->Internals->DsmProxy
  );
  this->Internals->BonsaiReader->UpdateProperty("DsmManager");
  //
  pqSMAdaptor::setElementProperty(
    this->Internals->BonsaiReader->GetProperty("GenerateVertexCells"), 1);
  this->Internals->BonsaiReader->UpdateVTKObjects();
}
//-----------------------------------------------------------------------------
void pqDsmOptions::UpdateBonsaiInformation()
{
  this->Internals->BonsaiReader->InvokeCommand("FileModified");
  this->GetPipelineTimeInformation(this->Internals->BonsaiReader);
}
//-----------------------------------------------------------------------------
void pqDsmOptions::UpdateBonsaiPipeline()
{
  this->Internals->BonsaiReader->UpdatePipeline(this->Internals->PipelineTime);
}
//-----------------------------------------------------------------------------
// H5Part
//-----------------------------------------------------------------------------
void pqDsmOptions::CreateH5PartPipeline()
{
  vtkSMProxyManager *pm = vtkSMProxyManager::GetProxyManager();
  this->Internals->H5PartReader.TakeReference(
    vtkSMSourceProxy::SafeDownCast(pm->NewProxy("sources", "H5PartDsm")));
  // Connect our DSM manager to the reader (Xdmf)
  pqSMAdaptor::setProxyProperty(
    this->Internals->H5PartReader->GetProperty("DsmManager"), this->Internals->DsmProxy
  );
  this->Internals->H5PartReader->UpdateProperty("DsmManager");
  //
  std::vector<std::string> H5PartStrings = this->Internals->SteeringParser->GetH5PartStrings();
  //
  pqSMAdaptor::setElementProperty(
    this->Internals->H5PartReader->GetProperty("StepName"), H5PartStrings[0].c_str()); 
  pqSMAdaptor::setElementProperty(
    this->Internals->H5PartReader->GetProperty("Xarray"), H5PartStrings[1].c_str()); 
  pqSMAdaptor::setElementProperty(
    this->Internals->H5PartReader->GetProperty("Yarray"), H5PartStrings[2].c_str()); 
  pqSMAdaptor::setElementProperty(
    this->Internals->H5PartReader->GetProperty("Zarray"), H5PartStrings[3].c_str()); 
  pqSMAdaptor::setElementProperty(
    this->Internals->H5PartReader->GetProperty("GenerateVertexCells"), 1); 
  this->Internals->H5PartReader->UpdateVTKObjects();
}
//-----------------------------------------------------------------------------
void pqDsmOptions::UpdateH5PartInformation()
{
  this->Internals->H5PartReader->InvokeCommand("FileModified");
  this->GetPipelineTimeInformation(this->Internals->H5PartReader);
}
//-----------------------------------------------------------------------------
void pqDsmOptions::UpdateH5PartPipeline()
{
  this->Internals->H5PartReader->UpdatePipeline(this->Internals->PipelineTime);
}
//-----------------------------------------------------------------------------
// Table
//-----------------------------------------------------------------------------
void pqDsmOptions::CreateTablePipeline()
{
  vtkSMProxyManager *pm = vtkSMProxyManager::GetProxyManager();
  this->Internals->TableReader.TakeReference(
    vtkSMSourceProxy::SafeDownCast(pm->NewProxy("sources", "TableDsm")));
  // Connect our DSM manager to the reader (Xdmf)
  pqSMAdaptor::setProxyProperty(
    this->Internals->TableReader->GetProperty("DsmManager"), this->Internals->DsmProxy
  );
  this->Internals->TableReader->UpdateProperty("DsmManager");
  
  // Populate a string list with each name we have read from the template
  vtkSmartPointer<vtkSMProxy> StringList = pm->NewProxy("stringlists", "StringList");
  const std::vector<std::string> &list = this->Internals->SteeringParser->GetTableStrings();
  for (size_t i=0; i<list.size(); i++) {
    pqSMAdaptor::setElementProperty(
      StringList->GetProperty("AddString"), list[i].c_str());
    StringList->UpdateProperty("AddString");
  }
  // Pass the list into the reader
  pqSMAdaptor::setProxyProperty(
    this->Internals->TableReader->GetProperty("NameStrings"), StringList);
  this->Internals->TableReader->UpdateProperty("NameStrings");
}
//-----------------------------------------------------------------------------
void pqDsmOptions::UpdateTableInformation()
{
  this->Internals->TableReader->InvokeCommand("FileModified");
  this->GetPipelineTimeInformation(this->Internals->TableReader);
}
//-----------------------------------------------------------------------------
void pqDsmOptions::UpdateTablePipeline()
{
  this->Internals->TableReader->UpdatePipeline(this->Internals->PipelineTime);
}
//-----------------------------------------------------------------------------
// netCDF
//-----------------------------------------------------------------------------
void pqDsmOptions::CreateNetCDFPipeline()
{
  vtkSMProxyManager *pm = vtkSMProxyManager::GetProxyManager();
  this->Internals->NetCDFReader.TakeReference(
    vtkSMSourceProxy::SafeDownCast(pm->NewProxy("sources", "NetCDFDsm")));
  // Connect our DSM manager to the reader (Xdmf)
  pqSMAdaptor::setProxyProperty(
    this->Internals->NetCDFReader->GetProperty("DsmManager"), this->Internals->DsmProxy
  );
  this->Internals->NetCDFReader->UpdateProperty("DsmManager");
  // base netCDF reader classes need a non NULL filename
  pqSMAdaptor::setElementProperty(
    this->Internals->NetCDFReader->GetProperty("FileName"), "dsm");
  this->Internals->NetCDFReader->UpdateProperty("FileName");
  //
  // Temporary fix for netCDF which has not yet declared time ranges
  //
  double range[2] = {0.0, 25.0};
  this->SetTimeAndRange(range, range[0]);
}
//-----------------------------------------------------------------------------
void pqDsmOptions::UpdateNetCDFInformation()
{
  this->Internals->NetCDFReader->InvokeCommand("FileModified");
  this->GetPipelineTimeInformation(this->Internals->NetCDFReader);
}
//-----------------------------------------------------------------------------
void pqDsmOptions::UpdateNetCDFPipeline()
{
  this->Internals->NetCDFReader->UpdatePipeline(this->Internals->PipelineTime);
}
//-----------------------------------------------------------------------------
void pqDsmOptions::UpdateXdmfTemplate()
{
  if (!this->Internals->xdmfFilePathLineEdit->text().isEmpty()) {
    if (this->Internals->xdmfFileTypeComboBox->currentIndex() == XML_USE_ORIGINAL) {
      // Read file from GUI and send the string to the server for convenience
      QFile file(this->Internals->xdmfFilePathLineEdit->text().toLatin1().data());
      if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        vtkGenericWarningMacro(<< "Could not open description file");

      QTextStream xdmfDescription(&file);
      pqSMAdaptor::setElementProperty(
          this->Internals->DsmProxy->GetProperty("XdmfDescription"),
          xdmfDescription.readAll().toLatin1().constData());
      this->Internals->DsmProxy->UpdateVTKObjects();
      file.close();
    }
    //
    bool forceXdmfGeneration = false;
    static std::string descriptionFilePath = this->Internals->xdmfFilePathLineEdit->text().toLatin1().data();
    //
    if (this->Internals->xdmfFileTypeComboBox->currentIndex() == XML_USE_TEMPLATE) {
      forceXdmfGeneration = this->Internals->forceXdmfGeneration->isChecked();
      // Only re-generate if the description file path has changed or if force is set to true
      if (this->Internals->CreatePipelines || forceXdmfGeneration)
      {
        descriptionFilePath = this->Internals->xdmfFilePathLineEdit->text().toLatin1().data();
        // Generate xdmf file for reading
        // Send the whole string representing the DOM and not just the file path so that the 
        // template file does not need to be present on the server anymore
        pqSMAdaptor::setElementProperty(
            this->Internals->DsmProxy->GetProperty("XdmfTemplateDescription"),
            this->Internals->SteeringParser->GetXdmfXmlDoc().c_str());

        this->Internals->DsmProxy->UpdateVTKObjects();
        this->Internals->DsmProxy->InvokeCommand("GenerateXdmfDescription");
      }
    }
  }
  else {
    // The base class requires a non NULL filename, (we use stdin for DSM)
    pqSMAdaptor::setElementProperty(
        this->Internals->XdmfViewer->Pipeline->GetProperty("FileName"), "stdin"); 
    this->Internals->XdmfViewer->Pipeline->UpdateProperty("FileName");
  }
}
//-----------------------------------------------------------------------------
void pqDsmOptions::GetViewsForPipeline(vtkSMSourceProxy *source, std::set<pqView*> &viewlist)
{
  // find the pipeline associated with this source
  pqPipelineSource* pqsource = pqApplicationCore::instance()->
    getServerManagerModel()->findItem<pqPipelineSource*>(source);
  // and find all views it is present in
  if (pqsource) {
    foreach (pqView *view, pqsource->getViews()) {
      pqDataRepresentation *repr = pqsource->getRepresentation(0, view);
      if (repr && repr->isVisible()) {
        // add them to the list
        viewlist.insert(view);
      }
    }
  }
}
//-----------------------------------------------------------------------------
void pqDsmOptions::UpdateDsmPipeline()
{
  // lock the DSM 
  this->DSMLocked.lock();
  this->Internals->DsmProxy->InvokeCommand("OpenCollective");
//    H5FD_dsm_lock();
//    H5FD_dsm_dump();

  // there may be multiple views to update, so build a list
  std::set<pqView*> viewlist;

  //
  // If Table present, update the pipeline
  //
  if (this->Internals->SteeringParser && this->Internals->SteeringParser->GetHasTable()) {
    // create pipeline if needed
    if (this->Internals->CreatePipelines) { 
      this->CreateTablePipeline();
    }
    // update information
    this->UpdateTableInformation();
    // update data
    this->UpdateTablePipeline();
    if (this->Internals->CreatePipelines) { 
      this->ShowPipelineInGUI(this->Internals->TableReader, this->Internals->SteeringParser->GetTableName().c_str(), 0);
    }
    // we will need to update all views for this object
    this->GetViewsForPipeline(this->Internals->TableReader, viewlist);
  }

  //
  // If Xdmf present, update the pipeline 
  //
  if (this->Internals->SteeringParser && this->Internals->SteeringParser->GetHasXdmf()) {
    if (this->Internals->CreatePipelines) { 
      // XdmfReader+ExtractBlock filter is a custom XML filter description
      vtkCustomPipelineHelper::RegisterCustomFilters();
      // Create pipeline
      this->CreateXdmfPipeline();
    }
    // Regenerate Template if necessary
    this->UpdateXdmfTemplate();
    // update information
    this->UpdateXdmfInformation();
    // update data
    this->UpdateXdmfPipeline();
    // unregister custom filters
    if (this->Internals->CreatePipelines) { 
      this->ShowPipelineInGUI(this->Internals->XdmfViewer->PipelineEnd, "Xdmf-Dsm", 0);
      vtkCustomPipelineHelper::UnRegisterCustomFilters();
    }
    // we will need to update all views for this object
    this->GetViewsForPipeline(this->Internals->XdmfViewer->PipelineEnd, viewlist);
  }

  //
  // If Bonsai present, update the pipeline
  //
  std::cout << "About to setup Bonsai pipeline " << std::endl;
  if (this->Internals->SteeringParser && this->Internals->SteeringParser->GetHasBonsai()) {
    // create pipeline if needed
  std::cout << "About to CreatePipelines Bonsai pipeline " << std::endl;
    if (this->Internals->CreatePipelines) {
      this->CreateBonsaiPipeline();
    }
  std::cout << "About to UpdateBonsaiInformation Bonsai pipeline " << std::endl;
    // update information
    this->UpdateBonsaiInformation();
    // update data
  std::cout << "About to setup Bonsai pipeline 2 " << std::endl;
    this->UpdateBonsaiPipeline();
    if (this->Internals->CreatePipelines) {
      this->ShowPipelineInGUI(this->Internals->BonsaiReader, "Bonsai-Dsm", 0);
    }
    // we will need to update all views for this object
    this->GetViewsForPipeline(this->Internals->BonsaiReader, viewlist);
  }

  //
  // If H5Part present, update the pipeline 
  //
  if (this->Internals->SteeringParser && this->Internals->SteeringParser->GetHasH5Part()) {
    // create pipeline if needed
    if (this->Internals->CreatePipelines) { 
      this->CreateH5PartPipeline();
    }
    // update information
    this->UpdateH5PartInformation();
    // update data
    this->UpdateH5PartPipeline();
    if (this->Internals->CreatePipelines) { 
      this->ShowPipelineInGUI(this->Internals->H5PartReader, "H5Part-Dsm", 0);
    }
    // we will need to update all views for this object
    this->GetViewsForPipeline(this->Internals->H5PartReader, viewlist);
  }

  //
  // If netCDF present, update the pipeline
  //
  if (this->Internals->SteeringParser && this->Internals->SteeringParser->GetHasNetCDF()) {
    // create pipeline if needed
    if (this->Internals->CreatePipelines) { 
      this->CreateNetCDFPipeline();
    }
    // update information
    this->UpdateNetCDFInformation();
    // update data
    this->UpdateNetCDFPipeline();
    if (this->Internals->CreatePipelines) { 
      this->ShowPipelineInGUI(this->Internals->NetCDFReader, "NetCDF-Dsm", 0);
    }
    // we will need to update all views for this object
    this->GetViewsForPipeline(this->Internals->NetCDFReader, viewlist);
  }

  //
  // We only create new pipelines on the first iteration, or if saving all
  //
  this->Internals->CreatePipelines = this->Internals->storeDsmContents->isChecked();

  //
  // Allow messages on the accept which were blocked during an update.
  //
  this->Internals->DsmProxy->InvokeCommand("CloseCollective");
  this->DSMLocked.unlock();

  //
  // If our pipelines updated and changed time, set the value and set GUI updates = true
  // to reload the animation views etc.
  //
  ++this->Internals->CurrentTimeStep;
  this->SetTimeAndRange(this->Internals->PipelineTimeRange, this->Internals->PipelineTime, true);

  //
  // Update all views which are associated with out pipelines
  //
  for (std::set<pqView*>::iterator it=viewlist.begin(); it!=viewlist.end(); ++it) {
    (*it)->render();
  }
  //
  // Update events which take place after updates
  //
  if (this->Internals->autoSaveImage->isChecked()) {
    this->SaveSnapshot();
  }
  if (this->Internals->autoExport->isChecked()) {
    this->ExportData(true);
  }
  if (this->Internals->runScript->isChecked()) {
    this->RunScript();
  }
}

//-----------------------------------------------------------------------------
void pqDsmOptions::UpdateDsmStatus(QString status)
{
  this->Internals->infoCurrentSteeringCommand->clear();
  this->Internals->infoCurrentSteeringCommand->insert(status);
}
/*
Was used to track the active source and write it to DSM
//-----------------------------------------------------------------------------
void pqDsmOptions::TrackSource()
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
*/

//-----------------------------------------------------------------------------
void pqDsmOptions::BindWidgetToGrid(const char *propertyname, SteeringGUIWidgetInfo *info, int blockindex)
{
  if (this->Internals->ActiveView==NULL && pqActiveView::instance().current()) {
    this->onActiveViewChanged(pqActiveView::instance().current());
  }

  if (!this->Internals->XdmfViewer) {
    return;
  }

  //
  // Create a pipeline with XdmfReader+FlattenOneBlock+Transform filters inside it
  // [This can be extended to any number of internal filters]
  //
  vtkSMProxyManager *pm = vtkSMProxyManager::GetProxyManager();
  CustomPipeline widgetPipeline;
  widgetPipeline.TakeReference(vtkCustomPipelineHelper::New("filters", "TransformBlock"));
  this->Internals->WidgetPipelines.push_back(widgetPipeline);
  //
  // Replace the internal XdmfReader Proxy in the widget pipeline with our primary XdmfReader
  // so that we can display the pipeline in the browser without warnings about unregistered objects
  //
  vtkSMCompoundSourceProxy *ExtractOneBlock = widgetPipeline->GetCompoundPipeline();
  vtkSMSourceProxy *ExtractBlock = vtkSMSourceProxy::SafeDownCast(ExtractOneBlock->GetProxy("FlattenOneBlock1"));
  ExtractOneBlock->AddProxy("XdmfReader1", this->Internals->XdmfReader);
  pqSMAdaptor::setInputProperty(ExtractBlock->GetProperty("Input"), this->Internals->XdmfReader, 0);
  // 
  // Select just the one block/grid that this widget is bound to (NB. index subtract one : flat index)
  //
  int defaultNullDataType = this->Internals->SteeringParser->GetGridTypeForBlock(blockindex-1);
  pqSMAdaptor::setElementProperty(
    widgetPipeline->Pipeline->GetProperty("BlockIndex"), blockindex);  
  pqSMAdaptor::setElementProperty(
    widgetPipeline->Pipeline->GetProperty("DefaultNullDataType"), defaultNullDataType);
  //
  widgetPipeline->UpdateAll();
  //
  // Create a pipeline object in the browser to represent this widget control
  //
  char proxyName[256];
  sprintf(proxyName, "DSM-Control-%s-%d", info->AssociatedGrid.c_str(), this->Internals->CurrentTimeStep);
  pm->RegisterProxy("sources", proxyName, widgetPipeline->Pipeline);
  //
  pqPipelineSource* source = pqApplicationCore::instance()->
    getServerManagerModel()->findItem<pqPipelineSource*>(widgetPipeline->Pipeline);
  source->setModifiedState(pqProxy::UNMODIFIED);
  // Visible by default
  pqDisplayPolicy* display_policy = pqApplicationCore::instance()->getDisplayPolicy();
  pqOutputPort *port = source->getOutputPort(0);
  display_policy->setRepresentationVisibility(port, pqActiveObjects::instance().activeView(), 1);

  //
  // We need to map the user interactions in the widget to the transform in this mini-pipeline.
  // Note that the 3Dwidget was created automatically by the pqDsmObjectInspector when the XML
  // was parsed, and this was before any actual filters were created
  //
  vtkSMProxyProperty *transform = vtkSMProxyProperty::SafeDownCast(
    widgetPipeline->Pipeline->GetProperty("Transform"));
  vtkSMPropertyLink* link = vtkSMPropertyLink::New();
  link->AddLinkedProperty(this->Internals->DsmProxyHelper, propertyname, vtkSMPropertyLink::INPUT);
  link->AddLinkedProperty(transform->GetProxy(0), "Position", vtkSMPropertyLink::OUTPUT);
  //
  // And set the widget initial position to the centre of the grid
  //
  vtkSMOutputPort *out = widgetPipeline->GetOutputPort(0);
  double bounds[6];
  out->GetDataInformation()->GetBounds(bounds);
  info->pqWidget->resetBounds(bounds);

  //
  // Experimental, not working yet
  //
  //  vtkSMProxy *p = info->ControlledProxy;
  if (info->AbstractWidget->IsA("vtkBoxWidget2")) {
    vtkBoxWidget2::SafeDownCast(info->AbstractWidget)->SetRotationEnabled(0);
  }
}
//-----------------------------------------------------------------------------
void pqDsmOptions::onPreAccept()
{
#ifdef ICARUS_HAVE_H5FDDSM
  this->Internals->pqH5FDdsmWidget->onPreAccept();
#endif
}
//-----------------------------------------------------------------------------
void pqDsmOptions::onPostAccept()
{
#ifdef ICARUS_HAVE_H5FDDSM
  this->Internals->pqH5FDdsmWidget->onPostAccept();
#endif
  if (this->Internals->acceptIsPlay->isChecked()) {
    this->onPlay();
  }
}
//-----------------------------------------------------------------------------
void pqDsmOptions::ExportData(bool force)
{
  // 
  // @TODO Find a way to 'accept' all 3D widgets so that we can
  // trigger the writer with the correct values
  //
  pqActiveObjects::instance().activeView()->forceRender();

  // Get Source objects for DataExportWidgets
  // @TODO currently we only support one - if we wanted more, we'd
  // need a separate writer for each I think.
  QList<pqDataExportWidget*> widgets = this->Internals->pqObjectInspector->findChildren<pqDataExportWidget*>();
  for (int i=0; i<widgets.size(); i++) {
    pqDataExportWidget* widget = widgets[i];
    vtkSMProxy* controlledProxy = widget->getControlledProxy();
/*    
    // was the associated command 'clicked' (modified)
    QString command = widget->getProperty();
    vtkSMProperty *cp = vtkSMProperty::SafeDownCast(this->Internals->DsmProxyHelper->GetProperty(command.toLatin1().data()));
    if (force || cp->GetMTime()>this->Internals->LastExportTime) {
      if (force) {
        cp->Modified();
        this->Internals->DsmProxyHelper->UpdateVTKObjects();
      }
      //
      // make sure the Steering Writer knows where to get data from
      //
      if (this->Internals->SteeringWriter) {
        vtkSMPropertyHelper ip(this->Internals->SteeringWriter, "Input");
        ip.Set(controlledProxy,0);
        vtkSMPropertyHelper svp(this->Internals->SteeringWriter, "WriteDescription");
        svp.Set(widget->value().toLatin1().data());
        this->Internals->SteeringWriter->UpdateVTKObjects();
        this->Internals->SteeringWriter->UpdatePipeline();
      }
    }
    */
  }
  //
  this->Internals->LastExportTime.Modified();
}
//-----------------------------------------------------------------------------
// Useful snippet below, please do not delete.
//-----------------------------------------------------------------------------

/* 
  //
  // Do any of the DsmHelperProxy properties have widgets attached
  //
  // Obsolete : We do this in the parser now : keep for future use : just in case
  //
  vtkSMPropertyIterator *iter=this->Internals->DsmProxyHelper->NewPropertyIterator();
  while (!iter->IsAtEnd()) {
    vtkPVXMLElement *h = iter->GetProperty()->GetHints();
    int N = h ? h->GetNumberOfNestedElements() : 0;
    if (N>0) {
      //
      SteeringGUIWidgetInfo &info = SteeringWidgetMap[iter->GetKey()];
      info.Property = iter->GetProperty();
      //
      for (int i=0; i<N; i++) {
        vtkPVXMLElement *n = h->GetNestedElement(i);
        if (n->GetName()==std::string("AssociatedGrid")) {
          this->Internals->DsmProxyHelper;
          info.AssociatedGrid = n->GetAttribute("name");
        }
        if (n->GetName()==std::string("WidgetControl")) {
          info.WidgetType = n->GetAttribute("name");
        }
      }
    }
    iter->Next();
  }
  iter->Delete();
*/

  //
  // H5FD_dsm_dump();
  //
