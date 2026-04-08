#ifndef TELEMETRYPARSER_H
#define TELEMETRYPARSER_H

#include "telemetryframe.h"
#include <QString>
#include <QByteArray>

class TelemetryParser
{
public:
    static constexpr quint8 SYNC1 = 0xAA;
    static constexpr quint8 SYNC2 = 0x55;
    static constexpr quint8 EOT   = 0x04;
    static constexpr quint8 PROTOCOL_VERSION = 0x01;
    static constexpr quint8 RECORD_TYPE_TELEMETRY = 0x01;
    static constexpr int AUTH_TAG_SIZE = 16;

    static TelemetryFrame parsePacket(const QByteArray& packet);
    static QByteArray buildTelemetryPacket(const TelemetryFrame& frame,
                                           quint8 sourceId = 0x01,
                                           quint8 destinationId = 0x10);


    static bool bitStringToByteArray(const QString& bitString,
                                     QByteArray& outBytes,
                                     QString& error);

    static QString byteArrayToBitString(const QByteArray& data);
    static QString describePacket(const QByteArray& packet);
    static bool textToPacketBytes(const QString& text,
                                  QByteArray& outBytes,
                                  QString& error);

    static bool hexStringToByteArray(const QString& hexString,
                                     QByteArray& outBytes,
                                     QString& error);

private:
    static quint16 crc16CcittFalse(const QByteArray& data);
};

#endif // TELEMETRYPARSER_H