#ifndef FILEPROCESSOR_H
#define FILEPROCESSOR_H

#include <QObject>
#include <QRunnable>
#include <QMutex>
#include <QWaitCondition>
#include <QStringList>
#include <QByteArray>
#include <QAtomicInt>

class FileProcessor : public QObject
{
    Q_OBJECT
public:
    explicit FileProcessor(QObject *parent = nullptr);

    struct Settings {
        QString inputDir;
        QString outputDir;
        QStringList nameFilters;
        bool deleteInput;
        bool overwrite;
        QByteArray xorKey;
    };

    void setSettings(const Settings &settings);

public slots:
    void processFiles();
    void pause();
    void resume();
    void stop();

signals:
    void progressUpdated(int percent, const QString &fileName);
    void logMessage(const QString &msg);
    void finishedAll();

private:
    QString generateOutputFileName(const QString &fileName) const;

    Settings m_settings;
    QMutex m_mutex;
    QWaitCondition m_pauseCondition;
    QAtomicInt m_pauseFlag;
    QAtomicInt m_stopFlag;
};

#endif
