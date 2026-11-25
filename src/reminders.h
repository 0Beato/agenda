#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>

#include "event.h"

// The reminders already sent, so the minutely check never repeats one.
class SentLog {
public:
    bool load(const QString &path);
    bool save(const QString &path) const;
    bool contains(const QString &key) const { return m_entries.contains(key); }
    void insert(const QString &key, const QDateTime &fireAt) { m_entries.insert(key, fireAt.toSecsSinceEpoch()); }
    void prune(const QDateTime &now);
    int size() const { return m_entries.size(); }

private:
    QHash<QString, qint64> m_entries;
};

struct DueReminder {
    Event event;
    QDate occurrence;
    int minutes = 0;
};

// Every reminder whose time has come and that hasn't been sent, one per
// appointment. A reminder missed while the computer was off still arrives,
// as long as the appointment hasn't ended; when several are overdue for the
// same appointment only the latest is shown. All of them are marked sent.
QList<DueReminder> dueReminders(const QList<Event> &events, const QDateTime &now, SentLog &log);

// Shows one reminder as an Omarchy notification; false with a reason when
// the desktop can't show it.
bool notify(const DueReminder &reminder, const QDateTime &now, QString *error = nullptr);
