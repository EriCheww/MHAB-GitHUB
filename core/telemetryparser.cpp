#include "telemetryparser.h"
#include <QStringList>
#include <QMap>

TelemetryFrame TelemetryParser::parseLine(const QString& line)
{
    TelemetryFrame frame;
    QMap<QString, QString> map;

    const QStringList pairs = line.split(',', Qt::SkipEmptyParts);
    for (const QString& pair : pairs) {
        const int idx = pair.indexOf('=');
        if (idx > 0) {
            const QString key = pair.left(idx).trimmed().toUpper();
            const QString value = pair.mid(idx + 1).trimmed();
            map[key] = value;
        }
    }

    if (map.contains("SEQ")) frame.sequence = map["SEQ"].toUInt();
    if (map.contains("TIME")) frame.timestamp = QDateTime::fromString(map["TIME"], Qt::ISODate);
    if (map.contains("BAT")) frame.batteryVoltage = map["BAT"].toDouble();
    if (map.contains("ALT")) frame.altitude = map["ALT"].toDouble();
    if (map.contains("TEMP")) frame.temperature = map["TEMP"].toDouble();
    if (map.contains("LAT")) frame.latitude = map["LAT"].toDouble();
    if (map.contains("LON")) frame.longitude = map["LON"].toDouble();
    if (map.contains("CPU")) frame.cpuUsage = map["CPU"].toDouble();
    if (map.contains("RAM")) frame.ramUsage = map["RAM"].toDouble();
    if (map.contains("PER")) frame.packetErrorRate = map["PER"].toDouble();
    if (map.contains("FAULT")) frame.fault = (map["FAULT"] == "1");
    if (map.contains("FAULT_TEXT")) frame.faultText = map["FAULT_TEXT"];

    frame.valid = map.contains("SEQ");
    return frame;
}