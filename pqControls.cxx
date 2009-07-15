#include <iostream>

#include "pqControls.h"

#include <QApplication>
#include <QStyle>
#include <QCheckBox>
#include <QPushButton>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QLabel>

#include "pqApplicationCore.h"
#include "pqServerManagerSelectionModel.h"
#include "pqServerManagerModelItem.h"
#include "pqServerManagerModel.h"
#include "pqRenderView.h"
#include "pqOutputPort.h"
#include "pqPipelineSource.h"
#include "pqActiveView.h"
#include "pqServer.h"
#include "pqDataRepresentation.h"
#include "pqPropertyLinks.h"
#include "vtkSMProperty.h"

class pqControls::pqInternals
{
public:
  pqPropertyLinks Links;
};


pqControls::pqControls(QWidget* p)
  : QDockWidget("Active Tracker", p)
{
  this->Internals = new pqInternals();

  this->currentView = NULL;
  this->currentRep = NULL;
  this->setEnabled(false);

  QWidget *canvas = new QWidget();
  QVBoxLayout *vLayout = new QVBoxLayout();
  canvas->setLayout(vLayout);
  this->setWidget(canvas);
  QHBoxLayout *hl;


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

  ////////////
  QLabel *label;
  label = new QLabel("View Tracked");
  label->setAlignment(Qt::AlignHCenter);
  vLayout->addWidget(label);

  label = new QLabel("Representation Tracked");
  label->setAlignment(Qt::AlignHCenter);
  vLayout->addWidget(label);
}

pqControls::~pqControls()
{
  this->Internals->Links.removeAllPropertyLinks();
  delete this->Internals;
}

void pqControls::updateTrackee()
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

//---------------------------------------------------------------------------
void pqControls::connectToRepresentation(
  pqPipelineSource* source,
  pqDataRepresentation* rep)
{
  pqDataRepresentation* Rep = 
    source->getRepresentation(0, this->currentView);
  if (!Rep)
    {
    return;
    }
  //stop watching what we were
  this->Internals->Links.removeAllPropertyLinks();
  QObject::disconnect(source, SIGNAL(visibilityChanged(pqPipelineSource*,
                                              pqDataRepresentation*)),
                     this,
                     SLOT(connectToRepresentation(pqPipelineSource*,
                                                  pqDataRepresentation*))
                     );

  this->currentRep = Rep;  

  //change UI panel to show new contents

}

