#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "models/commandhistorymodel.h"
#include "models/pendingcommandmodel.h"
#include <QVector>
#include "models/commanddefinition.h"

#include <QMainWindow>
#include "core/communicationmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onTelemetryReceived(const TelemetryFrame& frame);
    void onStatusChanged(const QString& status);
    void loadCommands();

private:
    Ui::MainWindow *ui;
    CommunicationManager *comms;
    CommandHistoryModel *commandHistoryModel;
    PendingCommandModel *pendingCommandModel;
    QVector<CommandDefinition> availableCommands;




};

#endif // MAINWINDOW_H