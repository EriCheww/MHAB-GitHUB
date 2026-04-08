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


void CommunicationManager::feedRawPacket(const QByteArray& packet)
{
    feedRawBytes(packet);
    //     processPacket(packet);
}
void CommunicationManager::processPacket(const QByteArray& packet)
{
    //emit rawLineReceived(packet.toHex(' ').toUpper());   // once you move to stream mode, raw logging should happen when bytes arrive, not only after a packet is successfully recovered.

    TelemetryFrame frame = TelemetryParser::parsePacket(packet);
    if (frame.valid) {
        emit telemetryReceived(frame);
    } else {
        emit statusChanged("Packet parse failed: " + frame.parseError);
    }
}

void CommunicationManager::generateFakeTelemetry()
{
    seq++;
    battery -= 0.01;
    altitude += 8.5;
    temperature -= 0.15;

    TelemetryFrame frame;
    frame.valid = true;
    frame.sequence = seq;
    frame.timestamp = QDateTime::currentDateTimeUtc();

    frame.batteryVoltage = battery;
    frame.altitude = altitude;
    frame.temperature = temperature;
    frame.latitude = -37.910000;
    frame.longitude = 145.130000;
    frame.cpuUsage = 18.5;
    frame.ramUsage = 42.0;
    frame.packetErrorRate = 1.2;
    frame.fault = false;
    frame.faultCode = 0;
    frame.faultText = "NONE";

    QByteArray packet = TelemetryParser::buildTelemetryPacket(frame, 0x01, 0x10);

    int mode = seq % 3;

    if (mode == 0) {
        feedRawBytes(packet);
    } else if (mode == 1) {
        int splitIndex = packet.size() / 2;
        feedRawBytes(packet.left(splitIndex));
        feedRawBytes(packet.mid(splitIndex));
    } else if (mode == 2) {
        QByteArray noise;
        noise.append(char(0x12));
        noise.append(char(0x34));
        noise.append(char(0x56));

        feedRawBytes(noise);
        feedRawBytes(packet);
    }
    else {
        emit rawLineReceived("[SIM] Sending corrupted packet");
        QByteArray corrupted = packet;

        // Corrupt one payload byte so CRC should fail
        if (corrupted.size() > 15) {
            corrupted[15] ^= 0x01;
        }

        feedRawBytes(corrupted);
    }
}


void CommunicationManager::feedRawBytes(const QByteArray& data)
{
    if (data.isEmpty()) {
        return;
    }

    emit rawLineReceived(data.toHex(' ').toUpper());

    rxBuffer.append(data);
    extractPacketsFromBuffer();
}




void CommunicationManager::extractPacketsFromBuffer()
/**
1. Search for sync bytes

It looks for:

0xAA 0x55
2. Drop garbage before sync

If noise came in first, it removes it.

3. Wait until at least the minimum header/trailer exists

So it does not parse too early.

4. Read payload length at byte index 8

This gives expected full packet size.

5. If full packet not yet available, wait

This handles split packets.

6. If full packet exists, cut it out and process it

Then loop again in case more packets are in the buffer.
 */



{
    constexpr int MIN_PACKET_SIZE =
        2 + 1 + 1 + 1 + 1 + 2 + 1 + 4 + 2 + TelemetryParser::AUTH_TAG_SIZE + 1;

    while (true) {
        if (rxBuffer.size() < 2) {
            return;
        }

        int syncIndex = -1;

        for (int i = 0; i < rxBuffer.size() - 1; ++i) {
            quint8 b0 = static_cast<quint8>(rxBuffer[i]);
            quint8 b1 = static_cast<quint8>(rxBuffer[i + 1]);

            if (b0 == TelemetryParser::SYNC1 && b1 == TelemetryParser::SYNC2) {
                syncIndex = i;
                break;
            }
        }

        if (syncIndex < 0) {
            if (rxBuffer.size() > 1) {
                rxBuffer = rxBuffer.right(1);
            }
            return;
        }

        if (syncIndex > 0) {
            rxBuffer.remove(0, syncIndex);
        }

        if (rxBuffer.size() < MIN_PACKET_SIZE) {
            return;
        }

        quint8 payloadLength = static_cast<quint8>(rxBuffer[8]);

        int expectedSize =
            2 + 1 + 1 + 1 + 1 + 2 + 1 + 4 +
            payloadLength +
            2 +
            TelemetryParser::AUTH_TAG_SIZE +
            1;

        if (expectedSize < MIN_PACKET_SIZE) {
            rxBuffer.remove(0, 1);
            continue;
        }

        if (rxBuffer.size() < expectedSize) {
            return;
        }

        QByteArray packet = rxBuffer.left(expectedSize);
        rxBuffer.remove(0, expectedSize);

        processPacket(packet);
    }
}



