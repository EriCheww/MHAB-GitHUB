#include "communicationmanager.h"
#include "telemetryparser.h"
#include <QDateTime>

CommunicationManager::CommunicationManager(QObject *parent)
    : QObject(parent),
    simTimer(new QTimer(this))
{
    connect(simTimer, &QTimer::timeout,
            this, &CommunicationManager::generateFakeTelemetry);
}

CommunicationManager::~CommunicationManager()
{
    stopSimulation();
}

void CommunicationManager::startSimulation(int intervalMs)
{
    simTimer->start(intervalMs);
    emit statusChanged("Simulation mode started");
}

void CommunicationManager::stopSimulation()
{
    if (simTimer) {
        simTimer->stop();
    }
}

void CommunicationManager::generateFakeTelemetry()
{
    seq++;
    battery -= 0.01;
    altitude += 8.5;
    temperature -= 0.15;

    QString line = QString(
                       "SEQ=%1,TIME=%2,BAT=%3,ALT=%4,TEMP=%5,LAT=-37.910,LON=145.130,CPU=18.5,RAM=42.0,PER=1.2,FAULT=0,FAULT_TEXT=NONE"
                       )
                       .arg(seq)
                       .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
                       .arg(battery, 0, 'f', 2)
                       .arg(altitude, 0, 'f', 1)
                       .arg(temperature, 0, 'f', 1);

    emit rawLineReceived(line);

    TelemetryFrame frame = TelemetryParser::parseLine(line);
    if (frame.valid) {
        emit telemetryReceived(frame);
    }
}