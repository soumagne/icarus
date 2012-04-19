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
#include <QInputDialog>
#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QUrl>
#include <QDesktopServices>
#include <QTime>
#include <QTcpServer>
#include <QTcpSocket>

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
#include "vtkSMProxyDefinitionManager.h"
#include "vtkSMProxySelectionModel.h"
#include "vtkSMSessionProxyManager.h"

#include "vtkPVDataInformation.h"
#include "vtkPVCompositeDataInformation.h"
#include "vtkProcessModule.h"
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
#include "pqTimeKeeper.h"
#include "pqApplyPropertiesManager.h"
//
#include "pqObjectInspectorWidget.h"
#include "pqNamedWidgets.h"
#include "pq3DWidget.h"
#include "pqDataExportWidget.h"
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
#include "ui_pqDsmViewerPanel.h"
//
#include "vtkDsmManager.h"
#include "H5FDdsm.h"
#include "XdmfSteeringParser.h"
#include "vtkCustomPipelineHelper.h"
#include "vtkSMCommandProperty.h"
//
#include <vtksys/SystemTools.hxx>
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
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
//  emit this->preaccept();
  this->pqObjectInspectorWidget::accept();
  emit this->postaccept();
}
