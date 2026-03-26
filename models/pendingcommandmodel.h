#pragma once

#include <QAbstractTableModel>
#include <QVector>
#include "pendingcommand.h"

class PendingCommandModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit PendingCommandModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void addPendingCommand(const PendingCommand &cmd);
    void clear();

    void updatePendingCommand(quint32 cmdSeq, const QString &state, int retriesLeft, int timeoutRemainingS);
    void removePendingCommand(quint32 cmdSeq);

private:
    QVector<PendingCommand> commands;
};