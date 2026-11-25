#include "backend.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocale>
#include <QProcess>
#include <QStandardPaths>
#include <algorithm>

#include "format.h"

namespace {
QByteArray fingerprint(const QList<Event> &events) {
    QByteArray bytes;
    for (const auto &event : events)
        bytes += QJsonDocument(event.toJson()).toJson(QJsonDocument::Compact);
    return bytes;
}
}

Backend::Backend(const QString &path, QObject *parent) : QObject(parent), m_store(path) {
    QString error;
    if (!m_store.load(&error))
        m_loadProblem = error;
    setProblem(m_loadProblem);

    // The command line and other windows write the same file; pick up their
    // changes, debounced because a rename arrives as several events.
    m_reload.setSingleShot(true);
    m_reload.setInterval(120);
    connect(&m_reload, &QTimer::timeout, this, [this] {
        const QByteArray before = fingerprint(m_store.events);
        Store fresh(m_store.path());
        QString error;
        if (fresh.load(&error)) {
            m_loadProblem.clear();
            if (fingerprint(fresh.events) != before) {
                // Someone else changed the calendar, so undoing a step taken
                // before that would quietly throw their change away.
                m_store.events = fresh.events;
                m_store.defaultReminders = fresh.defaultReminders;
                m_undo.clear();
                m_redo.clear();
            }
        } else {
            m_loadProblem = error;
        }
        setProblem(m_loadProblem.isEmpty() ? m_reminderProblem : m_loadProblem);
        watch();
        emit changed();
    });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_reload, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_reload, qOverload<>(&QTimer::start));
    watch();

    // Relative times ("in 12 minutes") and "today" move on by themselves.
    m_clock.setInterval(30000);
    connect(&m_clock, &QTimer::timeout, this, [this] {
        if (m_today != QDate::currentDate()) {
            if (m_selected == m_today)
                m_selected = QDate::currentDate();
            m_today = QDate::currentDate();
        }
        emit changed();
    });
    m_clock.start();
}

void Backend::watch() {
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);
    const QString directory = QFileInfo(m_store.path()).absolutePath();
    if (QFileInfo::exists(directory))
        m_watcher.addPath(directory);
    if (QFileInfo::exists(m_store.path()))
        m_watcher.addPath(m_store.path());
}

void Backend::refresh() {
    emit changed();
}

void Backend::setProblem(const QString &problem) {
    if (problem == m_problem)
        return;
    m_problem = problem;
    emit problemChanged();
}

QString Backend::monthTitle() const {
    const QLocale locale;
    return tr("%1 %2").arg(locale.standaloneMonthName(m_selected.month(), QLocale::LongFormat))
        .arg(m_selected.year());
}

QString Backend::dayTitle() const {
    const QLocale locale;
    QString full = locale.toString(m_selected, QLocale::LongFormat);
    if (m_selected == m_today)
        return tr("Today, %1").arg(full);
    if (m_selected == m_today.addDays(1))
        return tr("Tomorrow, %1").arg(full);
    // Some languages write weekdays in lower case; a heading starts upper.
    if (!full.isEmpty())
        full[0] = full[0].toUpper();
    return full;
}

QVariantList Backend::weekdays() const {
    const QLocale locale;
    QVariantList names;
    const int first = locale.firstDayOfWeek();
    for (int i = 0; i < 7; ++i)
        names.append(locale.standaloneDayName((first - 1 + i) % 7 + 1, QLocale::ShortFormat));
    return names;
}

QVariantList Backend::monthCells() const {
    const QDate first(m_selected.year(), m_selected.month(), 1);
    const int offset = (first.dayOfWeek() - QLocale().firstDayOfWeek() + 7) % 7;
    const QDate start = first.addDays(-offset), end = start.addDays(41);

    QHash<QDate, int> counts;
    for (const Event &event : m_store.events)
        for (const QDate &day : event.occurrencesBetween(start, end))
            ++counts[day];

    QVariantList cells;
    for (QDate day = start; day <= end; day = day.addDays(1)) {
        cells.append(QVariantMap{
            {"date", day.toString(Qt::ISODate)},
            {"day", QLocale().toString(day.day())},
            {"inMonth", day.month() == m_selected.month()},
            {"today", day == m_today},
            {"selected", day == m_selected},
            {"count", counts.value(day)},
        });
    }
    return cells;
}

QVariantMap Backend::occurrenceMap(const Event &event, const QDate &occurrence) const {
    const QDateTime now = QDateTime::currentDateTime();
    return {
        {"id", event.id},
        {"title", event.title},
        {"date", occurrence.toString(Qt::ISODate)},
        {"time", Format::timeRange(event)},
        {"when", Format::when(event, occurrence, now, true)},
        {"location", event.location},
        {"notes", event.notes},
        {"allDay", event.allDay},
        {"reminders", Format::reminders(event.reminders)},
        {"hasReminders", !event.reminders.isEmpty()},
        {"repeats", event.repeat != "none"},
        {"repeat", Format::repeat(event.repeat)},
        {"past", std::max(event.endAt(occurrence), event.startAt(occurrence).addSecs(600)) <= now},
        {"now", event.startAt(occurrence) <= now && event.endAt(occurrence) > now},
    };
}

static void sortByStart(QList<QPair<QDateTime, QVariantMap>> &items) {
    std::stable_sort(items.begin(), items.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
}

QVariantList Backend::dayEvents() const {
    QList<QPair<QDateTime, QVariantMap>> items;
    for (const Event &event : m_store.events)
        if (event.occursOn(m_selected))
            // All-day events lead the day.
            items.append({event.allDay ? QDateTime(m_selected, QTime(0, 0)) : event.startAt(m_selected),
                          occurrenceMap(event, m_selected)});
    sortByStart(items);
    QVariantList list;
    for (const auto &item : items)
        list.append(item.second);
    return list;
}

QVariantList Backend::upcoming() const {
    const QDateTime now = QDateTime::currentDateTime();
    QList<QPair<QDateTime, QVariantMap>> items;
    for (const Event &event : m_store.events)
        for (const QDate &day : event.occurrencesBetween(m_today, m_today.addDays(14)))
            if (event.endAt(day) > now)
                items.append({event.allDay ? QDateTime(day, QTime(0, 0)) : event.startAt(day), occurrenceMap(event, day)});
    sortByStart(items);
    QVariantList list;
    for (int i = 0; i < items.size() && i < 5; ++i)
        list.append(items[i].second);
    return list;
}

QVariantList Backend::reminderChoices() const {
    QVariantList list;
    for (int minutes : Format::reminderChoices())
        list.append(QVariantMap{{"minutes", minutes}, {"label", Format::reminder(minutes)},
                                {"short", Format::reminderShort(minutes)}});
    return list;
}

QVariantList Backend::repeatChoices() const {
    QVariantList list;
    for (const QString &kind : Event::repeatKinds())
        list.append(QVariantMap{{"value", kind}, {"label", Format::repeat(kind)}});
    return list;
}

void Backend::select(const QString &isoDate) {
    const QDate date = QDate::fromString(isoDate, Qt::ISODate);
    if (!date.isValid() || date == m_selected)
        return;
    m_selected = date;
    emit changed();
}

void Backend::moveDays(int days) {
    m_selected = m_selected.addDays(days);
    emit changed();
}

void Backend::moveMonths(int months) {
    m_selected = m_selected.addMonths(months);
    emit changed();
}

void Backend::goToday() {
    m_selected = m_today = QDate::currentDate();
    emit changed();
}

QString Backend::formatDate(const QString &isoDate) const {
    return QLocale().toString(QDate::fromString(isoDate, Qt::ISODate), QLocale::ShortFormat);
}

QString Backend::reminderLabel(int minutes) const {
    return Format::reminder(minutes);
}

static QVariantList toVariant(const QList<int> &values) {
    QVariantList list;
    for (int v : values)
        list.append(v);
    return list;
}

QVariantMap Backend::draft() const {
    QTime start(9, 0);
    if (m_selected == m_today) {
        const QTime now = QTime::currentTime();
        start = QTime(std::min(now.hour() + 1, 23), 0);
    }
    return {
        {"id", QString()},
        {"title", QString()},
        {"date", QLocale().toString(m_selected, QLocale::ShortFormat)},
        {"allDay", false},
        {"start", start.toString("HH:mm")},
        {"end", (start.hour() == 23 ? QTime(23, 59) : start.addSecs(3600)).toString("HH:mm")},
        {"location", QString()},
        {"notes", QString()},
        {"reminders", toVariant(m_store.defaultReminders)},
        {"repeat", "none"},
    };
}

QVariantMap Backend::eventById(const QString &id) const {
    const int index = m_store.indexOf(id);
    if (index < 0)
        return {};
    const Event &event = m_store.events[index];
    return {
        {"id", event.id},
        {"title", event.title},
        {"date", QLocale().toString(event.date, QLocale::ShortFormat)},
        {"allDay", event.allDay},
        {"start", event.allDay ? QString("09:00") : event.start.toString("HH:mm")},
        {"end", event.allDay ? QString("10:00") : event.end.toString("HH:mm")},
        {"location", event.location},
        {"notes", event.notes},
        {"reminders", toVariant(event.reminders)},
        {"repeat", event.repeat},
    };
}

QString Backend::save(const QVariantMap &fields) {
    Event event;
    event.id = fields.value("id").toString();
    event.title = fields.value("title").toString().trimmed();
    if (event.title.isEmpty())
        return tr("Give the appointment a title");
    event.date = parseDate(fields.value("date").toString(), m_today);
    if (!event.date.isValid())
        return tr("“%1” isn't a date").arg(fields.value("date").toString());
    event.allDay = fields.value("allDay").toBool();
    if (!event.allDay) {
        event.start = parseTime(fields.value("start").toString());
        if (!event.start.isValid())
            return tr("“%1” isn't a time").arg(fields.value("start").toString());
        const QString endText = fields.value("end").toString().trimmed();
        // Without an end, an hour, but never past midnight.
        if (endText.isEmpty())
            event.end = event.start.hour() == 23 ? QTime(23, 59) : event.start.addSecs(3600);
        else
            event.end = parseTime(endText);
        if (!event.end.isValid())
            return tr("“%1” isn't a time").arg(endText);
        if (event.end < event.start)
            return tr("It ends before it starts");
    }
    event.location = fields.value("location").toString().trimmed();
    event.notes = fields.value("notes").toString().trimmed();
    for (const QVariant &value : fields.value("reminders").toList())
        if (value.toInt() >= 0 && !event.reminders.contains(value.toInt()))
            event.reminders.append(value.toInt());
    std::sort(event.reminders.begin(), event.reminders.end());
    event.repeat = Event::repeatKinds().contains(fields.value("repeat").toString())
                 ? fields.value("repeat").toString() : QString("none");

    const QList<Event> before = m_store.events;
    const int index = event.id.isEmpty() ? -1 : m_store.indexOf(event.id);
    if (index >= 0) {
        m_store.events[index] = event;
    } else {
        event.id = Event::newId();
        m_store.events.append(event);
    }
    m_store.defaultReminders = event.reminders;
    m_selected = event.date;
    m_lastSavedId = event.id;
    commit(before);
    return {};
}

bool Backend::remove(const QString &id) {
    const int index = m_store.indexOf(id);
    if (index < 0)
        return false;
    const QList<Event> before = m_store.events;
    m_store.events.removeAt(index);
    commit(before);
    return true;
}

void Backend::commit(const QList<Event> &before) {
    QString error;
    if (!m_store.save(&error)) {
        m_store.events = before;
        setProblem(error);
        emit changed();
        return;
    }
    m_undo.append(before);
    m_redo.clear();
    setProblem(m_reminderProblem);
    emit changed();
}

void Backend::undo() {
    if (m_undo.isEmpty())
        return;
    const QList<Event> current = m_store.events;
    m_store.events = m_undo.takeLast();
    QString error;
    if (!m_store.save(&error)) {
        m_undo.append(m_store.events);
        m_store.events = current;
        setProblem(error);
    } else {
        m_redo.append(current);
    }
    emit changed();
}

void Backend::redo() {
    if (m_redo.isEmpty())
        return;
    const QList<Event> current = m_store.events;
    m_store.events = m_redo.takeLast();
    QString error;
    if (!m_store.save(&error)) {
        m_redo.append(m_store.events);
        m_store.events = current;
        setProblem(error);
    } else {
        m_undo.append(current);
    }
    emit changed();
}

void Backend::ensureReminders() {
    auto run = [](const QStringList &arguments, QString *output = nullptr) {
        QProcess process;
        process.start("systemctl", QStringList{"--user"} + arguments);
        if (!process.waitForFinished(5000))
            return -1;
        if (output)
            *output = QString::fromUtf8(process.readAllStandardOutput() + process.readAllStandardError()).trimmed();
        return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
    };

    QString problem;
    if (QStandardPaths::findExecutable("omarchy-notification-send").isEmpty()
        && QStandardPaths::findExecutable("notify-send").isEmpty()) {
        problem = tr("Reminders can't be shown: neither omarchy-notification-send nor notify-send is installed.");
    } else if (QStandardPaths::findExecutable("systemctl").isEmpty()) {
        problem = tr("Reminders are off: systemd isn't available to run them.");
    } else if (run({"is-active", "--quiet", "agenda-reminders.timer"}) != 0) {
        QString output;
        if (run({"enable", "--now", "agenda-reminders.timer"}, &output) != 0)
            problem = tr("Reminders are off: the agenda-reminders timer can't start (%1). Install Agenda with bin/install.")
                          .arg(output.isEmpty() ? tr("systemctl failed") : output.section('\n', 0, 0));
    }
    m_reminderProblem = problem;
    setProblem(m_loadProblem.isEmpty() ? m_reminderProblem : m_loadProblem);
}
