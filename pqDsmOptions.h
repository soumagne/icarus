/*=========================================================================

  Project                 : Icarus
  Module                  : pqDsmOptions.h

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
#ifndef _pqDsmOptions_h
#define _pqDsmOptions_h

#include <QDockWidget>
#include <QMutex>
#include <set>

// Core Qt 
class QTreeWidgetItem;
// Servermanager and views
class pqServer;
class pqView;
class vtkSMSourceProxy;

struct SteeringGUIWidgetInfo;

class pqDsmOptions : public QDockWidget
{
  Q_OBJECT

public:
  /// constructor
  pqDsmOptions(QWidget* p = NULL);
  virtual ~pqDsmOptions();

  bool DsmProxyReady();
  bool DsmReady();
  bool DsmListening();
  void DsmInitFunction();

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

  // triggered by updates from simulation
  void UpdateDsmInformation();
  void UpdateDsmPipeline();
  void UpdateDsmStatus(QString status);
  void onPreAccept();
  void onPostAccept();

  void BindWidgetToGrid(const char *propertyname, SteeringGUIWidgetInfo *info, int blockindex);

private slots:

protected:
  /// Load/Save settings from paraview ini/config file
  void LoadSettings();
  void SaveSettings();
  //
  void CreateXdmfPipeline();
  void UpdateXdmfInformation();
  void UpdateXdmfPipeline();
  void UpdateXdmfTemplate();
  //
  void CreateH5PartPipeline();
  void UpdateH5PartInformation();
  void UpdateH5PartPipeline();
  //
  void CreateNetCDFPipeline();
  void UpdateNetCDFPipeline();
  void UpdateNetCDFInformation();
  //
  void CreateTablePipeline();
  void UpdateTableInformation();
  void UpdateTablePipeline();
  //
  void CreateBonsaiPipeline();
  void UpdateBonsaiInformation();
  void UpdateBonsaiPipeline();
  //
  void ShowPipelineInGUI(vtkSMSourceProxy *source, const char *name, int Id);
  void GetPipelineTimeInformation(vtkSMSourceProxy *source);
  void SetTimeAndRange(double range[2], double timenow, bool GUIupdate=false);
  void GetViewsForPipeline(vtkSMSourceProxy *source, std::set<pqView*> &viewlist);

  /// Generate objects for steering
  void ParseXMLTemplate(const char *filepath);
  void DeleteSteeringWidgets();

  /// Control of blocks
  void ChangeItemState(QTreeWidgetItem *item);
  void onQuickSync(bool b);

  class pqInternals;
  pqInternals *Internals;
  QMutex DSMLocked;

protected slots:

};

#endif

