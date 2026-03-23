#pragma once

#include <QString>

struct PendingCommand
{
    quint32 cmdSeq = 0;
    QString command;
    QString state;              // WAITING_ACK / RETRYING / DONE
    int retriesLeft = 0;
    int timeoutRemainingS = 0;
    int timeoutS = 0;
    QString lastTx;
};