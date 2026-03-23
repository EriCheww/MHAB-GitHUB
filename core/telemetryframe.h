#pragma once

#include <QString>
#include <QDateTime>

struct TelemetryFrame
{
    quint32 sequence = 0;
    QDateTime timestamp;

    double batteryVoltage = 0.0;
    double altitude = 0.0;
    double temperature = 0.0;

    double latitude = 0.0;
    double longitude = 0.0;

    double cpuUsage = 0.0;
    double ramUsage = 0.0;
    double packetErrorRate = 0.0;

    bool fault = false;
    QString faultText;
    bool valid = false;
};