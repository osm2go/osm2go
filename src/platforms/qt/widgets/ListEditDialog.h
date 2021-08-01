/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDialog>

enum list_button_t {
  LIST_BUTTON_NEW = (1 << 0),
  LIST_BUTTON_EDIT = (1 << 1),
  LIST_BUTTON_REMOVE = (1 << 2),
  LIST_BUTTON_USER0 = (1 << 3),
  LIST_BUTTON_USER1 = (1 << 4),
  LIST_BUTTON_USER2 = (1 << 5)
};

class QDialogButtonBox;
class QPushButton;
class QSortFilterProxyModel;
class QTableView;

class ListEditDialog : public QDialog {
  Q_OBJECT
  Q_DISABLE_COPY(ListEditDialog)

public:
  Q_DECLARE_FLAGS(Options, list_button_t)

  ListEditDialog(QWidget *parent, Options buttons, bool sortable = true);

  QTableView * const view;
  QDialogButtonBox * const windowButtons;
  QSortFilterProxyModel * const proxymodel;

  QPushButton *btnNew = nullptr;
  QPushButton *btnEdit = nullptr;
  QPushButton *btnRemove = nullptr;
  QPushButton *btnUser0 = nullptr;
  QPushButton *btnUser1 = nullptr;
  QPushButton *btnUser2 = nullptr;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ListEditDialog::Options)
