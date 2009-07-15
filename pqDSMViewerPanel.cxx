#include "pqDSMViewerPanel.h"

// Qt includes
#include <QTreeWidget>
#include <QVariant>
#include <QLabel>
#include <QComboBox>
#include <QTableWidget>

// VTK includes

// ParaView Server Manager includes
#include "vtkSMProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMStringVectorProperty.h"
#include "vtkSMArraySelectionDomain.h"
#include "pqActiveServer.h"

// ParaView includes
#include "pqApplicationCore.h"
#include "pqPropertyLinks.h"
#include "pqProxy.h"
#include "pqServer.h"
#include "pqServerManagerSelectionModel.h"
#include "pqServerManagerModelItem.h"
#include "pqServerManagerModel.h"
#include "pqSMAdaptor.h"
#include "pqTreeWidgetCheckHelper.h"
#include "pqTreeWidgetItemObject.h"
#include "ui_pqDSMViewerPanel.h"

class pqDSMViewerPanel::pqUI : public QObject, public Ui::DSMViewerPanel {
public:
  pqUI(pqDSMViewerPanel* p) : QObject(p)
  {
    this->Links = new pqPropertyLinks;
  }
  //
  ~pqUI() {
    delete this->Links;
  }
  //
  void CreateProxy() {
    vtkSMProxyManager* pm = vtkSMProxy::GetProxyManager();
    DSMProxy = pm->NewProxy("icarus_helpers", "DSMManager");
    this->DSMProxy->UpdatePropertyInformation();
  }
  //
  bool ProxyCreated() { return this->DSMProxy!=NULL; }
  pqPropertyLinks            *Links;
  vtkSmartPointer<vtkSMProxy> DSMProxy;
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

  this->connect(this->UI->Query,
    SIGNAL(clicked()), this, SLOT(onQueryDSM()));

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
  QObject::connect(selection, 
                   SIGNAL(currentChanged(pqServerManagerModelItem*)), 
                   this, SLOT(updateTrackee())
                   );
  pqServerManagerModel * model =
      pqApplicationCore::instance()->getServerManagerModel();
  QObject::connect(model,
                   SIGNAL(sourceAdded(pqPipelineSource*)),
                   this, SLOT(updateTrackee()));
  QObject::connect(model,
                   SIGNAL(sourceRemoved(pqPipelineSource*)),
                   this, SLOT(updateTrackee()));

}
//----------------------------------------------------------------------------
pqDSMViewerPanel::~pqDSMViewerPanel()
{
}
//----------------------------------------------------------------------------
void pqDSMViewerPanel::onserverAdded(pqServer *server)
{
  if (!this->UI->ProxyCreated()) {
    this->UI->CreateProxy();
    this->linkServerManagerProperties();
  }
}
//----------------------------------------------------------------------------
void pqDSMViewerPanel::startRemovingServer(pqServer *server)
{
  if (this->UI->ProxyCreated()) {
    this->UI->DSMProxy->InvokeCommand("DestroyDSM");
    this->UI->DSMProxy = NULL;
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
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onCreateDSM()
{
  if (this->UI->ProxyCreated()) {
    this->UI->DSMProxy->InvokeCommand("CreateDSM");
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onDestroyDSM()
{
  if (this->UI->ProxyCreated()) {
    this->UI->DSMProxy->InvokeCommand("DestroyDSM");
  }
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onTestDSM()
{
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::onQueryDSM()
{
}
//-----------------------------------------------------------------------------
void pqDSMViewerPanel::updateTrackee()
{
  //break stale connections between widgets and properties
  this->Internals->Links.removeAllPropertyLinks();

  //set to a default state
  this->currentRep = NULL;
  this->currentView = NULL;

  //find the active  view
  pqView* view = pqActiveView::instance().current();
  pqRenderView* View = 
    qobject_cast<pqRenderView*>(view);
  if (!View)
    {
    this->setEnabled(false);
    return;
    }
  this->currentView = View;
  this->setEnabled(true);

  //TODO: CHANGE UI somehow

  //find the active filter
  pqServerManagerModelItem *item =
    pqApplicationCore::instance()->getSelectionModel()->currentItem();
  if (item)
    {
    pqOutputPort* opPort = qobject_cast<pqOutputPort*>(item);
    pqPipelineSource *source = opPort? opPort->getSource() : 
      qobject_cast<pqPipelineSource*>(item);
    if (source)
      {
      pqDataRepresentation* Rep = 
        source->getRepresentation(0, this->currentView);
      if (Rep)
        {
        //we have a representation that we can connect to. do so now
        this->connectToRepresentation(source, Rep);
        }
      else
        {
        //try later when there might be something to connect to
        QObject::connect(source, 
                         SIGNAL(visibilityChanged(pqPipelineSource*, 
                                                  pqDataRepresentation*)),
                         this, 
                         SLOT(connectToRepresentation(pqPipelineSource*,
                                                      pqDataRepresentation*))
                         );
        }
      }
    }
}
