// You may need to build the project (run Qt uic code generator) to get "ui_MainWindow.h" resolved

#include "mainwindow.hpp"
#include "ui_MainWindow.h"

#include <QThread>
#include <QNetworkReply>
#include <QMetaEnum>
#include <QMessageBox>
#include <QThreadPool>
#include <expected>

#include "asyncrt/qt/context.hpp"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , _lifetime{std::make_shared<char>('a')} {
    ui->setupUi(this);

    connect(ui->startButton, &QPushButton::clicked, this, &MainWindow::onStartButtonClicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &MainWindow::onCancelButtonClicked);
    connect(this, &MainWindow::taskExceptionOccurred, this, &MainWindow::onTaskExceptionOccurred, Qt::QueuedConnection); // signal can come from different thread
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::onStartButtonClicked() {
    auto task = onStartButtonClickedAsync();
    task.start(_stop_source.get_token());

    QThreadPool::globalInstance()->start([this, task = std::move(task)] mutable {
        const auto lifetime = weak_ref();
        try {
            task.unwrap();
        } catch (const std::exception &e) {
            if (!lifetime.expired()) {
                emit taskExceptionOccurred(e);
            }
        }
    });
}

void MainWindow::onCancelButtonClicked() {
    _stop_source.request_stop();
}

asyncrt::qt::Task<std::expected<QString, QString>> downloadFile() {
    QNetworkAccessManager net;
    const auto reply = net.get(QNetworkRequest{QUrl("https://microsoftedge.github.io/Demos/json-dummy-data/5MB.json")});
    if (!reply) {
        co_return std::unexpected{"Could not send download request"};
    }

    std::promise<void> barrier;
    std::expected<QString, QString> result;

    QObject::connect(reply, &QNetworkReply::finished, [&barrier] mutable { barrier.set_value(); });

    switch (barrier.get_future().wait_for(std::chrono::seconds(10))) {
        case std::future_status::timeout:
            result = std::unexpected{"Request timed out"};
            break;
        case std::future_status::ready: {
            if (const auto error = reply->error();
                QNetworkReply::NetworkError::NoError != error) {
                result = QString::fromUtf8(reply->readAll());
            } else {
                result = std::unexpected{QString{"Could not download file: %1"}.arg(reply->errorString())};
            }
            break;
        }
        default:
            std::unreachable();
            break;
    }
    reply->deleteLater();
    co_return result;
}

asyncrt::qt::Task<void> MainWindow::onStartButtonClickedAsync() {
    const auto weak = weak_ref(); // used to check if dialog expired
    // capture current context
    const asyncrt::qt::Context ui_ctx{};

    ui->console->append("Task started");

    // this is run on a worker thread
    co_await asyncrt::qt::resume_on_threadpool();
    //const auto text = co_await downloadFile();
    QThread::msleep(5000);
    const std::expected<QString, QString> text{"Foo"};
    co_await asyncrt::qt::resume_on_application_thread();
    if (weak.expired()) {
        co_return;
    }

    if (text) {
        ui->console->setText(*text);
    } else {
        QMessageBox::warning(this, "Network error", text.error());
    }

    // const auto downloadProgressConnection = connect(reply, &QNetworkReply::downloadProgress, this, &MainWindow::onDownloadProgress, Qt::QueuedConnection);
    // disconnect(downloadProgressConnection);
}

void MainWindow::onTaskExceptionOccurred(const std::exception &e){
    QMessageBox::critical(this, "Task Exception", e.what());
}
