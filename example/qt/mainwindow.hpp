#ifndef ASYNCRT_RUNTIME_MAINWINDOW_HPP
#define ASYNCRT_RUNTIME_MAINWINDOW_HPP

#include <QMainWindow>
#include <asyncrt/qt/task.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void onStartButtonClicked();
    void onCancelButtonClicked();

    asyncrt::qt::Task<void> onStartButtonClickedAsync();

    auto weak_ref() -> std::weak_ptr<char> { return {_lifetime}; }
private:
    std::shared_ptr<char> _lifetime;
    Ui::MainWindow *ui;
};


#endif //ASYNCRT_RUNTIME_MAINWINDOW_HPP
