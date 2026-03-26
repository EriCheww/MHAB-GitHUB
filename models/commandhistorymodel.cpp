#include "commandhistorymodel.h"

CommandHistoryModel::CommandHistoryModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int CommandHistoryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return records.size();
}

int CommandHistoryModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return 5;
}

QVariant CommandHistoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return QVariant();

    const CommandRecord &record = records.at(index.row());

    switch (index.column()) {
    case 0: return record.time;
    case 1: return record.command;
    case 2: return record.cmdSeq;
    case 3: return record.status;
    case 4: return record.result;
    default: return QVariant();
    }
}

QVariant CommandHistoryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return "Time";
        case 1: return "Command";
        case 2: return "CmdSeq";
        case 3: return "Status";
        case 4: return "Result";
        default: return QVariant();
        }
    }

    return QVariant();
}

void CommandHistoryModel::addRecord(const CommandRecord &record)
{
    beginInsertRows(QModelIndex(), records.size(), records.size());
    records.append(record);
    endInsertRows();
}

void CommandHistoryModel::clear()
{
    beginResetModel();
    records.clear();
    endResetModel();
}



void CommandHistoryModel::updateRecordStatus(quint32 cmdSeq, const QString &status, const QString &result)
{
    for (int row = 0; row < records.size(); ++row) {
        if (records[row].cmdSeq == cmdSeq) {
            records[row].status = status;
            records[row].result = result;

            QModelIndex topLeft = index(row, 0);
            QModelIndex bottomRight = index(row, columnCount() - 1);
            emit dataChanged(topLeft, bottomRight);
            return;
        }
    }
}

