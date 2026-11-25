#pragma once

#include <QList>
#include <QString>

#include "event.h"

// The calendar on disk: one JSON file the window and the command line share.
class Store {
public:
    explicit Store(const QString &path = defaultPath());

    QString path() const { return m_path; }
    bool load(QString *error = nullptr);
    bool save(QString *error = nullptr) const;

    QList<Event> events;
    // New appointments start with the reminders the last one was saved with.
    QList<int> defaultReminders{15};

    int indexOf(const QString &id) const;

    static QString defaultPath();
    static QString defaultSentPath();

private:
    QString m_path;
};
