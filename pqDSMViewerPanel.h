#ifndef _pqDSMViewerPanel_h
#define _pqDSMViewerPanel_h

#include <QDockWidget>

class QCheckBox;
class QComboBox;
class QPushButton;
class pqServer;
class QTreeWidgetItem;

class pqDSMViewerPanel : public QDockWidget
{
  Q_OBJECT

public:
  /// constructor
  pqDSMViewerPanel(QWidget* p = NULL);
 ~pqDSMViewerPanel();

  bool ProxyReady();

signals:

public slots:
  void onserverAdded(pqServer *server);
  void startRemovingServer(pqServer *server);
  void onCreateDSM();
  void onDestroyDSM();
  void onTestDSM();
  void onQueryDSM();
  void TrackSource();


private slots:

protected:
  /// populate widgets with properties from the server manager
  virtual void linkServerManagerProperties();

  class pqUI;
  pqUI* UI;

protected slots:

};

#endif

