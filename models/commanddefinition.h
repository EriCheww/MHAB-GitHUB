#pragma once

#include <QString>

struct CommandDefinition
{
    QString displayName;      // what user sees on GUI
    QString commandKey;       // internal key, e.g. "shutdown"
    QString description;      // short explanation
    bool requiresParameter = false;
    QString parameterHint;    // e.g. "angle in degrees"
    int defaultRetries = 3;
    int defaultTimeoutS = 5;
};