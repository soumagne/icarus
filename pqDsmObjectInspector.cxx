/*=========================================================================

  Project                 : Icarus
  Module                  : pqDsmObjectInspector.cxx

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
#include "pqDsmObjectInspector.h"
#include "pqApplyPropertiesManager.h"
#include "pqApplicationCore.h"
#include "pqActiveObjects.h"

#include <QPushButton>
//----------------------------------------------------------------------------
pqDsmObjectInspector::pqDsmObjectInspector(QWidget *parent) : pqPropertiesPanel(parent)
{
  /// Set the panel mode.
  this->setPanelMode(SOURCE_PROPERTIES);

  //---------------------------------------------------------------------------
  // Listen to various signals from the application indicating changes in active
  // source/view/representation, etc.
  pqActiveObjects *activeObjects = &pqActiveObjects::instance();
  this->disconnect(activeObjects, SIGNAL(portChanged(pqOutputPort*)),
                this, SLOT(setOutputPort(pqOutputPort*)));
  this->disconnect(activeObjects, SIGNAL(viewChanged(pqView*)),
                this, SLOT(setView(pqView*)));
  this->disconnect(activeObjects, SIGNAL(representationChanged(pqDataRepresentation*)),
                this, SLOT(setRepresentation(pqDataRepresentation*)));
}

//----------------------------------------------------------------------------
pqDsmObjectInspector::~pqDsmObjectInspector()
{
}
//----------------------------------------------------------------------------
void pqDsmObjectInspector::updatePropertiesPanel(pqPipelineSource* source)
{
  this->pqPropertiesPanel::updatePropertiesPanel(source);
}
/*
//----------------------------------------------------------------------------
void pqDsmObjectInspector::accept()
{
  emit this->preaccept();
  this->pqObjectInspectorWidget::accept();
  emit this->postaccept();
}
*/