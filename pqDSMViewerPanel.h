#ifndef _pqDSMViewerPanel_h
#define _pqDSMViewerPanel_h

#include <QDockWidget>

// For communicator selection, has to match XdmfDsmComm.h
#define XDMF_DSM_COMM_MPI 0x10
#define XDMF_DSM_COMM_SOCKET 0x11

class QCheckBox;
class QComboBox;
class QPushButton;
class pqServer;
class QTreeWidgetItem;
class QProgressDialog;
class QTimer;

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
  void StartRemovingServer(pqServer *server);

  void onBrowseFile();

  void onCreateDSM();
  void onDestroyDSM();
  void onClearDSM();

  void onConnectDSM();
  void onDisconnectDSM();

  void CreatePublishNameDialog();
  void TimeoutPublishName();
  void CancelPublishNameDialog();
  void onPublishDSM();
  void onUnpublishDSM();

  void onH5Dump();
  void onTestDSM();
  void onDisplayDSM();
  void TrackSource();
  void FillDSMContents(QTreeWidgetItem *item, int node);

private slots:

protected:
  /// populate widgets with properties from the server manager
  virtual void LinkServerManagerProperties();

  class pqUI;
  pqUI* UI;
  QProgressDialog *PublishNameDialog;
  QTimer          *PublishNameTimer;
  int              PublishNameSteps;
  bool             PublishedNameFound;
  bool             ConnectionFound;
  QTreeWidgetItem *DSMContentTree;
  int              DSMCommType;

protected slots:

};

#endif

