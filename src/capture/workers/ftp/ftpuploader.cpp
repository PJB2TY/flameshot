// Copyright(c) 2017-2018 Alejandro Sirgo Rica & Contributors
//
// This file is part of Flameshot.
//
//     Flameshot is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     Flameshot is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the GNU General Public License
//     along with Flameshot.  If not, see <http://www.gnu.org/licenses/>.

#include "ftpuploader.h"
#include "src/utils/filenamehandler.h"
#include "src/utils/systemnotification.h"
#include "src/capture/workers/imgur/loadspinner.h"
#include "src/capture/workers/imgur/imagelabel.h"
#include "src/capture/workers/imgur/notificationwidget.h"
#include "src/utils/confighandler.h"
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QShortcut>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDrag>
#include <QMimeData>
#include <QBuffer>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

FTPUploader::FTPUploader(const QPixmap &capture, QWidget *parent) :
    QWidget(parent), m_pixmap(capture)
{
    setWindowTitle(("Upload to FTP"));

    m_spinner = new LoadSpinner(this);
    m_spinner->setColor(ConfigHandler().uiMainColorValue());
    m_spinner->start();

    m_infoLabel = new QLabel(tr("Uploading Image to FTP"));

    m_vLayout = new QVBoxLayout();
    setLayout(m_vLayout);
    m_vLayout->addWidget(m_spinner, 0, Qt::AlignHCenter);
    m_vLayout->addWidget(m_infoLabel);

    m_NetworkAM = new QNetworkAccessManager(this);
    connect(m_NetworkAM, &QNetworkAccessManager::finished, this,
            &FTPUploader::handleReply);

    setAttribute(Qt::WA_DeleteOnClose);

    upload();
    //QTimer::singleShot(2000, this, &FTPUploader::onUploadOk); // testing
}

void FTPUploader::handleReply(QNetworkReply *reply) {
    m_spinner->deleteLater();
    if (reply->error() == QNetworkReply::NoError) {
        QString site = ConfigHandler().ftpSite();
        QString url = QStringLiteral("%1%2").arg(site, m_fileName);
        m_imageURL.setUrl(url);
        onUploadOk();
    } else {
        m_infoLabel->setText(reply->errorString());
    }
    new QShortcut(Qt::Key_Escape, this, SLOT(close()));
}

void FTPUploader::startDrag() {
    QMimeData *mimeData = new QMimeData;
    mimeData->setUrls(QList<QUrl> { m_imageURL });
    mimeData->setImageData(m_pixmap);

    QDrag *dragHandler = new QDrag(this);
    dragHandler->setMimeData(mimeData);
    dragHandler->setPixmap(m_pixmap.scaled(256, 256, Qt::KeepAspectRatioByExpanding,
                                           Qt::SmoothTransformation));
    dragHandler->exec();
}

void FTPUploader::upload() {
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    m_pixmap.save(&buffer, "PNG");

    m_fileName = FileNameHandler().getRandom() + ".png";

    QUrl url("ftp://" + ConfigHandler().ftpHostname() + "/" + m_fileName);
    url.setUserName(ConfigHandler().ftpLogin());
    url.setPassword(ConfigHandler().ftpPassword());
    url.setPort(ConfigHandler().ftpPort());

    m_NetworkAM->put(QNetworkRequest(url), byteArray);
}

void FTPUploader::onUploadOk() {
    m_infoLabel->deleteLater();

    m_notification = new NotificationWidget();
    m_vLayout->addWidget(m_notification);

    ImageLabel *imageLabel = new ImageLabel();
    imageLabel->setScreenshot(m_pixmap);
    imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(imageLabel, &ImageLabel::dragInitiated, this, &FTPUploader::startDrag);
    m_vLayout->addWidget(imageLabel);

    m_hLayout = new QHBoxLayout();
    m_vLayout->addLayout(m_hLayout);

    m_copyUrlButton = new QPushButton(tr("Copy URL"));
    m_openUrlButton = new QPushButton(tr("Open URL"));
    m_toClipboardButton = new QPushButton(tr("Image to Clipboard."));
    m_hLayout->addWidget(m_copyUrlButton);
    m_hLayout->addWidget(m_openUrlButton);
    m_hLayout->addWidget(m_toClipboardButton);

    connect(m_copyUrlButton, &QPushButton::clicked,
            this, &FTPUploader::copyURL);
    connect(m_openUrlButton, &QPushButton::clicked,
            this, &FTPUploader::openURL);
    connect(m_toClipboardButton, &QPushButton::clicked,
            this, &FTPUploader::copyImage);

}

void FTPUploader::openURL() {
    bool successful = QDesktopServices::openUrl(m_imageURL);
    if (!successful) {
        m_notification->showMessage(tr("Unable to open the URL."));
    }
}

void FTPUploader::copyURL() {
    QApplication::clipboard()->setText(m_imageURL.toString());
    m_notification->showMessage(tr("URL copied to clipboard."));
}

void FTPUploader::copyImage() {
    QApplication::clipboard()->setPixmap(m_pixmap);
    m_notification->showMessage(tr("Screenshot copied to clipboard."));
}
