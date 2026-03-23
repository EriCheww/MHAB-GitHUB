#pragma once

#include <QString>

struct LinkMetrics
{
    QString lastRx;
    double packetsPerMin = 0.0;
    double snr = 0.0;
    double rssi = 0.0;
    QString txTime;
    QString rxTime;
    bool connected = false;
};