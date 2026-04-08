#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QHeaderView>
#include <QString>
#include "core/commandregistry.h"
#include "core/telemetryparser.h"
#include <QDateTime>
MainWindow::MainWindow(QWidget *parent) // 4/9
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , comms(new CommunicationManager(this))
    , commandHistoryModel(new CommandHistoryModel(this))
    , pendingCommandModel(new PendingCommandModel(this))
    , pendingCommandTimer(new QTimer(this))
{
    ui->setupUi(this);
    connect(ui->buttonSendCommand, &QPushButton::clicked,
            this, &MainWindow::onSendCommandClicked);

    connect(pendingCommandTimer, &QTimer::timeout,
            this, &MainWindow::processPendingCommands);

    pendingCommandTimer->start(1000);

    loadCommands();
    ui->tableCommandHistory->setModel(commandHistoryModel);
    ui->tablePendingCommands->setModel(pendingCommandModel);

    connect(comms, &CommunicationManager::telemetryReceived,
            this, &MainWindow::onTelemetryReceived);

    connect(comms, &CommunicationManager::statusChanged,
            this, &MainWindow::onStatusChanged);
    connect(ui->buttonParsePacketBits, &QPushButton::clicked,
            this, &MainWindow::onParsePacketBitsClicked);
    connect(ui->buttonGenerateExamplePacket, &QPushButton::clicked,
            this, &MainWindow::onGenerateExamplePacketClicked);
    connect(comms, &CommunicationManager::rawLineReceived,
            this, &MainWindow::onRawLineReceived);

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

    //comms->startSimulation(1000);

    nextCommandSeq = 5;


    pendingCommandCountdowns[1] = 5;
    pendingCommandCountdowns[4] = 3;


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



void MainWindow::onSendCommandClicked()
{
    int index = ui->comboBoxCommand->currentIndex();

    if (index < 0 || index >= availableCommands.size()) {
        ui->labelCommandResult->setText("No command selected.");
        return;
    }

    const CommandDefinition &selected = availableCommands[index];
    QString parameterText = ui->lineEditCommandParam->text().trimmed();

    QString fullCommandText = selected.commandKey;

    if (selected.requiresParameter) {
        if (parameterText.isEmpty()) {
            ui->labelCommandResult->setText(
                QString("Command '%1' requires a parameter.")
                    .arg(selected.displayName)
                );
            return;
        }
        fullCommandText += " " + parameterText;
    }

    // 1. Add to Command History
    CommandRecord record;
    record.time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    record.command = fullCommandText;
    record.cmdSeq = nextCommandSeq;
    record.status = "QUEUED";
    record.result = "Pending send";
    commandHistoryModel->addRecord(record);

    // 2. Add to Pending Commands
    PendingCommand cmd;
    cmd.cmdSeq = nextCommandSeq;
    cmd.command = fullCommandText;
    cmd.state = "WAITING_ACK";
    cmd.retriesLeft = selected.defaultRetries;
    cmd.timeoutRemainingS = selected.defaultTimeoutS;
    cmd.timeoutS = selected.defaultTimeoutS;
    cmd.lastTx = QDateTime::currentDateTime().toString("hh:mm:ss");
    pendingCommandModel->addPendingCommand(cmd);

    // 3. Add countdown for fake ACK/timeout simulation
    pendingCommandCountdowns[nextCommandSeq] = selected.defaultTimeoutS;

    // 4. Update UI feedback
    ui->labelCommandResult->setText(
        QString("Queued command '%1' with CmdSeq %2")
            .arg(fullCommandText)
            .arg(nextCommandSeq)
        );

    // 5. Clear parameter input
    ui->lineEditCommandParam->clear();

    // 6. Increment sequence
    nextCommandSeq++;
}

void MainWindow::processPendingCommands()
{
    QList<quint32> keys = pendingCommandCountdowns.keys();

    for (quint32 cmdSeq : keys) {
        int remaining = pendingCommandCountdowns.value(cmdSeq);
        remaining--;

        if (remaining > 0) {
            pendingCommandCountdowns[cmdSeq] = remaining;
            pendingCommandModel->updatePendingCommand(cmdSeq, "WAITING_ACK", 3, remaining);
        } else {
            // Fake simulation rule:
            // even cmdSeq -> ACKED
            // odd cmdSeq  -> TIMEOUT
            if (cmdSeq % 2 == 0) {
                commandHistoryModel->updateRecordStatus(cmdSeq, "ACKED", "Simulated ACK received");
            } else {
                commandHistoryModel->updateRecordStatus(cmdSeq, "TIMEOUT", "Simulated timeout");
            }

            pendingCommandModel->removePendingCommand(cmdSeq);
            pendingCommandCountdowns.remove(cmdSeq);
        }
    }
}




void MainWindow::onParsePacketBitsClicked()
{
    QString inputText = ui->plainTextPacketBits->toPlainText().trimmed();

    if (inputText.isEmpty()) {
        ui->plainTextPacketDecoded->setPlainText("Packet parsing failed:\nInput is empty");
        return;
    }

    QByteArray packetBytes;
    QString error;

    if (!TelemetryParser::textToPacketBytes(inputText, packetBytes, error)) {
        ui->plainTextPacketDecoded->setPlainText("Packet parsing failed:\n" + error);
        return;
    }

    QString description = TelemetryParser::describePacket(packetBytes);
    ui->plainTextPacketDecoded->setPlainText(description);

    // Feed into stream-based comms path so GUI behaves like real downlink handling
    comms->feedRawPacket(packetBytes);
}

void MainWindow::onRawLineReceived(const QString& line)
{
    ui->plainTextRawPacketLog->appendPlainText(line);
}




void MainWindow::onGenerateExamplePacketClicked()
{
    TelemetryFrame frame;
    frame.valid = true;
    frame.sequence = 123;
    frame.timestamp = QDateTime::currentDateTimeUtc();

    frame.batteryVoltage = 12.34;
    frame.altitude = 1520.5;
    frame.temperature = 24.75;
    frame.latitude = -37.9100000;
    frame.longitude = 145.1300000;
    frame.cpuUsage = 18.5;
    frame.ramUsage = 42.0;
    frame.packetErrorRate = 1.2;
    frame.fault = false;
    frame.faultCode = 0;
    frame.faultText = "NONE";

    QByteArray packet = TelemetryParser::buildTelemetryPacket(frame, 0x01, 0x10);


    ui->plainTextPacketBits->setPlainText(packet.toHex(' ').toUpper());


    ui->plainTextPacketDecoded->setPlainText(
        "Generated valid example packet.\n\n" +
        TelemetryParser::describePacket(packet)
        );

    statusBar()->showMessage("Valid example packet generated");
}
