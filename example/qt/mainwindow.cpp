// You may need to build the project (run Qt uic code generator) to get "ui_MainWindow.h" resolved

#include "mainwindow.hpp"
#include "ui_MainWindow.h"

#include <QThread>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , _lifetime{std::make_shared<char>('a')} {
    ui->setupUi(this);

    connect(ui->startButton, &QPushButton::clicked, this, &MainWindow::onStartButtonClicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &MainWindow::onCancelButtonClicked);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::onStartButtonClicked() {
    onStartButtonClickedAsync().start();
}

void MainWindow::onCancelButtonClicked() {
    // @TODO cancellation support for tasks via internal stop-state (in flags)
    // still needs to be implemented
}

asyncrt::qt::Task<void> MainWindow::onStartButtonClickedAsync() {
    const auto weak = weak_ref(); // used to check if dialog expired
    // capture current context
    const asyncrt::qt::Context ui_ctx{}; // @TODO this needs to capture the application dispatcher somehow. check for application thread and if so, return application dispatcher

    ui->console->append("Task started");

    co_await asyncrt::qt::resume_on_threadpool();

    // @TODO download stuff from an URL or something
    QThread::msleep(1000);

    co_await asyncrt::core::resume_on(ui_ctx);

    if (weak.expired()) {
        co_return;
    }

    ui->console->append("Task finished");
    co_return;
}
