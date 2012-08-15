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
pqDsmObjectInspector::pqDsmObjectInspector(QWidget* p) : pqObjectInspectorWidget(p)
{
  this->disconnect(
    &pqActiveObjects::instance(),
    SIGNAL(portChanged(pqOutputPort*)),
    this,
    SLOT(setOutputPort(pqOutputPort*)));

  // connect the apply button to the apply properties manager
  pqApplyPropertiesManager *applyPropertiesManager =
      qobject_cast<pqApplyPropertiesManager *>(
        pqApplicationCore::instance()->manager("APPLY_PROPERTIES"));

  if(applyPropertiesManager)
    {
    this->disconnect(this->AcceptButton,
                  SIGNAL(clicked()),
                  applyPropertiesManager,
                  SLOT(applyProperties()));

    this->disconnect(applyPropertiesManager,
                  SIGNAL(apply()),
                  this,
                  SLOT(accept()));
    }

    this->connect(this->AcceptButton,
                  SIGNAL(clicked()),
                  this,
                  SLOT(accept()));

    this->canAccept(false);
}
//----------------------------------------------------------------------------
pqDsmObjectInspector::~pqDsmObjectInspector()
{
}
//----------------------------------------------------------------------------
void pqDsmObjectInspector::accept()
{
  emit this->preaccept();
  this->pqObjectInspectorWidget::accept();
  emit this->postaccept();
}
