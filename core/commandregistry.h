#pragma once

#include <QVector>
#include "../models/commanddefinition.h"
class CommandRegistry
{
public:
    static QVector<CommandDefinition> defaultCommands();
};