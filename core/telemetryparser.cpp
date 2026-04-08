#include "telemetryparser.h"

#include <QDataStream>
#include <QBuffer>
#include <QIODevice>
#include <QDateTime>
#include <QTimeZone>
#include <QtGlobal>
#include <QTextStream>
#include <QStringList>

static QString hexByte(quint8 value)
{
    return QString("0x%1").arg(value, 2, 16, QChar('0')).toUpper();
}

static QString faultCodeToText(quint8 faultCode)
{
    switch (faultCode) {
    case 0:  return "NONE";
    case 1:  return "LOW_BATTERY";
    case 2:  return "GPS_FAULT";
    case 3:  return "SENSOR_FAULT";
    case 4:  return "LINK_DEGRADED";
    default: return "UNKNOWN_FAULT";
    }
}

quint16 TelemetryParser::crc16CcittFalse(const QByteArray& data)
{
    quint16 crc = 0xFFFF;
    for (unsigned char byte : data) {
        crc ^= static_cast<quint16>(byte) << 8;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

bool TelemetryParser::bitStringToByteArray(const QString& bitString,
                                           QByteArray& outBytes,
                                           QString& error)
{
    QString cleaned;
    cleaned.reserve(bitString.size());

    for (QChar ch : bitString) {
        if (ch == '0' || ch == '1') {
            cleaned.append(ch);
        } else if (ch.isSpace()) {
            continue;
        } else {
            error = QString("Invalid character in bit string: '%1'").arg(ch);
            return false;
        }
    }

    if (cleaned.isEmpty()) {
        error = "Bit string is empty";
        return false;
    }

    if (cleaned.size() % 8 != 0) {
        error = QString("Bit count must be a multiple of 8. Current bits = %1").arg(cleaned.size());
        return false;
    }

    outBytes.clear();
    outBytes.reserve(cleaned.size() / 8);

    for (int i = 0; i < cleaned.size(); i += 8) {
        quint8 byte = 0;
        for (int j = 0; j < 8; ++j) {
            byte <<= 1;
            if (cleaned[i + j] == '1') {
                byte |= 0x01;
            }
        }
        outBytes.append(static_cast<char>(byte));
    }

    return true;
}

QString TelemetryParser::byteArrayToBitString(const QByteArray& data)
{
    QStringList groups;
    groups.reserve(data.size());

    for (unsigned char byte : data) {
        QString bits;
        for (int i = 7; i >= 0; --i) {
            bits.append((byte & (1 << i)) ? '1' : '0');
        }
        groups.append(bits);
    }

    return groups.join(' ');
}

TelemetryFrame TelemetryParser::parsePacket(const QByteArray& packet)
{
    TelemetryFrame frame;
    frame.valid = false;

    constexpr int MIN_PACKET_SIZE = 2 + 1 + 1 + 1 + 1 + 2 + 1 + 4 + 2 + AUTH_TAG_SIZE + 1;

    if (packet.size() < MIN_PACKET_SIZE) {
        frame.parseError = "Packet too short";
        return frame;
    }

    const quint8 sync1 = static_cast<quint8>(packet[0]);
    const quint8 sync2 = static_cast<quint8>(packet[1]);

    if (sync1 != SYNC1 || sync2 != SYNC2) {
        frame.parseError = "Invalid sync bytes";
        return frame;
    }

    const quint8 payloadLength = static_cast<quint8>(packet[8]);

    const int expectedSize =
        2 + 1 + 1 + 1 + 1 + 2 + 1 + 4 +
        payloadLength +
        2 +
        AUTH_TAG_SIZE +
        1;

    if (packet.size() != expectedSize) {
        frame.parseError = "Packet size does not match payload length";
        return frame;
    }

    const quint8 eot = static_cast<quint8>(packet[packet.size() - 1]);
    if (eot != EOT) {
        frame.parseError = "Invalid end-of-transmission byte";
        return frame;
    }

    const int crcIndex = 2 + 1 + 1 + 1 + 1 + 2 + 1 + 4 + payloadLength;
    const QByteArray crcRegion = packet.mid(2, crcIndex - 2);

    const quint16 receivedCrc =
        (static_cast<quint8>(packet[crcIndex]) << 8) |
        static_cast<quint8>(packet[crcIndex + 1]);

    const quint16 computedCrc = crc16CcittFalse(crcRegion);

    if (receivedCrc != computedCrc) {
        frame.parseError = "CRC check failed";
        return frame;
    }

    QByteArray data = packet.mid(2);
    QDataStream in(data);
    in.setByteOrder(QDataStream::BigEndian);

    quint8 protocolVersion = 0;
    quint8 recordType = 0;
    quint8 sourceId = 0;
    quint8 destinationId = 0;
    quint16 sequence = 0;
    quint8 payloadLen = 0;
    quint32 unixTime = 0;

    in >> protocolVersion;
    in >> recordType;
    in >> sourceId;
    in >> destinationId;
    in >> sequence;
    in >> payloadLen;
    in >> unixTime;

    if (protocolVersion != PROTOCOL_VERSION) {
        frame.parseError = "Unsupported protocol version";
        return frame;
    }

    if (recordType != RECORD_TYPE_TELEMETRY) {
        frame.parseError = "Unsupported record type";
        return frame;
    }

    constexpr quint8 TELEMETRY_PAYLOAD_SIZE = 24;

    if (payloadLen != TELEMETRY_PAYLOAD_SIZE) {
        frame.parseError = "Unexpected telemetry payload size";
        return frame;
    }

    quint16 battery_mV = 0;
    qint32 altitude_dm = 0;
    qint16 temperature_cC = 0;
    qint32 latitude_e7 = 0;
    qint32 longitude_e7 = 0;
    quint16 cpu_tenths = 0;
    quint16 ram_tenths = 0;
    quint16 per_tenths = 0;
    quint8 fault_flag = 0;
    quint8 fault_code = 0;

    in >> battery_mV;
    in >> altitude_dm;
    in >> temperature_cC;
    in >> latitude_e7;
    in >> longitude_e7;
    in >> cpu_tenths;
    in >> ram_tenths;
    in >> per_tenths;
    in >> fault_flag;
    in >> fault_code;

    frame.protocolVersion = protocolVersion;
    frame.recordType = recordType;
    frame.sourceId = sourceId;
    frame.destinationId = destinationId;
    frame.sequence = sequence;
    frame.timestamp = QDateTime::fromSecsSinceEpoch(unixTime, QTimeZone::UTC).toLocalTime();

    frame.batteryVoltage = static_cast<double>(battery_mV) / 1000.0;
    frame.altitude = static_cast<double>(altitude_dm) / 10.0;
    frame.temperature = static_cast<double>(temperature_cC) / 100.0;
    frame.latitude = static_cast<double>(latitude_e7) / 10000000.0;
    frame.longitude = static_cast<double>(longitude_e7) / 10000000.0;
    frame.cpuUsage = static_cast<double>(cpu_tenths) / 10.0;
    frame.ramUsage = static_cast<double>(ram_tenths) / 10.0;
    frame.packetErrorRate = static_cast<double>(per_tenths) / 10.0;
    frame.fault = (fault_flag != 0);
    frame.faultCode = fault_code;
    frame.faultText = faultCodeToText(fault_code);

    frame.valid = true;
    frame.parseError.clear();
    return frame;
}

QByteArray TelemetryParser::buildTelemetryPacket(const TelemetryFrame& frame,
                                                 quint8 sourceId,
                                                 quint8 destinationId)
{
    constexpr quint8 payloadLength = 24;

    QByteArray packet;
    QBuffer buffer(&packet);
    buffer.open(QIODevice::WriteOnly);

    QDataStream out(&buffer);
    out.setByteOrder(QDataStream::BigEndian);

    out << static_cast<quint8>(SYNC1);
    out << static_cast<quint8>(SYNC2);

    out << static_cast<quint8>(PROTOCOL_VERSION);
    out << static_cast<quint8>(RECORD_TYPE_TELEMETRY);
    out << static_cast<quint8>(sourceId);
    out << static_cast<quint8>(destinationId);
    out << static_cast<quint16>(frame.sequence);
    out << static_cast<quint8>(payloadLength);
    out << static_cast<quint32>(frame.timestamp.toSecsSinceEpoch());

    out << static_cast<quint16>(qBound(0, static_cast<int>(frame.batteryVoltage * 1000.0), 65535));
    out << static_cast<qint32>(frame.altitude * 10.0);
    out << static_cast<qint16>(frame.temperature * 100.0);
    out << static_cast<qint32>(frame.latitude * 10000000.0);
    out << static_cast<qint32>(frame.longitude * 10000000.0);
    out << static_cast<quint16>(qBound(0, static_cast<int>(frame.cpuUsage * 10.0), 65535));
    out << static_cast<quint16>(qBound(0, static_cast<int>(frame.ramUsage * 10.0), 65535));
    out << static_cast<quint16>(qBound(0, static_cast<int>(frame.packetErrorRate * 10.0), 65535));
    out << static_cast<quint8>(frame.fault ? 1 : 0);
    out << static_cast<quint8>(frame.faultCode);

    QByteArray crcRegion = packet.mid(2);
    const quint16 crc = crc16CcittFalse(crcRegion);
    out << static_cast<quint16>(crc);

    for (int i = 0; i < AUTH_TAG_SIZE; ++i) {
        out << static_cast<quint8>(0x00);
    }

    out << static_cast<quint8>(EOT);

    buffer.close();
    return packet;
}

QString TelemetryParser::describePacket(const QByteArray& packet)
{
    QString result;
    QTextStream ts(&result);

    ts << "===== Packet Breakdown =====\n";
    ts << "Total bytes: " << packet.size() << "\n";
    ts << "Total bits : " << packet.size() * 8 << "\n";
    ts << "Bit stream : " << byteArrayToBitString(packet) << "\n\n";

    constexpr int MIN_PACKET_SIZE = 2 + 1 + 1 + 1 + 1 + 2 + 1 + 4 + 2 + AUTH_TAG_SIZE + 1;
    if (packet.size() < MIN_PACKET_SIZE) {
        ts << "ERROR: Packet too short\n";
        return result;
    }

    quint8 sync1 = static_cast<quint8>(packet[0]);
    quint8 sync2 = static_cast<quint8>(packet[1]);
    quint8 version = static_cast<quint8>(packet[2]);
    quint8 recordType = static_cast<quint8>(packet[3]);
    quint8 sourceId = static_cast<quint8>(packet[4]);
    quint8 destinationId = static_cast<quint8>(packet[5]);
    quint16 sequence =
        (static_cast<quint8>(packet[6]) << 8) |
        static_cast<quint8>(packet[7]);
    quint8 payloadLength = static_cast<quint8>(packet[8]);

    quint32 unixTime =
        (static_cast<quint8>(packet[9]) << 24) |
        (static_cast<quint8>(packet[10]) << 16) |
        (static_cast<quint8>(packet[11]) << 8) |
        static_cast<quint8>(packet[12]);

    ts << "[Header Parts]\n";
    ts << "Sync1           : " << hexByte(sync1) << "\n";
    ts << "Sync2           : " << hexByte(sync2) << "\n";
    ts << "ProtocolVersion : " << static_cast<int>(version) << "\n";
    ts << "RecordType      : " << static_cast<int>(recordType) << "\n";
    ts << "SourceId        : " << static_cast<int>(sourceId) << "\n";
    ts << "DestinationId   : " << static_cast<int>(destinationId) << "\n";
    ts << "Sequence        : " << sequence << "\n";
    ts << "PayloadLength   : " << static_cast<int>(payloadLength) << "\n";
    ts << "UnixTimestamp   : " << unixTime << "\n";
    ts << "DecodedTime     : "
       << QDateTime::fromSecsSinceEpoch(unixTime, QTimeZone::UTC).toLocalTime().toString("yyyy-MM-dd hh:mm:ss")
       << "\n\n";

    int expectedSize =
        2 + 1 + 1 + 1 + 1 + 2 + 1 + 4 +
        payloadLength +
        2 +
        AUTH_TAG_SIZE +
        1;

    ts << "[Packet Size Check]\n";
    ts << "Expected size   : " << expectedSize << "\n";
    ts << "Actual size     : " << packet.size() << "\n";
    ts << "Size match      : " << (expectedSize == packet.size() ? "YES" : "NO") << "\n\n";

    if (packet.size() >= expectedSize) {
        const int payloadStart = 13;
        QByteArray payload = packet.mid(payloadStart, payloadLength);

        const int crcIndex = payloadStart + payloadLength;
        quint16 receivedCrc =
            (static_cast<quint8>(packet[crcIndex]) << 8) |
            static_cast<quint8>(packet[crcIndex + 1]);

        QByteArray crcRegion = packet.mid(2, crcIndex - 2);
        quint16 computedCrc = crc16CcittFalse(crcRegion);

        QByteArray authTag = packet.mid(crcIndex + 2, AUTH_TAG_SIZE);
        quint8 eot = static_cast<quint8>(packet[crcIndex + 2 + AUTH_TAG_SIZE]);

        ts << "[Integrity / Trailer]\n";
        ts << "Received CRC    : 0x" << QString::number(receivedCrc, 16).toUpper() << "\n";
        ts << "Computed CRC    : 0x" << QString::number(computedCrc, 16).toUpper() << "\n";
        ts << "CRC valid       : " << (receivedCrc == computedCrc ? "YES" : "NO") << "\n";
        ts << "AuthTag         : " << authTag.toHex(' ').toUpper() << "\n";
        ts << "EOT             : " << hexByte(eot) << "\n";
        ts << "EOT valid       : " << (eot == EOT ? "YES" : "NO") << "\n\n";

        ts << "[Payload Raw]\n";
        ts << "Payload hex     : " << payload.toHex(' ').toUpper() << "\n";
        ts << "Payload bits     : " << byteArrayToBitString(payload) << "\n\n";
    }

    TelemetryFrame frame = parsePacket(packet);

    ts << "[Parsing Result]\n";
    ts << "Valid           : " << (frame.valid ? "YES" : "NO") << "\n";
    if (!frame.valid) {
        ts << "Parse Error     : " << frame.parseError << "\n";
        return result;
    }

    ts << "\n[Decoded Telemetry]\n";
    ts << "Battery         : " << frame.batteryVoltage << " V\n";
    ts << "Altitude        : " << frame.altitude << " m\n";
    ts << "Temperature     : " << frame.temperature << " C\n";
    ts << "Latitude        : " << frame.latitude << "\n";
    ts << "Longitude       : " << frame.longitude << "\n";
    ts << "CPU Usage       : " << frame.cpuUsage << " %\n";
    ts << "RAM Usage       : " << frame.ramUsage << " %\n";
    ts << "PER             : " << frame.packetErrorRate << " %\n";
    ts << "Fault           : " << (frame.fault ? "YES" : "NO") << "\n";
    ts << "Fault Code      : " << static_cast<int>(frame.faultCode) << "\n";
    ts << "Fault Text      : " << frame.faultText << "\n";

    return result;
}












bool TelemetryParser::hexStringToByteArray(const QString& hexString,
                                           QByteArray& outBytes,
                                           QString& error)
{
    QString cleaned;
    cleaned.reserve(hexString.size());

    for (QChar ch : hexString) {
        if (ch.isSpace()) {
            continue;
        }

        if (ch.isDigit() ||
            (ch.toUpper() >= QChar('A') && ch.toUpper() <= QChar('F'))) {
            cleaned.append(ch.toUpper());
        } else {
            error = QString("Invalid character in hex string: '%1'").arg(ch);
            return false;
        }
    }

    if (cleaned.isEmpty()) {
        error = "Hex string is empty";
        return false;
    }

    if (cleaned.size() % 2 != 0) {
        error = QString("Hex digit count must be even. Current digits = %1").arg(cleaned.size());
        return false;
    }

    outBytes.clear();
    outBytes.reserve(cleaned.size() / 2);

    for (int i = 0; i < cleaned.size(); i += 2) {
        bool ok = false;
        QString byteText = cleaned.mid(i, 2);
        quint8 byte = static_cast<quint8>(byteText.toUInt(&ok, 16));
        if (!ok) {
            error = QString("Failed to parse hex byte: %1").arg(byteText);
            return false;
        }
        outBytes.append(static_cast<char>(byte));
    }

    return true;
}

bool TelemetryParser::textToPacketBytes(const QString& text,
                                        QByteArray& outBytes,
                                        QString& error)
{
    QString cleaned;
    cleaned.reserve(text.size());

    for (QChar ch : text) {
        if (!ch.isSpace()) {
            cleaned.append(ch);
        }
    }

    if (cleaned.isEmpty()) {
        error = "Input is empty";
        return false;
    }

    bool looksLikeBits = true;
    for (QChar ch : cleaned) {
        if (ch != '0' && ch != '1') {
            looksLikeBits = false;
            break;
        }
    }

    if (looksLikeBits) {
        return bitStringToByteArray(text, outBytes, error);
    }

    return hexStringToByteArray(text, outBytes, error);
}




