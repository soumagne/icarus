#ifndef _pqDSMViewerPanel_h
#define _pqDSMViewerPanel_h

#include <QDockWidget>

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
  void onserverAdded(pqServer *server);
  void startRemovingServer(pqServer *server);
  void onCreateDSM();
  void onDestroyDSM();
  void onClearDSM();

  void onConnectDSM();
  void onDisconnectDSM();

  void createPublishNameDialog();
  void timeoutPublishName();
  void cancelPublishNameDialog();
  void onPublishDSM();
  void onUnpublishDSM();

  void onH5Dump();
  void onTestDSM();
  void onDisplayDSM();
  void TrackSource();
  void fillDSMContents(QTreeWidgetItem *item, int node);

private slots:

protected:
  /// populate widgets with properties from the server manager
  virtual void linkServerManagerProperties();

  class pqUI;
  pqUI* UI;
  QProgressDialog *publishNameDialog;
  QTimer          *publishNameTimer;
  int              publishNameSteps;
  bool             publishedNameFound;
  bool             connectionFound;
  QTreeWidgetItem *dsmContentTree;

protected slots:

};

#endif

