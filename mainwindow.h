#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "models/commandhistorymodel.h"
#include "models/pendingcommandmodel.h"
#include <QVector>
#include "models/commanddefinition.h"

#include <QMainWindow>
#include "core/communicationmanager.h"


#include <QTimer>
#include <QMap>


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
    void onSendCommandClicked();
    void processPendingCommands();


private:
    Ui::MainWindow *ui;
    CommunicationManager *comms;
    CommandHistoryModel *commandHistoryModel;
    PendingCommandModel *pendingCommandModel;
    QVector<CommandDefinition> availableCommands;
    quint32 nextCommandSeq = 1;

    QTimer *pendingCommandTimer;
    QMap<quint32, int> pendingCommandCountdowns;


};

#endif // MAINWINDOW_H