#pragma once

#include <QDate>
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QTime>

// One appointment. Dates and times are local wall-clock values, so a meeting
// at 14:00 stays at 14:00 when the clocks change.
struct Event {
    QString id;
    QString title;
    QDate date;
    QTime start;
    QTime end;
    bool allDay = false;
    QString location;
    QString notes;
    QList<int> reminders; // minutes before the start
    QString repeat = "none"; // none, daily, weekly, monthly, yearly

    QJsonObject toJson() const;
    static Event fromJson(const QJsonObject &object);

    bool occursOn(const QDate &day) const;
    QList<QDate> occurrencesBetween(const QDate &from, const QDate &to) const;

    // All-day events have no start time, so their reminders count back from
    // 09:00 on the day: "1 day before" a birthday arrives the morning before,
    // not at midnight.
    QDateTime startAt(const QDate &occurrence) const;
    QDateTime endAt(const QDate &occurrence) const;

    static QStringList repeatKinds();
    static QString newId();
};

// Lenient parsing for what people type: "14", "14:30", "1430", "14h", "14h30".
QTime parseTime(const QString &text);
// The locale's short date, then d/M, d/M/yy, d/M/yyyy, and ISO.
QDate parseDate(const QString &text, const QDate &today = QDate::currentDate());
