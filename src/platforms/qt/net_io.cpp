/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "net_io.h"

#include <notifications.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>
#include <unordered_map>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include "osm2go_stl.h"
#include <osm2go_platform.h>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QSsl>
#include <QUrl>

namespace {

/* structure shared between worker and master thread */
struct net_io_request_t {
  net_io_request_t(const std::string &u, const std::string &f, bool c);
  net_io_request_t(const std::string &u, std::string *smem) __attribute__((nonnull(3)));

  const QUrl url;
  bool cancel;

  /* http related stuff: */
  QNetworkReply::NetworkError error;
  QList<QSslError> sslErrors;

  /* request specific fields */
  QFile file;   /* used for NET_IO_DL_FILE */
  std::string * const mem;   /* used for NET_IO_DL_MEM */
  const bool use_compression;
};

net_io_request_t::net_io_request_t(const std::string &u, const std::string &f, bool c)
  : url(QString::fromStdString(u))
  , cancel(false)
  , error(QNetworkReply::NoError)
  , file(QString::fromStdString(f))
  , mem(nullptr)
  , use_compression(c)
{
  assert(!f.empty());
  if(!file.open(QIODevice::WriteOnly))
    error = static_cast<QNetworkReply::NetworkError>(-1);
}

net_io_request_t::net_io_request_t(const std::string &u, std::string *smem)
  : url(QString::fromStdString(u))
  , cancel(false)
  , error(QNetworkReply::NoError)
  , mem(smem)
  , use_compression(false)
{
}

/**
 * @brief perform the download
 * @param parent parent widget for progress bar
 * @param rq request to serve
 * @param title title string for progress dialog
 * @returns if the request was successful
 *
 * In case parent is nullptr, no progress dialog is shown and title is ignored.
 */
bool
net_io_do(osm2go_platform::Widget *parent, net_io_request_t &request, const QString &title)
{
  /* the request structure is shared between master and worker thread. */
  /* typically the master thread will do some waiting until the worker */
  /* thread returns. But the master may very well stop waiting since e.g. */
  /* the user activated some cancel button. The client will learn this */
  /* from the fact that it's holding the only reference to the request */

  /* create worker thread */
  bool dlgcancelled = false;
  QPointer<QProgressDialog> dialog;
  if(likely(parent != nullptr)) {
    /* create the dialog box shown while worker is running */
    dialog = new QProgressDialog(parent);
    dialog->setWindowTitle(trstring("Downloading %1").arg(title));
    dialog->setWindowModality(Qt::WindowModal);
  }

  auto mgr = new QNetworkAccessManager(parent);

  QNetworkRequest req(QUrl(request.url));
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
  auto ssl = req.sslConfiguration();
  ssl.setProtocol(QSsl::TlsV1_0OrLater);
  req.setSslConfiguration(ssl);
  req.setHeader(QNetworkRequest::UserAgentHeader, PACKAGE "-QtNetwork/" VERSION "-" QT_VERSION_STR);
  if(request.use_compression)
    req.setRawHeader("Accept-Encoding", "gzip");
  QNetworkReply *r = mgr->get(req);

  QObject::connect(r,
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                   &QNetworkReply::errorOccurred,
#else
                   QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error),
#endif
    [&request](QNetworkReply::NetworkError err) {  request.error = err; });
  QObject::connect(r, &QNetworkReply::sslErrors, [&request](const QList<QSslError> &err) {
    request.sslErrors = err;
  });

  if(!request.file.fileName().isEmpty()) {
    QObject::connect(r, &QIODevice::readyRead, [&request, r]() {
      request.file.write(r->readAll());
    });
  } else {
    QObject::connect(r, &QIODevice::readyRead, [&request, r]() {
      const QByteArray d = r->readAll();
      request.mem->append(d.constData(), d.size());
    });
  }

  if(!dialog.isNull()) {
    QObject::connect(dialog, &QProgressDialog::canceled, r, &QNetworkReply::abort);
    QObject::connect(dialog, &QProgressDialog::canceled, [&dlgcancelled]() { dlgcancelled = true; });
    QObject::connect(r, &QNetworkReply::downloadProgress, dialog, [dialog](qint64 bytesReceived, qint64 bytesTotal) {
      if(bytesTotal >= 0)
        dialog->setMaximum(bytesTotal);
      dialog->setValue(bytesReceived);
      dialog->setLabelText(QString::number(bytesReceived));
    });
    dialog->show();
  }

  while(!r->isFinished())
    QCoreApplication::processEvents();

  delete dialog;
  request.file.close();

  /* user pressed cancel */
  if(dlgcancelled) {
    qDebug() << "operation cancelled, leave worker alone";
    return false;
  }

  qDebug() << "Transfer finished";

  /* --------- evaluate result --------- */
  mgr->deleteLater();
  r->deleteLater();

  /* the http connection itself may have failed */
  if(request.error != QNetworkReply::NoError) {
    error_dlg(trstring("Download failed with message:\n\n%1").arg(request.error), parent);
    return false;
  }

  /* a valid http connection may have returned an error */
  const auto v = r->attribute(QNetworkRequest::HttpStatusCodeAttribute);
  if(v.toInt() != 200) {
    error_dlg(trstring("Download failed with code %1:\n\n%2\n").arg(v.toInt())
                       .arg(http_message(v.toInt())), parent);
    return false;
  }

  return true;
}

bool
net_io_download_file(osm2go_platform::Widget *parent, const std::string &url, const std::string &filename,
                     const QString &title, bool compress)
{
  net_io_request_t request(url, filename, compress);

  qDebug() << "net_io: download " << url.c_str() << " to file " << filename.c_str();

  bool result = net_io_do(parent, request, title);
  if(!result) {

    /* remove the file that may have been written by now. */

    qDebug() << "request failed, deleting " << filename.c_str();
    if(!request.file.fileName().isEmpty())
      request.file.remove();
  } else
    qDebug() << "request ok";

  return result;
}

} // namespace

bool
net_io_download_file(osm2go_platform::Widget *parent, const std::string &url, const std::string &filename,
                     trstring::native_type_arg title, bool compress)
{
  return net_io_download_file(parent, url, filename, static_cast<QString>(title), compress);
}

bool
net_io_download_file(osm2go_platform::Widget *parent, const std::string &url,
                     const std::string &filename, const std::string &title, bool compress)
{
  return net_io_download_file(parent, url, filename, QString::fromStdString(title), compress);
}

bool
net_io_download_mem(osm2go_platform::Widget *parent, const std::string &url, std::string &data,
                    trstring::native_type_arg title)
{
  net_io_request_t request(url, &data);

  qDebug() << "net_io: download " << url.c_str() << " to memory";

  bool result = net_io_do(parent, request, title);
  if(unlikely(!result))
    data.clear();

  return result;
}
