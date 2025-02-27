#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

class MainWindow : public QMainWindow {
    Q_OBJECT  // 重点：必须加上这个宏

public:
    MainWindow();

private slots:
    void onFindLog();

private:
    QLineEdit* datetimeInput;
    QLabel* resultLabel;
};

#endif // MAINWINDOW_H
