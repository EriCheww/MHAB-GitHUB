#pragma once

#include <QObject>
#include <QTimer>
#include "telemetryframe.h"

class CommunicationManager : public QObject
{
    Q_OBJECT

public:
    explicit CommunicationManager(QObject *parent = nullptr);
    ~CommunicationManager();

    void startSimulation(int intervalMs = 1000);
    void stopSimulation();

signals:
    void telemetryReceived(const TelemetryFrame& frame);
    void statusChanged(const QString& status);
    void rawLineReceived(const QString& line);

private slots:
    void generateFakeTelemetry();

private:
    QTimer* simTimer = nullptr;

    quint32 seq = 0;
    double battery = 12.6;
    double altitude = 0.0;
    double temperature = 22.0;
};