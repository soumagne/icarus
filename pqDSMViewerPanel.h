/*=========================================================================

  Project                 : Icarus
  Module                  : pqDSMViewerPanel.h

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
#ifndef _pqDSMViewerPanel_h
#define _pqDSMViewerPanel_h

#include <QDockWidget>

#include <vtkSmartPointer.h>

#include <vector>

// For communicator selection, has to match XdmfDsmComm.h
#define H5FD_DSM_COMM_MPI     0x10
#define H5FD_DSM_COMM_SOCKET  0x11
#define H5FD_DSM_COMM_MPI_RMA 0x12
#define H5FD_DSM_COMM_GNI     0x13

class pqServer;
class pqView;
class QTimer;
class QGraphicsScene;
class QTreeWidgetItem;
class QSpinBox;
class QDoubleSpinBox;
class QLabel;

class vtkSMSourceProxy;
class vtkSMRepresentationProxy;

class XdmfSteeringParser;

class pqDSMViewerPanel : public QDockWidget
{
  Q_OBJECT

public:
  /// constructor
  pqDSMViewerPanel(QWidget* p = NULL);
 ~pqDSMViewerPanel();

  bool ProxyReady();
  bool DSMReady();

signals:

public slots:
  void onServerAdded(pqServer *server);
  void onActiveServerChanged(pqServer *server);
  void StartRemovingServer(pqServer *server);
  void onActiveViewChanged(pqView* view);

  void onAddServerDSM();

  void onBrowseFile();
  void onBrowseFileImage();
  void onautoSaveImageChecked(int);
  void SaveSnapshot();

  void onUpdateTimeout();

  void onPublishDSM();
  void onUnpublishDSM();

  void onArrayItemChanged(QTreeWidgetItem*, int);

  void onSCRestart();
  void onSCPause();
  void onSCPlay();
  void onSCWriteDisk();

  // Sending data to DSM
  void onWriteDataToDSM();

  void onAdvancedControlUpdate();

  void onDisplayDSM();
  void TrackSource();

private slots:

protected:
  /// populate widgets with properties from the server manager
  virtual void LinkServerManagerProperties();

  void LoadSettings();
  void SaveSettings();

  void DeleteSteeringWidgets();

  void DescFileParse(const char *filepath);
  void ChangeItemState(QTreeWidgetItem *item);

  class pqUI;
  pqUI            *UI;
  bool             Connected;
  int              DSMCommType;
  QTimer          *UpdateTimer;
  int              CurrentTimeStep;

  XdmfSteeringParser *SteeringParser;
  std::vector<QSpinBox*> advancedControlIntScalarSpinBoxes;
  std::vector<QLabel*> advancedControlIntScalarLabels;
  std::vector<QDoubleSpinBox*> advancedControlDoubleScalarSpinBoxes;
  std::vector<QLabel*> advancedControlDoubleScalarLabels;

  // For HTM drawing
  QGraphicsScene  *HTMScene;

  vtkSmartPointer<vtkSMSourceProxy>         XdmfReader;
  vtkSmartPointer<vtkSMRepresentationProxy> XdmfRepresentation;

protected slots:

};

#endif

