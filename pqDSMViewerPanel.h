#ifndef _pqDSMViewerPanel_h
#define _pqDSMViewerPanel_h

#include <QDockWidget>

#include <vtkSmartPointer.h>

// For communicator selection, has to match XdmfDsmComm.h
#define XDMF_DSM_COMM_MPI 0x10
#define XDMF_DSM_COMM_SOCKET 0x11

class pqServer;
class pqView;
class QTimer;

class vtkSMSourceProxy;
class vtkSMRepresentationProxy;

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

  void onUpdateTimeout();

  void onPublishDSM();
  void onUnpublishDSM();

  void onAutoDisplayDSM();
  void onDSMWriteDisk();

  void onSCRestart();
  void onSCPause();
  void onSCPlay();

  void onDisplayDSM();
  void TrackSource();

  void LoadSettings();
  void SaveSettings();

private slots:

protected:
  /// populate widgets with properties from the server manager
  virtual void LinkServerManagerProperties();

  class pqUI;
  pqUI* UI;
  bool             Connected;
  int              DSMCommType;
  QTimer          *UpdateTimer;

  vtkSmartPointer<vtkSMSourceProxy>         XdmfReader;
  vtkSmartPointer<vtkSMRepresentationProxy> XdmfRepresentation;

protected slots:

};

#endif

