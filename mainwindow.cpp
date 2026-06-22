#include "mainwindow.h"
#include "fileprocessor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QCloseEvent>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    m_workerThread(nullptr),
    m_processor(nullptr),
    m_isClosing(false)
{
    setupUi();
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::onTimerTicked);
}

MainWindow::~MainWindow() {
    stopProcessing();
}

void MainWindow::setupUi() {
    QWidget *central = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);

    m_inDirEdit = new QLineEdit("C:/Test/Input");
    m_outDirEdit = new QLineEdit("C:/Test/Output");

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_maskEdit = new QLineEdit("*.bin *.txt");
#else
    m_maskEdit = new QLineEdit("*.bin *.txt");
#endif

    m_deleteCheck = new QCheckBox("Удалять входные файлы");

    mainLayout->addWidget(new QLabel("Входная директория:"));
    mainLayout->addWidget(m_inDirEdit);
    mainLayout->addWidget(new QLabel("Выходная директория:"));
    mainLayout->addWidget(m_outDirEdit);
    mainLayout->addWidget(new QLabel("Маска файлов (через пробел):"));
    mainLayout->addWidget(m_maskEdit);
    mainLayout->addWidget(m_deleteCheck);

    m_collisionCombo = new QComboBox();
    m_collisionCombo->addItems({"Перезаписать", "Добавить счетчик"});
    mainLayout->addWidget(new QLabel("При совпадении имен:"));
    mainLayout->addWidget(m_collisionCombo);

    m_modeCombo = new QComboBox();
    m_modeCombo->addItems({"Разовый запуск", "По таймеру"});
    m_timerSpin = new QSpinBox();
    m_timerSpin->setSuffix(" сек");
    m_timerSpin->setRange(1, 3600);
    m_timerSpin->setValue(5);

    QHBoxLayout *timerLayout = new QHBoxLayout();
    timerLayout->addWidget(new QLabel("Режим:"));
    timerLayout->addWidget(m_modeCombo);
    timerLayout->addWidget(new QLabel("Интервал:"));
    timerLayout->addWidget(m_timerSpin);
    mainLayout->addLayout(timerLayout);

    m_hexKeyEdit = new QLineEdit("1234567890ABCDEF");
    m_hexKeyEdit->setMaxLength(16);
    mainLayout->addWidget(new QLabel("8-байт XOR ключ (Hex):"));
    mainLayout->addWidget(m_hexKeyEdit);

    m_progressBar = new QProgressBar();
    m_progressBar->setValue(0);
    mainLayout->addWidget(m_progressBar);

    m_logEdit = new QTextEdit();
    m_logEdit->setReadOnly(true);
    mainLayout->addWidget(m_logEdit);

    m_startBtn = new QPushButton("Старт");
    m_pauseBtn = new QPushButton("Пауза");
    m_resumeBtn = new QPushButton("Продолжить");
    m_stopBtn = new QPushButton("Стоп");

    m_pauseBtn->setEnabled(false);
    m_resumeBtn->setEnabled(false);
    m_stopBtn->setEnabled(false);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_pauseBtn);
    btnLayout->addWidget(m_resumeBtn);
    btnLayout->addWidget(m_stopBtn);
    mainLayout->addLayout(btnLayout);

    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::startProcessing);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::pauseProcessing);
    connect(m_resumeBtn, &QPushButton::clicked, this, &MainWindow::resumeProcessing);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::stopProcessing);

    setCentralWidget(central);
    resize(500, 600);
}

void MainWindow::enableControls(bool enable) {
    m_inDirEdit->setEnabled(enable);
    m_outDirEdit->setEnabled(enable);
    m_maskEdit->setEnabled(enable);
    m_deleteCheck->setEnabled(enable);
    m_collisionCombo->setEnabled(enable);
    m_modeCombo->setEnabled(enable);
    m_timerSpin->setEnabled(enable);
    m_hexKeyEdit->setEnabled(enable);
}

void MainWindow::cleanupWorker() {
    if (m_processor) {
        m_processor->disconnect();
        m_processor = nullptr;
    }
    if (m_workerThread) {
        m_workerThread->disconnect();
        m_workerThread = nullptr;
    }
}

void MainWindow::startProcessing() {
    QString hexKey = m_hexKeyEdit->text();
    if (hexKey.length() != 16) {
        QMessageBox::warning(this, "Ошибка", "Ключ должен содержать ровно 16 hex символов (8 байт).");
        return;
    }

    QByteArray key = QByteArray::fromHex(hexKey.toUtf8());
    if (key.size() != 8) {
        QMessageBox::warning(this, "Ошибка", "Неверный формат hex ключа! Используйте только символы 0-9, A-F.");
        return;
    }

    QDir inDir(m_inDirEdit->text());
    if (!inDir.exists()) {
        QMessageBox::warning(this, "Ошибка", "Входная директория не существует!");
        return;
    }

    if (m_modeCombo->currentIndex() == 1) {
        m_timer->start(m_timerSpin->value() * 1000);
        appendLog(QString("Запущен режим мониторинга. Интервал: %1 сек").arg(m_timerSpin->value()));
        enableControls(false);
        m_startBtn->setEnabled(false);
        m_stopBtn->setEnabled(true);
        onTimerTicked();
    } else {
        onTimerTicked();
    }
}

void MainWindow::onTimerTicked() {
    if (m_workerThread && m_workerThread->isRunning()) {
        appendLog("Предыдущая задача еще выполняется, пропускаем тик...");
        return;
    }

    cleanupWorker();

    m_workerThread = new QThread(this);
    m_processor = new FileProcessor();
    m_processor->moveToThread(m_workerThread);

    FileProcessor::Settings s;
    s.inputDir = m_inDirEdit->text();
    s.outputDir = m_outDirEdit->text();

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    s.nameFilters = m_maskEdit->text().split(" ", Qt::SkipEmptyParts);
#else
    s.nameFilters = m_maskEdit->text().split(" ", QString::SkipEmptyParts);
#endif

    s.deleteInput = m_deleteCheck->isChecked();
    s.overwrite = (m_collisionCombo->currentIndex() == 0);
    s.xorKey = QByteArray::fromHex(m_hexKeyEdit->text().toUtf8());

    m_processor->setSettings(s);

    connect(m_workerThread, &QThread::started, m_processor, &FileProcessor::processFiles);
    connect(m_processor, &FileProcessor::progressUpdated, this, &MainWindow::updateProgress);
    connect(m_processor, &FileProcessor::logMessage, this, &MainWindow::appendLog);
    connect(m_processor, &FileProcessor::finishedAll, this, &MainWindow::onWorkerFinished);

    connect(m_processor, &FileProcessor::finishedAll, m_processor, &QObject::deleteLater);
    connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);

    m_workerThread->start();

    if (m_modeCombo->currentIndex() == 0) {
        enableControls(false);
        m_startBtn->setEnabled(false);
    }

    m_pauseBtn->setEnabled(true);
    m_stopBtn->setEnabled(true);
}

void MainWindow::pauseProcessing() {
    if (m_processor) {
        QMetaObject::invokeMethod(m_processor, "pause", Qt::QueuedConnection);
        m_pauseBtn->setEnabled(false);
        m_resumeBtn->setEnabled(true);
    }
}

void MainWindow::resumeProcessing() {
    if (m_processor) {
        QMetaObject::invokeMethod(m_processor, "resume", Qt::QueuedConnection);
        m_pauseBtn->setEnabled(true);
        m_resumeBtn->setEnabled(false);
    }
}

void MainWindow::stopProcessing() {
    m_timer->stop();

    if (m_processor) {
        QMetaObject::invokeMethod(m_processor, "stop", Qt::QueuedConnection);
    }

    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        if (!m_workerThread->wait(5000)) {
            appendLog("ПРЕДУПРЕЖДЕНИЕ: Поток не завершился за 5 секунд, принудительная остановка...");
            m_workerThread->terminate();
            m_workerThread->wait();
        }
    }

    onWorkerFinished();
}

void MainWindow::onWorkerFinished() {
    m_progressBar->setValue(0);
    m_progressBar->setFormat("");

    m_pauseBtn->setEnabled(false);
    m_resumeBtn->setEnabled(false);

    if (m_modeCombo->currentIndex() == 0 || !m_timer->isActive()) {
        m_stopBtn->setEnabled(false);
        m_startBtn->setEnabled(true);
        enableControls(true);
    }

    if (m_isClosing) {
        QTimer::singleShot(100, this, &MainWindow::close);
    }
}

void MainWindow::updateProgress(int percent, const QString &fileName) {
    m_progressBar->setValue(percent);
    m_progressBar->setFormat(QString("%1: %2%").arg(fileName).arg(percent));
}

void MainWindow::appendLog(const QString &msg) {
    m_logEdit->append(msg);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_workerThread && m_workerThread->isRunning()) {
        if (!m_isClosing) {
            m_isClosing = true;
            event->ignore();

            QMessageBox::StandardButton reply = QMessageBox::question(
                this,
                "Завершение работы",
                "Идет обработка файлов. Дождаться завершения?",
                QMessageBox::Yes | QMessageBox::No
                );

            if (reply == QMessageBox::Yes) {
                appendLog("Ожидание завершения обработки перед закрытием...");
                stopProcessing();
            } else {
                appendLog("Принудительное закрытие...");
                m_timer->stop();
                if (m_processor) {
                    QMetaObject::invokeMethod(m_processor, "stop", Qt::QueuedConnection);
                }
                if (m_workerThread) {
                    m_workerThread->quit();
                    if (!m_workerThread->wait(2000)) {
                        m_workerThread->terminate();
                        m_workerThread->wait();
                    }
                }
                event->accept();
            }
        }
    } else {
        event->accept();
    }
}
