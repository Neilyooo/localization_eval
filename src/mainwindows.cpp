#include "mainwindows.h"
#include "LogFinder.h"

MainWindow::MainWindow() {
    setWindowTitle("日志查找工具");
    resize(400, 200);

    datetimeInput = new QLineEdit(this);
    datetimeInput->setPlaceholderText("输入日期时间 (例如: 25-02-25-235400)");
    datetimeInput->setGeometry(50, 50, 300, 30);

    QPushButton* findButton = new QPushButton("查找日志", this);
    findButton->setGeometry(150, 100, 100, 30);

    resultLabel = new QLabel(this);
    resultLabel->setGeometry(50, 150, 300, 30);

    connect(findButton, &QPushButton::clicked, this, &MainWindow::onFindLog);
}

void MainWindow::onFindLog() {
    std::string vehicle_id = getenv("USER");
    LogFinder logFinder(vehicle_id);

    std::string datetime = datetimeInput->text().toStdString();
    std::string result = logFinder.findLog(datetime);

    resultLabel->setText(QString::fromStdString(result));
}

