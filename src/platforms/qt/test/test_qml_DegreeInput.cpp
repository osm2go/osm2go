#include <QQmlEngine>
#include <QtQuickTest>

class Setup : public QObject
{
  Q_OBJECT

public:
  Setup() {}

public slots:
  void qmlEngineAvailable(QQmlEngine *engine)
  {
    engine->addImportPath(QStringLiteral("qrc:///"));
  }
};

QUICK_TEST_MAIN_WITH_SETUP(DegreeInput, Setup)

#include "test_qml_DegreeInput.moc"
