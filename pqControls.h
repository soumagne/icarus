#ifndef __pqControls_h
#define __pqControls_h

#include <QDockWidget>

class pqDataRepresentation;
class pqRenderView;
class pqPipelineSource;
class pqPropertyLinks;
class QCheckBox;
class QComboBox;
class QPushButton;
class QSpinBox;

class pqControls : public QDockWidget
{
  Q_OBJECT
public:
  pqControls(QWidget* p);
  ~pqControls();

public slots:

private slots:

  //changes widgets to reflect the currently active representation and view
  void updateTrackee();
  void connectToRepresentation(pqPipelineSource*, pqDataRepresentation*);

private:

  pqDataRepresentation *currentRep;
  pqRenderView *currentView;

  pqPropertyLinks *propertyLinks;

  class pqInternals;
  pqInternals* Internals;
};

#endif // __pqControls_h
