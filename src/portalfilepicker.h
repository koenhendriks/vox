#pragma once

#include "filepicker.h"

// Opens files through xdg-desktop-portal, so the dialog is the desktop's own
// and works the same in a sandbox.
class PortalFilePicker : public FilePicker {
    Q_OBJECT

public:
    using FilePicker::FilePicker;
    void openFiles(const QString &title, const QString &mimeType) override;

private slots:
    void response(uint code, const QVariantMap &results);

private:
    QString m_requestPath;
};
