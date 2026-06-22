#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QTimer>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QPushButton>
#include <QPointer>

class FileProcessor;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void startProcessing();
    void pauseProcessing();
    void resumeProcessing();
    void stopProcessing();
    void onTimerTicked();
    void onWorkerFinished();
    void updateProgress(int percent, const QString &fileName);
    void appendLog(const QString &msg);

private:
    void setupUi();
    void enableControls(bool enable);
    void cleanupWorker();

    QLineEdit *m_inDirEdit;
    QLineEdit *m_outDirEdit;
    QLineEdit *m_maskEdit;
    QCheckBox *m_deleteCheck;
    QComboBox *m_collisionCombo;
    QComboBox *m_modeCombo;
    QSpinBox *m_timerSpin;
    QLineEdit *m_hexKeyEdit;

    QProgressBar *m_progressBar;
    QTextEdit *m_logEdit;

    QPushButton *m_startBtn;
    QPushButton *m_pauseBtn;
    QPushButton *m_resumeBtn;
    QPushButton *m_stopBtn;

    QPointer<QThread> m_workerThread;
    QPointer<FileProcessor> m_processor;
    QTimer *m_timer;

    bool m_isClosing;
};

#endif
