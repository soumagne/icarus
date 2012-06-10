/*=========================================================================

  Project                 : Icarus
  Module                  : pqDsmViewerPanel.h

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
#ifndef _pqDsmViewerPanel_h
#define _pqDsmViewerPanel_h

#include <QDockWidget>
// Core Qt 
class QTreeWidgetItem;
// Servermanager and views
class pqServer;
class pqView;
class vtkSMSourceProxy;

struct SteeringGUIWidgetInfo;

class pqDsmViewerPanel : public QDockWidget
{
  Q_OBJECT

public:
  /// constructor
  pqDsmViewerPanel(QWidget* p = NULL);
  virtual ~pqDsmViewerPanel();

  bool DsmProxyReady();
  bool DsmReady();

signals:

public slots:
  // Server Add/Remove notifications
  void onServerAdded(pqServer *server);
  void onStartRemovingServer(pqServer *server);
  // Active View changes
  void onActiveViewChanged(pqView* view);

  // Enable / Disable setting blocks
  void onLockSettings(int state);

  // Enable/Disable DSM operation (server listening)
  void onPublish();
  void onUnpublish();

  // User actions to edit DSM server locations
  void onAddDsmServer();

  // User activated steering actions
  void onBrowseFile();
  void onBrowseFileImage();
  void onautoSaveImageChecked(int);
  void SaveSnapshot();
  void RunScript();
  void ExportData(bool force);


  void onArrayItemChanged(QTreeWidgetItem*, int);

  void onPause();
  void onPlay();

  void onWriteDataToDisk();

  // Sending data to DSM
  void onWriteDataToDSM();

  void onNewNotificationSocket();
  // triggered by updates from simulation
  void onNotified();
  void UpdateDsmInformation();
  void UpdateDsmPipeline();
  void onPreAccept();
  void onPostAccept();

  void TrackSource();

  void BindWidgetToGrid(const char *propertyname, SteeringGUIWidgetInfo *info, int blockindex);

private slots:

protected:
  /// Load/Save settings from paraview ini/config file
  void LoadSettings();
  void SaveSettings();

  void CreateXdmfPipeline();
  void CreateH5PartPipeline();
  void CreateNetCDFPipeline();
  //
  void UpdateXdmfPipeline();
  //
  void UpdateH5PartPipeline();
  //
  void UpdateNetCDFPipeline();
  void UpdateNetCDFInformation();
  //
  void UpdateXdmfTemplate();
  void ShowPipelineInGUI(vtkSMSourceProxy *source, const char *name, int Id);

  /// Generate objects for steering
  void ParseXMLTemplate(const char *filepath);
  void DeleteSteeringWidgets();

  /// Control of blocks
  void ChangeItemState(QTreeWidgetItem *item);

  class pqInternals;
  pqInternals *Internals;

protected slots:

};

#endif

