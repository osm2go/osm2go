#pragma once

#include <limits>
#include <QAbstractItemModel>
#include <QTest>

void checkHeaderData(const QAbstractItemModel *model, const std::vector<QString> &strings, Qt::Orientation orientation)
{
  const int sections = (orientation == Qt::Horizontal) ? model->columnCount(QModelIndex()) : model->rowCount(QModelIndex());

  for (int i = 0; i < sections; i++) {
    QCOMPARE(model->headerData(i, orientation, Qt::DisplayRole).toString(), strings.at(i));
    QCOMPARE(model->headerData(i, orientation, Qt::EditRole), QVariant());
  }

  QCOMPARE(model->headerData(sections, orientation, Qt::DisplayRole), QVariant());
  QCOMPARE(model->headerData(- 1, orientation, Qt::DisplayRole), QVariant());
}

void checkHeaderDataEmpty(const QAbstractItemModel *model, Qt::Orientation orientation)
{
  const int sections = (orientation == Qt::Horizontal) ? model->columnCount(QModelIndex()) : model->rowCount(QModelIndex());

  for (int i = -1; i <= sections; i++) {
    QCOMPARE(model->headerData(i, orientation, Qt::DisplayRole), QVariant());
    QCOMPARE(model->headerData(i, orientation, Qt::EditRole), QVariant());
  }
}
