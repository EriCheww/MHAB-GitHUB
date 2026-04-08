#ifndef TELEMETRYFRAME_H
#define TELEMETRYFRAME_H

#include <QString>
#include <QDateTime>
#include <QtGlobal>

struct TelemetryFrame
{
    bool valid = false;
    QString parseError;

    quint8 protocolVersion = 0;
    quint8 recordType = 0;
    quint8 sourceId = 0;
    quint8 destinationId = 0;

    quint16 sequence = 0;
    QDateTime timestamp;

    double batteryVoltage = 0.0;   // V
    double altitude = 0.0;         // m
    double temperature = 0.0;      // deg C
    double latitude = 0.0;         // deg
    double longitude = 0.0;        // deg
    double cpuUsage = 0.0;         // %
    double ramUsage = 0.0;         // %
    double packetErrorRate = 0.0;  // %
    bool fault = false;
    quint8 faultCode = 0;
    QString faultText;
};

#endif // TELEMETRYFRAME_H