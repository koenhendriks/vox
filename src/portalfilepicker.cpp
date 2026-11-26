#include "portalfilepicker.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QRandomGenerator>
#include <QUrl>

namespace {

// The portal's filter type: a name and a list of (kind, pattern), where kind
// 0 is a glob and 1 a MIME type.
struct Filter {
    QString name;
    QList<QPair<uint, QString>> patterns;
};

QDBusArgument &operator<<(QDBusArgument &argument, const Filter &filter) {
    argument.beginStructure();
    argument << filter.name;
    argument.beginArray(qMetaTypeId<QPair<uint, QString>>());
    for (const auto &[kind, pattern] : filter.patterns) {
        argument.beginStructure();
        argument << kind << pattern;
        argument.endStructure();
    }
    argument.endArray();
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Filter &filter) {
    argument.beginStructure();
    argument >> filter.name;
    argument.beginArray();
    filter.patterns.clear();
    while (!argument.atEnd()) {
        QPair<uint, QString> pattern;
        argument.beginStructure();
        argument >> pattern.first >> pattern.second;
        argument.endStructure();
        filter.patterns << pattern;
    }
    argument.endArray();
    argument.endStructure();
    return argument;
}

}

Q_DECLARE_METATYPE(Filter)

void PortalFilePicker::openFiles(const QString &title, const QString &mimeType) {
    qDBusRegisterMetaType<QPair<uint, QString>>();
    qDBusRegisterMetaType<Filter>();
    qDBusRegisterMetaType<QList<Filter>>();

    QDBusConnection bus = QDBusConnection::sessionBus();
    // The reply arrives on a request object whose path follows from our bus
    // name and a token we choose, so we listen there before asking, and a
    // fast reply can't slip past.
    const QString token = QString("vox%1").arg(QRandomGenerator::global()->generate());
    QString sender = bus.baseService().mid(1);
    sender.replace('.', '_');
    if (!m_requestPath.isEmpty())
        bus.disconnect({}, m_requestPath, "org.freedesktop.portal.Request", "Response", this,
                       SLOT(response(uint, QVariantMap)));
    m_requestPath = QString("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
    bus.connect({}, m_requestPath, "org.freedesktop.portal.Request", "Response", this,
                SLOT(response(uint, QVariantMap)));

    const Filter filter{title, {{1u, mimeType}}};
    QVariantMap options{{"handle_token", token},
                        {"multiple", true},
                        {"filters", QVariant::fromValue(QList<Filter>{filter})},
                        {"current_filter", QVariant::fromValue(filter)}};
    QDBusMessage call = QDBusMessage::createMethodCall("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
                                                       "org.freedesktop.portal.FileChooser", "OpenFile");
    call << QString() << title << options;
    const QDBusMessage reply = bus.call(call, QDBus::Block, 5000);
    if (reply.type() == QDBusMessage::ErrorMessage)
        emit failed(tr("The file chooser couldn't open: %1").arg(reply.errorMessage()));
}

void PortalFilePicker::response(uint code, const QVariantMap &results) {
    // 0 is chosen, 1 cancelled, 2 failed; only a choice picks anything.
    if (code != 0)
        return;
    QStringList paths;
    for (const QString &uri : results.value("uris").toStringList()) {
        const QUrl url(uri);
        if (url.isLocalFile())
            paths << url.toLocalFile();
    }
    if (!paths.isEmpty())
        emit picked(paths);
}
