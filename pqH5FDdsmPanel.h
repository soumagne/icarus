/*=========================================================================

  Project                 : Icarus
  Module                  : pqH5FDdsmPanel.h

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
#ifndef _pqH5FDdsmPanel_h
#define _pqH5FDdsmPanel_h

#include <QWidget>
#include <QMutex>
#include <set>
#ifndef ICARUS_NO_LIBCXX
  #include <functional>
  #define icarus_std std
#else
  #include <boost/function.hpp>
  #include <boost/bind.hpp>
  #define icarus_std boost
  #define nullptr NULL
#endif

#include "vtkSmartPointer.h"
// Core Qt 
class QTreeWidgetItem;
// Servermanager and views
class pqServer;
class pqView;
class vtkSMProxy;
class vtkSMSourceProxy;

struct SteeringGUIWidgetInfo;

class pqH5FDdsmPanel : public QWidget
{
  Q_OBJECT

public:
  /// constructor
  pqH5FDdsmPanel(QWidget* p = NULL);
  virtual ~pqH5FDdsmPanel();

  bool DsmProxyReady();
  bool DsmReady();
  bool DsmListening();
  bool DsmInitialized();

  vtkSmartPointer<vtkSMProxy> CreateDsmProxy();
  vtkSmartPointer<vtkSMProxy> CreateDsmHelperProxy();

  /// Load/Save settings from paraview ini/config file
  void LoadSettings();
  void SaveSettings();

  void setInitializationFunction(icarus_std::function<void()> &f) { 
    this->initCallback = f; 
  }

public slots:
  // Server Add/Remove notifications
  void onServerAdded(pqServer *server);
  void onStartRemovingServer(pqServer *server);

  // Enable / Disable setting blocks
  void onLockSettings(int state);

  // Enable/Disable DSM operation (server listening)
  void onPublish();
  void onUnpublish();

  // User actions to edit DSM server locations
  void onAddDsmServer();

  // Sending data to DSM
  void onWriteDataToDSM();

  void onNewNotificationSocket();

  // triggered by updates from simulation
  void onNotified();
  void onPreAccept();
  void onPostAccept();
  void onPause();
  void onPlay();

  void TrackSource();

public:
signals:

  void UpdateInformation();
  void UpdateData();
  void UpdateStatus(QString status);

protected:

  icarus_std::function<void()> initCallback;
  class pqInternals;
  pqInternals *Internals;
  QMutex DSMLocked;

protected slots:

};

#endif

