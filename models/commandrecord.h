#pragma once

#include <QString>

struct CommandRecord
{
    QString time;
    quint32 cmdSeq = 0;
    QString command;
    QString status;   // QUEUED / SENT / ACKED / TIMEOUT / FAILED
    QString result;
};