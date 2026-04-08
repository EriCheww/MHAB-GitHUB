#ifndef COMMUNICATIONMANAGER_H
#define COMMUNICATIONMANAGER_H

#include <QObject>
#include <QTimer>
#include <QByteArray>
#include "telemetryframe.h"

class CommunicationManager : public QObject
{
    Q_OBJECT

public:
    explicit CommunicationManager(QObject *parent = nullptr);
    ~CommunicationManager();

    void startSimulation(int intervalMs = 1000);
    void stopSimulation();
    void feedRawPacket(const QByteArray& packet);
    void feedRawBytes(const QByteArray& data);

signals:
    void telemetryReceived(const TelemetryFrame& frame);
    void statusChanged(const QString& status);
    void rawLineReceived(const QString& line);

private slots:
    void generateFakeTelemetry();

private:
    void processPacket(const QByteArray& packet);
    void extractPacketsFromBuffer();

    QTimer* simTimer = nullptr;

    QByteArray rxBuffer;

    quint16 seq = 0;
    double battery = 12.6;
    double altitude = 0.0;
    double temperature = 22.0;
};

#endif // COMMUNICATIONMANAGER_H