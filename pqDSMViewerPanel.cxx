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
#include "vtkSMPropertyHelper.h"
#include "vtkSMInputProperty.h"
#include "vtkSMProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMStringVectorProperty.h"
#include "vtkSMIntVectorProperty.h"
#include "vtkSMArraySelectionDomain.h"
#include "vtkSMProxyProperty.h"
#include "vtkSMViewProxy.h"
#include "vtkSMRepresentationProxy.h"
#include "vtkSMNewWidgetRepresentationProxy.h"
#include "vtkSMPropertyIterator.h"
#include "vtkSMPropertyLink.h"
#include "vtkSMOutputPort.h"
#include "vtkSMCompoundSourceProxy.h"
#include "vtkPVDataInformation.h"
#include "vtkPVCompositeDataInformation.h"
#include "vtkProcessModule.h"
#include "vtkProcessModuleConnectionManager.h"
#include "vtkPVXMLElement.h"
#include "vtkPVXMLParser.h"
#include "vtkAbstractWidget.h"
#include "vtkBoxWidget2.h"

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
#include "pqObjectBuilder.h"
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
#include "pqTimeKeeper.h"
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
#include "vtkDSMManager.h"
#include "H5FDdsmComm.h"
#include "XdmfSteeringParser.h"
#include "vtkCustomPipelineHelper.h"
//
#include <vtksys/SystemTools.hxx>
//----------------------------------------------------------------------------
#define XML_USE_TEMPLATE 1
#define XML_USE_ORIGINAL 0
#define XML_USE_SENT     2
//----------------------------------------------------------------------------
//
typedef vtkSmartPointer<vtkCustomPipelineHelper> CustomPipeline;
typedef std::vector< CustomPipeline > PipelineList;
//----------------------------------------------------------------------------
class pqDSMViewerPanel::pqInternals : public QObject, public Ui::DSMViewerPanel 
{
public:
  pqInternals(pqDSMViewerPanel* p) : QObject(p)
  {
    this->DSMInitialized     = false;
    this->DSMListening       = false;
    this->ActiveSourcePort   = 0;
    this->ActiveView         = NULL;
    this->pqObjectInspector  = NULL;
    this->pqDSMProxyHelper   = NULL;
    this->SteeringParser     = NULL;
    this->CurrentTimeStep    = 0;
    this->DSMCommType        = 0;
    this->CreateObjects      = true;
  };

  //
  ~pqInternals() {
    if (this->DSMProxyCreated()) {
      this->DSMProxy->InvokeCommand("DestroyDSM");
    }
    this->DSMProxy           = NULL;
    this->DSMProxyHelper     = NULL;
    this->XdmfReader         = NULL;
    if (this->SteeringParser) {
      delete this->SteeringParser;
    }
    this->ActiveSourceProxy  = NULL;
  }
  //
  void CreateDSMProxy() {
    vtkSMProxyManager *pm = vtkSMProxy::GetProxyManager();
    this->DSMProxy = pm->NewProxy("icarus_helpers", "DSMManager");
    this->DSMProxy->SetConnectionID(pqActiveObjects::instance().activeServer()->GetConnectionID());
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
    this->DSMProxyHelper->SetConnectionID(pqActiveObjects::instance().activeServer()->GetConnectionID());
    this->DSMProxyHelper->UpdatePropertyInformation();
    this->DSMProxyHelper->UpdateVTKObjects();
//    pm->RegisterProxy("icarus_helpers", "DSMProxyHelper", this->DSMProxyHelper);
    //
    this->TransformProxy = pm->NewProxy("extended_sources", "Transform3");
    this->TransformProxy->SetConnectionID(pqActiveObjects::instance().activeServer()->GetConnectionID());
//    pm->RegisterProxy("extended_sources", "Transform3", this->TransformProxy);
    //
    // Set our Transform object in the HelperProxy for later use
//    pqSMAdaptor::setProxyProperty(this->DSMProxyHelper->GetProperty("Transform"), this->TransformProxy);
    //
    // Set the DSM manager it uses for communication, (warning: updates all properties)
    pqSMAdaptor::setProxyProperty(this->DSMProxyHelper->GetProperty("DSMManager"), this->DSMProxy);
    this->DSMProxyHelper->UpdatePropertyInformation();
    this->DSMProxyHelper->UpdateVTKObjects();
    //
    //
    // wrap the DSMProxyHelper object in a pqProxy so that we can use it in our object inspector
    if (this->pqDSMProxyHelper) {
      delete this->pqDSMProxyHelper;
    }
    this->pqDSMProxyHelper = new pqProxy("icarus_helpers", "DSMProxyHelper", 
      this->DSMProxyHelper, pqActiveObjects::instance().activeServer()); 
  }
  //
  bool DSMProxyCreated() { return this->DSMProxy!=NULL; }
  bool HelperProxyCreated() { return this->DSMProxyHelper!=NULL; }

  // ---------------------------------------------------------------
  // Variables for DSM management
  // ---------------------------------------------------------------
  int                                       DSMInitialized;
  vtkSmartPointer<vtkSMProxy>               DSMProxy;
  vtkSmartPointer<vtkSMProxy>               DSMProxyHelper;
  // ---------------------------------------------------------------
  // Principal pipeline of Xdmf[->ExtractBlock]
  // ---------------------------------------------------------------
  vtkSmartPointer<vtkCustomPipelineHelper>  XdmfViewer;
  vtkSmartPointer<vtkSMSourceProxy>         XdmfReader;
  bool                                      DSMListening;
  int                                       DSMCommType;
  bool                                      CreateObjects;
  // ---------------------------------------------------------------
  // Display of controls via object inspector panel
  // ---------------------------------------------------------------
  XdmfSteeringParser                       *SteeringParser;
  pqObjectInspectorWidget                  *pqObjectInspector;
  pqProxy                                  *pqDSMProxyHelper;
  // ---------------------------------------------------------------
  // Pipelines for steerable objects, one per 3D widget
  // ---------------------------------------------------------------
  PipelineList                              WidgetPipelines;
  // ---------------------------------------------------------------
  // General management 
  // ---------------------------------------------------------------
  pqRenderView                             *ActiveView;
  int                                       CurrentTimeStep;
  // ---------------------------------------------------------------
  // Experimental, writing to Xdmf 
  // ---------------------------------------------------------------
  vtkSmartPointer<vtkSMProxy>               ActiveSourceProxy;
  int                                       ActiveSourcePort;
  //
  //
  vtkSmartPointer<vtkSMSourceProxy>        TransformFilterProxy;
  vtkSmartPointer<vtkSMProxy>              TransformProxy;
};
//-----------------------------------------------------------------------------
class dsmStandardWidgets : public pq3DWidgetInterface
{
public:
  pq3DWidget* newWidget(const QString& name, vtkSMProxy* referenceProxy, vtkSMProxy* controlledProxy)
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
  this->connect(this->Internals->testButton,
    SIGNAL(clicked()), this, SLOT(testClicked()));
  this->connect(this->Internals->testButton2,
    SIGNAL(clicked()), this, SLOT(test2Clicked()));
  
  // DSM Commands
  this->connect(this->Internals->addServerDSM,
      SIGNAL(clicked()), this, SLOT(onAddServerDSM()));

  this->connect(this->Internals->publishDSM,
    SIGNAL(clicked()), this, SLOT(onPublishDSM()));

  this->connect(this->Internals->unpublishDSM,
    SIGNAL(clicked()), this, SLOT(onUnpublishDSM()));

  // Steering commands
  this->connect(this->Internals->scPause,
      SIGNAL(clicked()), this, SLOT(onSCPause()));

  this->connect(this->Internals->scPlay,
      SIGNAL(clicked()), this, SLOT(onSCPlay()));

  this->connect(this->Internals->dsmWriteDisk,
      SIGNAL(clicked()), this, SLOT(onSCWriteDisk()));

  this->connect(this->Internals->dsmArrayTreeWidget,
      SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(onArrayItemChanged(QTreeWidgetItem*, int)));

//  this->connect(this->Internals->writeToDSM,
//      SIGNAL(clicked()), this, SLOT(onWriteDataToDSM()));
  this->connect(this->Internals->writeToDSM,
      SIGNAL(clicked()), this, SLOT(onWriteSteeringDataToDSM()));
  //
  // Link paraview events to callbacks
  //
  pqServerManagerModel* smModel =
    pqApplicationCore::instance()->getServerManagerModel();

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

  if (this->Internals->DSMProxyCreated()) {
    // Get the XML for our Helper proxy and send it to the DSM manager
    // it will register the XML with the proxt manager on the server
    std::string HelperProxyXML = this->Internals->SteeringParser->GetHelperProxyString();
    // register proxy on the server
    pqSMAdaptor::setElementProperty(
      this->Internals->DSMProxy->GetProperty("HelperProxyXMLString"), HelperProxyXML.c_str());
    this->Internals->DSMProxy->UpdateProperty("HelperProxyXMLString");
    // and register on the client too 
    vtkDSMManager::RegisterHelperProxy(HelperProxyXML.c_str());
    // now create an actual proxy
    this->Internals->CreateDSMHelperProxy();
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
  this->Internals->pqObjectInspector = new pqObjectInspectorWidget(this->Internals->devTab);
  this->Internals->pqObjectInspector->setView(this->Internals->ActiveView);
  this->Internals->pqObjectInspector->setProxy(this->Internals->pqDSMProxyHelper);
  this->Internals->pqObjectInspector->setDeleteButtonVisibility(false);
  this->Internals->pqObjectInspector->setHelpButtonVisibility(false);
  this->Internals->generatedLayout->addWidget(this->Internals->pqObjectInspector);

/* 
  //
  // Do any of the DSMHelperProxy properties have widgets attached
  //
  // Obsolete : We do this in the parser now : keep for future use : just in case
  //
  vtkSMPropertyIterator *iter=this->Internals->DSMProxyHelper->NewPropertyIterator();
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
          this->Internals->DSMProxyHelper;
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
}
//----------------------------------------------------------------------------
void pqDSMViewerPanel::StartRemovingServer(pqServer *server)
{
  if (this->Internals->DSMProxyCreated()) {
    this->Internals->DSMProxy->InvokeCommand("DestroyDSM");
    this->Internals->DSMProxy = NULL;
    this->Internals->DSMInitialized = 0;
  }
  delete this->Internals->pqObjectInspector;
  delete this->Internals->pqDSMProxyHelper;
  this->Internals->pqObjectInspector = NULL;
  this->Internals->pqDSMProxyHelper  = NULL;
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onActiveViewChanged(pqView* view)
{
  pqRenderView* renView = qobject_cast<pqRenderView*>(view);
  this->Internals->ActiveView = renView;
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
    // Create GUI controls from template
    //
    if (!this->Internals->xdmfFilePathLineEdit->text().isEmpty()) {
      this->ParseXMLTemplate(this->Internals->xdmfFilePathLineEdit->text().toStdString().c_str());
    }
  }
  return this->Internals->DSMInitialized;
}
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

  GridMap &steeringConfig = this->Internals->SteeringParser->GetSteeringConfig();
  std::string gridname = item->data(0, 1).toString().toStdString();
  std::string attrname = item->text(0).toStdString();

  pqSMAdaptor::setElementProperty(
    this->Internals->DSMProxy->GetProperty("DisabledObject"),
    "Modified");
  if (!item->parent()) {
    pqSMAdaptor::setElementProperty(
        this->Internals->DSMProxy->GetProperty("DisabledObject"),
        attrname.c_str());
    this->Internals->DSMProxy->UpdateVTKObjects();
  } else {
    pqSMAdaptor::setElementProperty(
        this->Internals->DSMProxy->GetProperty("DisabledObject"),
        steeringConfig[gridname].attributeConfig[attrname].hdfPath.c_str());
    this->Internals->DSMProxy->UpdateVTKObjects();
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::ChangeItemState(QTreeWidgetItem *item)
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
        "Modified");
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
    const char *steeringCmd = "disk";

    pqSMAdaptor::setElementProperty(
        this->Internals->DSMProxy->GetProperty("SteeringCommand"),
        "Modified");

    pqSMAdaptor::setElementProperty(
        this->Internals->DSMProxy->GetProperty("SteeringCommand"),
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
    XdmfWriter->SetConnectionID(pqActiveObjects::instance().activeServer()->GetConnectionID());

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
void pqDSMViewerPanel::RunScript() 
{
  std::string scriptname = this->Internals->scriptPath->text().toStdString();
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onWriteSteeringDataToDSM()
{
  if (this->DSMReady()) {
    if (!this->Internals->ActiveSourceProxy) {
      vtkGenericWarningMacro(<<"Nothing to Write");
      return;
    }
    //
    vtkSMProxyManager* pm = vtkSMProxy::GetProxyManager();
    vtkSmartPointer<vtkSMSourceProxy> SteeringWriter =
      vtkSMSourceProxy::SafeDownCast(pm->NewProxy("icarus_helpers", "SteeringWriter"));
    SteeringWriter->SetConnectionID(pqActiveObjects::instance().activeServer()->GetConnectionID());

    // Delete our reference now and let smart pointer clean up later
    SteeringWriter->UnRegister(NULL);

    pqSMAdaptor::setProxyProperty(
        SteeringWriter->GetProperty("DSMManager"), this->Internals->DSMProxy);

    pqSMAdaptor::setElementProperty(
        SteeringWriter->GetProperty("GroupPath"), "/PartType1");

    pqSMAdaptor::setInputProperty(
        SteeringWriter->GetProperty("Input"),
      this->Internals->ActiveSourceProxy,
      this->Internals->ActiveSourcePort
      );

    SteeringWriter->UpdateVTKObjects();
    SteeringWriter->UpdatePipeline();
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
void pqDSMViewerPanel::onautoSaveImageChecked(int checked) {
  if (checked) {
    SaveSnapshot();
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onDSMUpdateInformation()
{
  double range[2] = { -1.0, -1.0 };
  vtkSMPropertyHelper timerange(this->Internals->DSMProxyHelper, "TimeRangeInfo");
  timerange.UpdateValueFromServer();
  timerange.Get(range,2);
  if (range[0]!=-1.0 && range[1]!=-1.0) {
    // Increment the time as new steps appear.
    // @TODO : To be replaced with GetTimeStep from reader
    // @Warning : We must set any new time on the main paraview animation timekeeper before triggering
    // updates, otherwise we get mistmatched 'UpdateTime' messages which can in turn trigger erroneous updates later
    QList<pqAnimationScene*> scenes = pqApplicationCore::instance()->getServerManagerModel()->findItems<pqAnimationScene *>();
    foreach (pqAnimationScene *scene, scenes) {
      pqTimeKeeper* timekeeper = scene->getServer()->getTimeKeeper();
      vtkSMProxy *tkp = timekeeper->getProxy();
      if (tkp && tkp->IsA("vtkSMTimeKeeperProxy")) {
        vtkSMPropertyHelper tr2(timekeeper->getProxy(), "TimeRange");
        tr2.Set(range,2);
      }
    }
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onDSMUpdatePipeline()
{
  bool force_generate     = false;
  static int current_time = 0;
  static std::string xdmf_description_file_path = this->Internals->xdmfFilePathLineEdit->text().toStdString();
  //
#ifdef DISABLE_DISPLAY
  if (this->DSMReady()) {
    this->Internals->DSMProxy->InvokeCommand("H5DumpLight");
  }
#else
  //
  vtkSMProxyManager *pm = vtkSMProxy::GetProxyManager();
  if (!this->Internals->XdmfViewer || this->Internals->storeDSMContents->isChecked()) {
    // set create objects flag
    this->Internals->CreateObjects = true;
    vtkCustomPipelineHelper::RegisterCustomFilters();

    //
    // Create a new Reader+ExtractBlock proxy and register it with the system
    //
    this->Internals->XdmfViewer = vtkCustomPipelineHelper::New("sources", "XdmfReaderBlock");

    //
    // Connect our DSM manager to the reader
    //
    pqSMAdaptor::setProxyProperty(
      this->Internals->XdmfViewer->Pipeline->GetProperty("DSMManager"), this->Internals->DSMProxy
    );
    this->Internals->XdmfViewer->Pipeline->UpdateProperty("DSMManager");
  }

  //
  // If we are using an Xdmf XML file supplied manually or generated, get it
  //
  if (!this->Internals->xdmfFilePathLineEdit->text().isEmpty()) {
    if (this->Internals->xdmfFileTypeComboBox->currentIndex() == XML_USE_ORIGINAL) {
      pqSMAdaptor::setElementProperty(
        this->Internals->XdmfViewer->Pipeline->GetProperty("FileName"), 
        this->Internals->xdmfFilePathLineEdit->text().toStdString().c_str());
      this->Internals->XdmfViewer->Pipeline->UpdateProperty("FileName");
    }
    if (this->Internals->xdmfFileTypeComboBox->currentIndex() == XML_USE_TEMPLATE) {
      force_generate = this->Internals->forceXdmfGeneration->isChecked();
      // Only re-generate if the description file path has changed or if force is set to true
      if ((xdmf_description_file_path != this->Internals->xdmfFilePathLineEdit->text().toStdString()) 
        || this->Internals->CreateObjects || force_generate) 
      {
        xdmf_description_file_path = this->Internals->xdmfFilePathLineEdit->text().toStdString();
        // Generate xdmf file for reading
        // Send the whole string representing the DOM and not just the file path so that the 
        // template file does not to be present on the server anymore
        pqSMAdaptor::setElementProperty(
            this->Internals->DSMProxy->GetProperty("XMFDescriptionFilePath"),
            this->Internals->SteeringParser->GetConfigDOM()->Serialize());

        this->Internals->DSMProxy->UpdateVTKObjects();
        this->Internals->DSMProxy->InvokeCommand("GenerateXMFDescription");
      }
    }
  }
  else {
    // The base class requires a non NULL filename, (we use stdin for DSM)
    pqSMAdaptor::setElementProperty(
        this->Internals->XdmfViewer->Pipeline->GetProperty("FileName"), "stdin"); 
    this->Internals->XdmfViewer->Pipeline->UpdateProperty("FileName");
  }

  //
  // We need to know if the XdmfReader output is multiblock or not
  // first update the reader
  //
  if (this->Internals->CreateObjects) {
    bool multiblock = false;
    vtkSMCompoundSourceProxy *pipeline1 = vtkSMCompoundSourceProxy::SafeDownCast(
      this->Internals->XdmfViewer->Pipeline);
    this->Internals->XdmfReader = vtkSMSourceProxy::SafeDownCast(pipeline1->GetProxy("XdmfReader1"));
    this->Internals->XdmfReader->UpdatePipeline();
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

    //
    // Trigger complete update of reader/ExtractBlock/etc pipeline
    //
    this->Internals->XdmfViewer->UpdateAll();

    //
    // Registering the proxy as a source will create a pipeline source in the browser
    // temporarily disable error messages to squash one warning about Input being
    // declared but ot registered with the pipeline browser.
    //
    char proxyName[256];
    sprintf(proxyName, "DSM-Data-%d", current_time);
    pqApplicationCore::instance()->disableOutputWindow();
    pm->RegisterProxy("sources", proxyName, this->Internals->XdmfViewer->PipelineEnd);
    pqApplicationCore::instance()->enableOutputWindow();

    //
    // Set status of registered pipeline source to unmodified 
    //
    pqPipelineSource* source = pqApplicationCore::instance()->
      getServerManagerModel()->findItem<pqPipelineSource*>(this->Internals->XdmfViewer->PipelineEnd);
    source->setModifiedState(pqProxy::UNMODIFIED);

    //
    // (on First creation), make the output of the pipeline source visible.
    //
    pqDisplayPolicy* display_policy = pqApplicationCore::instance()->getDisplayPolicy();
    pqOutputPort *port = source->getOutputPort(0);
    display_policy->setRepresentationVisibility(port, pqActiveObjects::instance().activeView(), 1);
    //
    vtkCustomPipelineHelper::UnRegisterCustomFilters();
  }
  else {
    this->Internals->XdmfReader->InvokeCommand("Modified");
    this->Internals->XdmfViewer->UpdateAll();
    // Update all widget pipelines
    for (PipelineList::iterator w=this->Internals->WidgetPipelines.begin(); 
      w!=this->Internals->WidgetPipelines.end(); ++w)
    {
      (*w)->UpdateAll();
    }
  }
  //
  this->Internals->CreateObjects = false;

  // this->Internals->DSMProxy->InvokeCommand("H5DumpLight");
  
  double tval[1] = { -1.0 };
  vtkSMPropertyHelper time(this->Internals->DSMProxyHelper, "TimeInfo");
  time.UpdateValueFromServer();
  time.Get(tval,1);
  if (tval[0]!=-1.0) {
    // Increment the time as new steps appear.
    // @TODO : To be replaced with GetTimeStep from reader
    // @Warning : We must set any new time on the main paraview animation timekeeper before triggering
    // updates, otherwise we get mistmatched 'UpdateTime' messages which can in turn trigger erroneous updates later
    QList<pqAnimationScene*> scenes = pqApplicationCore::instance()->getServerManagerModel()->findItems<pqAnimationScene *>();
    foreach (pqAnimationScene *scene, scenes) {
//      this->Internals->CurrentTimeStep = ++current_time;
      pqTimeKeeper* timekeeper = scene->getServer()->getTimeKeeper();
      timekeeper->setTime(tval[0]);
      scene->setAnimationTime(tval[0]);
//      QPair<double, double> trange = timekeeper->getTimeRange();
//      pqSMAdaptor::setElementProperty(scene->getProxy()->GetProperty("AnimationTime"), current_time);
//      scene->getProxy()->UpdateProperty("AnimationTime");
    }
  } else {
      QList<pqAnimationScene*> scenes = pqApplicationCore::instance()->getServerManagerModel()->findItems<pqAnimationScene *>();
      foreach (pqAnimationScene *scene, scenes) {
        scene->setAnimationTime(++current_time);
        this->Internals->CurrentTimeStep = current_time;
      }
  }
  //
  // Trigger a render : if changed, everything should update as usual
  if (pqActiveObjects::instance().activeView())
  {
    pqActiveObjects::instance().activeView()->render();
    if (this->Internals->autoSaveImage->isChecked()) {
      this->SaveSnapshot();
    }
    if (this->Internals->runScript->isChecked()) {
      this->RunScript();
    }
  }
#endif //DISABLE_DISPLAY
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
      
      //
      // make sure the field arrays are propagated to our panel
      //
      if (this->Internals->DSMProxyHelper) {
        vtkSMProperty *ip = this->Internals->DSMProxyHelper->GetProperty("Input");
        pqSMAdaptor::setInputProperty(
          ip,
          this->Internals->ActiveSourceProxy,
          this->Internals->ActiveSourcePort
        );
        // This updates the ArrayListDomain domain 
        ip->UpdateDependentDomains();
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
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onUpdateTimeout()
{
  // make sure we only get one message at a time.
  this->UpdateTimer->stop();

  // we don't want to create anything just to inspect it, so test flags only
  if (this->Internals->DSMProxyCreated() && this->Internals->DSMInitialized) {
    if (this->DSMReady()) {
      vtkSMPropertyHelper ur(this->Internals->DSMProxy, "DsmUpdateReady");
      ur.UpdateValueFromServer();
      if (ur.GetAsInt() != 0) {
        this->Internals->DSMProxy->InvokeCommand("ClearDsmUpdateReady");
        //
        vtkSMPropertyHelper ig(this->Internals->DSMProxy, "DsmUpdateLevel");
        ig.UpdateValueFromServer();
        std::cout << "DSM Received Update : ";
        if (ig.GetAsInt()==0) {
          std::cout << "Information" << std::endl;;
          this->onDSMUpdateInformation();
        }
        else if (ig.GetAsInt()>=1) {
          std::cout << "Pipeline" << std::endl;;
          vtkSMPropertyHelper dm(this->Internals->DSMProxy, "DsmDataIsModified");
          dm.UpdateValueFromServer();
          if (this->Internals->autoDisplayDSM->isChecked() && dm.GetAsInt()) {
            this->onDSMUpdatePipeline();
            this->Internals->DSMProxy->InvokeCommand("ClearDsmDataIsModified");
          }
        }
        else {
          std::cout << "Update level " << ig.GetAsInt() << " not yet supported, please check simulation code " << std::endl;;
        }
//        this->onWriteSteeringDataToDSM();
        std::cout << "Update complete : calling RequestRemoteChannel " << std::endl;
        this->Internals->DSMProxy->InvokeCommand("RequestRemoteChannel");
      }
    }
  }

  // restart the timer before we exit
  this->UpdateTimer->start();
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::propModified()
{
  int x = 0;
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::testClicked()
{
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::test2Clicked()
{
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::BindWidgetToGrid(const char *propertyname, SteeringGUIWidgetInfo *info, int blockindex)
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
  vtkSMProxyManager *pm = vtkSMProxy::GetProxyManager();
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
  // Note that the 3Dwidget was created automatically by the pqObjectInspectorWidget when the XML
  // was parsed, and this was before any actual filters were created
  //
  vtkSMProxyProperty *transform = vtkSMProxyProperty::SafeDownCast(
    widgetPipeline->Pipeline->GetProperty("Transform"));
  vtkSMPropertyLink* link = vtkSMPropertyLink::New();
  link->AddLinkedProperty(this->Internals->DSMProxyHelper, propertyname, vtkSMPropertyLink::INPUT);
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
  vtkSMProxy *p = info->ControlledProxy;
  if (info->AbstractWidget->IsA("vtkBoxWidget2")) {
    vtkBoxWidget2::SafeDownCast(info->AbstractWidget)->SetRotationEnabled(0);
  }
}
//-----------------------------------------------------------------------------
