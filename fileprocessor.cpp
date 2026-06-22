#include "fileprocessor.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>

FileProcessor::FileProcessor(QObject *parent)
    : QObject(parent),
    m_pauseFlag(0),
    m_stopFlag(0)
{}

void FileProcessor::setSettings(const Settings &settings) {
    m_settings = settings;
}

void FileProcessor::pause() {
    m_pauseFlag.storeRelaxed(1);
    emit logMessage("Обработка приостановлена...");
}

void FileProcessor::resume() {
    m_pauseFlag.storeRelaxed(0);
    m_pauseCondition.wakeAll();
    emit logMessage("Обработка возобновлена...");
}

void FileProcessor::stop() {
    m_stopFlag.storeRelaxed(1);
    m_pauseFlag.storeRelaxed(0);
    m_pauseCondition.wakeAll();
}

QString FileProcessor::generateOutputFileName(const QString &fileName) const {
    QFileInfo fi(fileName);
    QString outPath = QDir(m_settings.outputDir).filePath(fi.fileName());

    if (m_settings.overwrite || !QFile::exists(outPath)) {
        return outPath;
    }

    int counter = 1;
    QString base = fi.completeBaseName();
    QString ext = fi.suffix().isEmpty() ? "" : "." + fi.suffix();

    while (true) {
        QString newName = QString("%1_%2%3").arg(base).arg(counter).arg(ext);
        outPath = QDir(m_settings.outputDir).filePath(newName);
        if (!QFile::exists(outPath)) break;
        counter++;
    }
    return outPath;
}

void FileProcessor::processFiles() {
    QDir outDir(m_settings.outputDir);
    if (!outDir.exists()) {
        if (!QDir().mkpath(m_settings.outputDir)) {
            emit logMessage("ОШИБКА: Не удалось создать выходную директорию!");
            emit finishedAll();
            return;
        }
    }

    QDir inDir(m_settings.inputDir);

    if (!inDir.exists()) {
        emit logMessage("ОШИБКА: Входная директория не существует!");
        emit finishedAll();
        return;
    }

    QStringList files = inDir.entryList(m_settings.nameFilters, QDir::Files);

    if (files.isEmpty()) {
        emit logMessage("Нет файлов для обработки.");
        emit finishedAll();
        return;
    }

    const qint64 CHUNK_SIZE = 4 * 1024 * 1024;

    for (const QString &file : files) {
        if (m_stopFlag.loadRelaxed()) break;

        QString inPath = inDir.filePath(file);
        QString outPath = generateOutputFileName(inPath);

        QFile inFile(inPath);
        QFile outFile(outPath);

        if (!inFile.open(QIODevice::ReadOnly)) {
            emit logMessage("Ошибка открытия на чтение: " + file);
            continue;
        }

        if (!outFile.open(QIODevice::WriteOnly)) {
            emit logMessage("Ошибка открытия на запись: " + outPath);
            inFile.close();
            continue;
        }

        emit logMessage("Обработка файла: " + file);
        qint64 totalSize = inFile.size();
        qint64 processedSize = 0;

        while (!inFile.atEnd()) {
            if (m_stopFlag.loadRelaxed()) break;

            while (m_pauseFlag.loadRelaxed()) {
                QMutexLocker locker(&m_mutex);
                m_pauseCondition.wait(&m_mutex, 100);
                if (m_stopFlag.loadRelaxed()) break;
            }
            if (m_stopFlag.loadRelaxed()) break;

            QByteArray chunk = inFile.read(CHUNK_SIZE);

            if (chunk.isEmpty()) break;

            for (qsizetype i = 0; i < chunk.size(); ++i) {
                chunk[i] = chunk[i] ^ m_settings.xorKey[(processedSize + i) % 8];
            }

            qint64 written = outFile.write(chunk);
            if (written != chunk.size()) {
                emit logMessage("ОШИБКА ЗАПИСИ: " + file + " - возможно, диск заполнен!");
                break;
            }

            processedSize += chunk.size();

            int percent = totalSize > 0 ? static_cast<int>((processedSize * 100) / totalSize) : 100;
            emit progressUpdated(percent, file);
        }

        inFile.close();
        outFile.close();

        if (m_stopFlag.loadRelaxed()) {
            emit logMessage("Операция прервана.");
            if (QFile::exists(outPath)) {
                QFile::remove(outPath);
            }
            break;
        }

        if (m_settings.deleteInput) {
            if (QFile::remove(inPath)) {
                emit logMessage("Файл удален: " + file);
            } else {
                emit logMessage("Не удалось удалить: " + file);
            }
        }
    }

    if (!m_stopFlag.loadRelaxed()) {
        emit logMessage("Все файлы обработаны.");
    }
    emit finishedAll();
}
