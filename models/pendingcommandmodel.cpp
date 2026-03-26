#include "pendingcommandmodel.h"

PendingCommandModel::PendingCommandModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int PendingCommandModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return commands.size();
}

int PendingCommandModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return 7;
}

QVariant PendingCommandModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return QVariant();

    const PendingCommand &cmd = commands.at(index.row());

    switch (index.column()) {
    case 0: return cmd.cmdSeq;
    case 1: return cmd.command;
    case 2: return cmd.state;
    case 3: return cmd.retriesLeft;
    case 4: return cmd.timeoutRemainingS;
    case 5: return cmd.timeoutS;
    case 6: return cmd.lastTx;
    default: return QVariant();
    }
}

QVariant PendingCommandModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return "CmdSeq";
        case 1: return "Command";
        case 2: return "State";
        case 3: return "RetriesLeft";
        case 4: return "TimeoutRemain_s";
        case 5: return "Timeout_s";
        case 6: return "LastTx";
        default: return QVariant();
        }
    }

    return QVariant();
}

void PendingCommandModel::addPendingCommand(const PendingCommand &cmd)
{
    beginInsertRows(QModelIndex(), commands.size(), commands.size());
    commands.append(cmd);
    endInsertRows();
}

void PendingCommandModel::clear()
{
    beginResetModel();
    commands.clear();
    endResetModel();
}





void PendingCommandModel::updatePendingCommand(quint32 cmdSeq, const QString &state, int retriesLeft, int timeoutRemainingS)
{
    for (int row = 0; row < commands.size(); ++row) {
        if (commands[row].cmdSeq == cmdSeq) {
            commands[row].state = state;
            commands[row].retriesLeft = retriesLeft;
            commands[row].timeoutRemainingS = timeoutRemainingS;

            QModelIndex topLeft = index(row, 0);
            QModelIndex bottomRight = index(row, columnCount() - 1);
            emit dataChanged(topLeft, bottomRight);
            return;
        }
    }
}

void PendingCommandModel::removePendingCommand(quint32 cmdSeq)
{
    for (int row = 0; row < commands.size(); ++row) {
        if (commands[row].cmdSeq == cmdSeq) {
            beginRemoveRows(QModelIndex(), row, row);
            commands.removeAt(row);
            endRemoveRows();
            return;
        }
    }
}


