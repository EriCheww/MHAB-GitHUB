#pragma once

#include <QAbstractTableModel>
#include <QVector>
#include "commandrecord.h"

class CommandHistoryModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit CommandHistoryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void addRecord(const CommandRecord &record);
    void clear();

private:
    QVector<CommandRecord> records;
};