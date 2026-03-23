#pragma once

#include "telemetryframe.h"
#include <QString>

class TelemetryParser
{
public:
    static TelemetryFrame parseLine(const QString& line);
};