#include "reminders.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

#include "format.h"

bool SentLog::load(const QString &path) {
    m_entries.clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return !file.exists();
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    for (auto it = object.begin(); it != object.end(); ++it)
        m_entries.insert(it.key(), it.value().toInteger());
    return true;
}

bool SentLog::save(const QString &path) const {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QJsonObject object;
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it)
        object.insert(it.key(), it.value());
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(object).toJson(QJsonDocument::Compact)) >= 0
        && file.commit();
}

void SentLog::prune(const QDateTime &now) {
    const qint64 cutoff = now.addDays(-40).toSecsSinceEpoch();
    for (auto it = m_entries.begin(); it != m_entries.end();)
        it = it.value() < cutoff ? m_entries.erase(it) : std::next(it);
}

QList<DueReminder> dueReminders(const QList<Event> &events, const QDateTime &now, SentLog &log) {
    QList<DueReminder> due;
    for (const Event &event : events) {
        if (event.reminders.isEmpty())
            continue;
        const int furthest = *std::max_element(event.reminders.begin(), event.reminders.end());
        const QDate to = now.date().addDays(furthest / 1440 + 2);
        for (const QDate &occurrence : event.occurrencesBetween(now.date().addDays(-1), to)) {
            const QDateTime start = event.startAt(occurrence);
            // An appointment without an end counts as over ten minutes in.
            const QDateTime over = std::max(event.endAt(occurrence), start.addSecs(600));
            if (over <= now)
                continue;

            int latest = -1;
            for (int minutes : event.reminders) {
                const QDateTime fireAt = start.addSecs(-60LL * minutes);
                // The start time is part of the key, so moving an appointment
                // re-arms its reminders.
                const QString key = QString("%1|%2|%3|%4").arg(event.id, occurrence.toString(Qt::ISODate),
                                                              start.time().toString("HH:mm")).arg(minutes);
                if (fireAt > now || log.contains(key))
                    continue;
                log.insert(key, fireAt);
                if (latest < 0 || minutes < latest)
                    latest = minutes;
            }
            if (latest >= 0)
                due.append({event, occurrence, latest});
        }
    }
    return due;
}

bool notify(const DueReminder &reminder, const QDateTime &now, QString *error) {
    QString body = Format::when(reminder.event, reminder.occurrence, now);
    if (!reminder.event.location.isEmpty())
        body += " · " + reminder.event.location;

    // Omarchy's own notifications look like the rest of the desktop and open
    // the app when clicked; plain notify-send works anywhere else.
    QString program = QStandardPaths::findExecutable("omarchy-notification-send");
    QStringList arguments;
    if (!program.isEmpty()) {
        arguments = {"--app-name", "Agenda", "-g", QString::fromUtf8("\U000F00ED"), "-u", "normal", "-i", "agenda",
                     reminder.event.title, body, "--exec", QCoreApplication::applicationFilePath(), "open"};
    } else {
        program = QStandardPaths::findExecutable("notify-send");
        arguments = {"-a", "Agenda", "-i", "agenda", "-u", "normal", reminder.event.title, body};
    }
    if (program.isEmpty()) {
        if (error)
            *error = QCoreApplication::translate("Agenda", "Neither omarchy-notification-send nor notify-send is installed");
        return false;
    }
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForFinished(10000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error)
            *error = QString::fromUtf8(process.readAllStandardError()).trimmed();
        return false;
    }
    return true;
}
