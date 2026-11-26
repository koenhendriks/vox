#pragma once

#include <QObject>
#include <QStringList>

// Asks the user for files. Behind an interface so tests can pick without a
// desktop.
class FilePicker : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    // `mimeType` narrows the choice, such as "audio/*".
    virtual void openFiles(const QString &title, const QString &mimeType) = 0;

signals:
    void picked(const QStringList &paths);
    void failed(const QString &message);
};
