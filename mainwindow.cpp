#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QHeaderView>
#include <QString>
#include "core/commandregistry.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , comms(new CommunicationManager(this))
    , commandHistoryModel(new CommandHistoryModel(this))
    , pendingCommandModel(new PendingCommandModel(this))
{
    ui->setupUi(this);
    loadCommands();
    ui->tableCommandHistory->setModel(commandHistoryModel);
    ui->tablePendingCommands->setModel(pendingCommandModel);

    connect(comms, &CommunicationManager::telemetryReceived,
            this, &MainWindow::onTelemetryReceived);

    connect(comms, &CommunicationManager::statusChanged,
            this, &MainWindow::onStatusChanged);

    comms->startSimulation(1000);


    ui->tableCommandHistory->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tablePendingCommands->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableCommandHistory->verticalHeader()->setVisible(false);
    ui->tablePendingCommands->verticalHeader()->setVisible(false);

    ui->tableCommandHistory->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tablePendingCommands->setSelectionBehavior(QAbstractItemView::SelectRows);

    ui->tableCommandHistory->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tablePendingCommands->setEditTriggers(QAbstractItemView::NoEditTriggers);

    CommandRecord record1;
    record1.time = "2026-03-20 14:30:00";
    record1.command = "shutdown";
    record1.cmdSeq = 1;
    record1.status = "QUEUED";
    record1.result = "OK";
    commandHistoryModel->addRecord(record1);

    CommandRecord record2;
    record2.time = "2026-03-20 15:00:05";
    record2.command = "ping";
    record2.cmdSeq = 2;
    record2.status = "ACKED";
    record2.result = "ACK received";
    commandHistoryModel->addRecord(record2);



// ---- test data for pending commands ----
    PendingCommand cmd1;
    cmd1.cmdSeq = 1;
    cmd1.command = "shutdown";
    cmd1.state = "WAITING_ACK";
    cmd1.retriesLeft = 3;
    cmd1.timeoutRemainingS = 5;
    cmd1.timeoutS = 5;
    cmd1.lastTx = "14:30:00";
    pendingCommandModel->addPendingCommand(cmd1);


    PendingCommand cmd2;
    cmd2.cmdSeq = 4;
    cmd2.command = "capture";
    cmd2.state = "RETRYING";
    cmd2.retriesLeft = 2;
    cmd2.timeoutRemainingS = 3;
    cmd2.timeoutS = 5;
    cmd2.lastTx = "15:00:12";
    pendingCommandModel->addPendingCommand(cmd2);

    comms->startSimulation(1000);






}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onTelemetryReceived(const TelemetryFrame& frame)
{
    ui->labelSequence->setText(QString::number(frame.sequence));
    ui->labelBattery->setText(QString::number(frame.batteryVoltage, 'f', 2) + " V");
    ui->labelAltitude->setText(QString::number(frame.altitude, 'f', 1) + " m");
    ui->labelTemperature->setText(QString::number(frame.temperature, 'f', 1) + " °C");
}

void MainWindow::onStatusChanged(const QString& status)
{
    statusBar()->showMessage(status);
}



void MainWindow::loadCommands()
{
    availableCommands = CommandRegistry::defaultCommands();

    ui->comboBoxCommand->clear();

    for (const CommandDefinition &cmd : availableCommands) {
        ui->comboBoxCommand->addItem(cmd.displayName, cmd.commandKey);
    }
}


