/*
 * SPDX-FileCopyrightText: 2020-2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <osm_api.h>
#include <osm_api_p.h>

#include <appdata.h>
#include <diff.h>
#include <map.h>
#include <net_io.h>
#include <osm.h>
#include <project.h>
#include <settings.h>
#include <uicontrol.h>

#include <cstring>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>

#include <osm2go_annotations.h>
#include "osm2go_i18n.h"
#include "osm2go_platform.h"
#include "osm2go_platform_qt.h"

#define COLOR_ERR  "red"
#define COLOR_OK   "darkgreen"

namespace {

class osm_upload_context_qt : public osm_upload_context_t {
public:
  osm_upload_context_qt(appdata_t &a, project_t::ref p, const QString &c, const QString &s);

  const QString cstr;
  const QString sstr;
  QPointer<QTextEdit> logview;
};

osm_upload_context_qt::osm_upload_context_qt(appdata_t &a, project_t::ref p, const QString &c, const QString &s)
  : osm_upload_context_t(a, p, c.toUtf8().constData(), s.toUtf8().constData())
  , cstr(c)
  , sstr(s)
  , logview(new QTextEdit())
{
  logview->setReadOnly(true);
}

template<typename T>
void table_insert_count(QTableWidget *table, const osm_t::dirty_t::counter<T> &counter, const int row)
{
  int column = 0;
  for (auto cnt : { static_cast<size_t>(counter.total), counter.added.size(), counter.changed.size(), counter.deleted.size() }) {
    auto *item = new QTableWidgetItem(QString::number(cnt));
    item->setTextAlignment(Qt::AlignCenter);
    table->setItem(row, column++, item);
  }
}

QTableWidget *
details_table(const osm_t::dirty_t &dirty)
{
  auto *table = new QTableWidget(3, 4);
  table->setHorizontalHeaderLabels({ trstring("Total"), trstring("New"), trstring("Modified"), trstring("Deleted") });
  table->setVerticalHeaderLabels({ trstring("Nodes:"), trstring("Ways:"), trstring("Relations:") });
  table_insert_count(table, dirty.nodes, 0);
  table_insert_count(table, dirty.ways, 1);
  table_insert_count(table, dirty.relations, 2);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->resizeColumnsToContents();
  table->horizontalHeader()->setStretchLastSection(true);

  return table;
}

} // namespace

void
osm_upload_dialog(appdata_t &appdata, const osm_t::dirty_t &dirty)
{
  osm2go_platform::DialogGuard dlg(new QDialog(appdata_t::window));
  dlg->setWindowTitle(trstring("Upload to OSM"));

  auto *ly = new QVBoxLayout(dlg);
  ly->addWidget(details_table(dirty));

  /* ------- add username and password entries ------------ */

  auto *fly = new QFormLayout();
  auto *uentry = new QLineEdit();
  settings_t::ref settings = settings_t::instance();

  uentry->setText(QString::fromStdString(settings->username));
  uentry->setPlaceholderText(trstring("<your osm username>"));
  fly->addRow(trstring("Username:"), uentry);

  auto *pentry = new QLineEdit();
  pentry->setEchoMode(QLineEdit::Password);
  if(!settings->password.empty())
    pentry->setText(QString::fromStdString(settings->password));
  pentry->setPlaceholderText(trstring("<your osm password>"));
  fly->addRow(trstring("Password:"), pentry);

  auto *sentry = new QLineEdit();
  fly->addRow(trstring("Source:"), sentry);

  ly->addLayout(fly);

  auto *cedit = new QPlainTextEdit();
  cedit->setPlaceholderText(trstring("Please add a comment"));
  ly->addWidget(cedit);

  auto *bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
  ly->addWidget(bbox);
  QObject::connect(bbox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
  QObject::connect(bbox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

  auto okbtn = bbox->button(QDialogButtonBox::Ok);
  okbtn->setEnabled(false);

  auto ch = [uentry, pentry, cedit, okbtn]() {
    okbtn->setEnabled(!uentry->text().trimmed().isEmpty() &&
                      !pentry->text().trimmed().isEmpty() &&
                      !cedit->toPlainText().trimmed().isEmpty());
  };

  QObject::connect(uentry, &QLineEdit::textChanged, ch);
  QObject::connect(pentry, &QLineEdit::textChanged, ch);
  QObject::connect(cedit, &QPlainTextEdit::textChanged, ch);

  osm2go_platform::dialog_size_hint(dlg, osm2go_platform::MISC_DIALOG_MEDIUM);

  if (dlg->exec() != QDialog::Accepted)
    return;

  /* retrieve username and password */
  settings->username = uentry->text().trimmed().toStdString();
  settings->password = pentry->text().toStdString();

  project_t::ref project = appdata.project;
  osm_upload_context_qt context(appdata, project, cedit->toPlainText().trimmed(),
                                sentry->text().trimmed());

  /* server url should not end with a slash */
  if(!project->rserver.empty() && project->rserver[project->rserver.size() - 1] == '/')
    project->rserver.erase(project->rserver.size() - 1);

  project->save();

  dlg = new QDialog(appdata_t::window);
  dlg->setWindowTitle(trstring("Uploading"));

  ly = new QVBoxLayout(dlg);
  ly->addWidget(context.logview);
  bbox = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
  ly->addWidget(bbox);
  QObject::connect(bbox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

  osm2go_platform::dialog_size_hint(dlg, osm2go_platform::MISC_DIALOG_LARGE);

  auto closebtn = bbox->button(QDialogButtonBox::Close);
  closebtn->setEnabled(false);

  dlg->setModal(true);
  dlg->show();

  context.upload(dirty, dlg);

  closebtn->setEnabled(true);

  dlg->exec();
}

osm_upload_context_t::osm_upload_context_t(appdata_t &a, project_t::ref p, const char *c, const char *s)
  : appdata(a)
  , osm(p->osm)
  , project(p)
  , urlbasestr(p->server(settings_t::instance()->server) + "/")
  , comment(c)
  , src(s == nullptr ? s : std::string())
{
}

void
osm_upload_context_t::append_str(const char *msg, const char *colorname)
{
  append(trstring(msg), colorname);
}

void
osm_upload_context_t::append(const trstring &msg, const char *colorname)
{
  static QHash<const char *, QColor> colors;
  QColor textColor;
  if (colorname == nullptr) {
    textColor = Qt::black;
  } else {
    auto it = colors.find(colorname);
    if (unlikely(it == colors.end())) {
      textColor = colorname;
      colors[colorname] = textColor;
    } else {
      textColor = it.value();
    }
  }

  auto logview = static_cast<osm_upload_context_qt *>(this)->logview;
  logview->setTextColor(textColor);
  logview->append(msg);
}
