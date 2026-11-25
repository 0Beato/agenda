#include "store.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

Store::Store(const QString &path) : m_path(path) {}

QString Store::defaultPath() {
    const QString override = qEnvironmentVariable("AGENDA_DATA");
    if (!override.isEmpty())
        return override;
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/agenda/events.json";
}

QString Store::defaultSentPath() {
    const QString override = qEnvironmentVariable("AGENDA_STATE");
    if (!override.isEmpty())
        return override;
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + "/agenda/sent.json";
}

int Store::indexOf(const QString &id) const {
    for (int i = 0; i < events.size(); ++i)
        if (events[i].id == id)
            return i;
    return -1;
}

bool Store::load(QString *error) {
    events.clear();
    QFile file(m_path);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QCoreApplication::translate("Store", "Can't read %1: %2").arg(m_path, file.errorString());
        return false;
    }
    QJsonParseError parse;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QCoreApplication::translate("Store", "%1 is damaged: %2").arg(m_path, parse.errorString());
        return false;
    }
    const QJsonObject root = document.object();
    for (const auto &value : root.value("events").toArray()) {
        const Event event = Event::fromJson(value.toObject());
        if (event.date.isValid() && !event.title.isEmpty())
            events.append(event);
    }
    if (root.contains("defaultReminders")) {
        defaultReminders.clear();
        for (const auto &value : root.value("defaultReminders").toArray())
            if (value.toInt(-1) >= 0)
                defaultReminders.append(value.toInt());
    }
    return true;
}

bool Store::save(QString *error) const {
    QDir().mkpath(QFileInfo(m_path).absolutePath());

    QList<Event> sorted = events;
    std::sort(sorted.begin(), sorted.end(), [](const Event &a, const Event &b) {
        return a.startAt(a.date) < b.startAt(b.date);
    });
    QJsonArray list;
    for (const auto &event : sorted)
        list.append(event.toJson());
    QJsonArray reminders;
    for (int minutes : defaultReminders)
        reminders.append(minutes);
    const QJsonObject root{{"version", 1}, {"events", list}, {"defaultReminders", reminders}};

    // Written beside the target and renamed over it, so a crash mid-save never
    // leaves a half-written calendar.
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0
        || !file.commit()) {
        if (error)
            *error = QCoreApplication::translate("Store", "Can't save %1: %2").arg(m_path, file.errorString());
        return false;
    }
    return true;
}
