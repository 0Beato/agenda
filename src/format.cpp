#include "format.h"

#include <QCoreApplication>
#include <QLocale>

namespace {
QString tr(const char *text, int n = -1) {
    return QCoreApplication::translate("Agenda", text, nullptr, n);
}
}

namespace Format {

QList<int> reminderChoices() {
    return {0, 5, 10, 15, 30, 60, 120, 1440, 2880, 10080};
}

QString reminder(int minutes) {
    if (minutes == 0)
        return tr("At the start time");
    if (minutes % 10080 == 0)
        return tr("%n weeks before", minutes / 10080);
    if (minutes % 1440 == 0)
        return tr("%n days before", minutes / 1440);
    if (minutes % 60 == 0)
        return tr("%n hours before", minutes / 60);
    return tr("%n minutes before", minutes);
}

QString reminderShort(int minutes) {
    if (minutes == 0)
        return tr("At start");
    if (minutes % 10080 == 0)
        return tr("%n wk", minutes / 10080);
    if (minutes % 1440 == 0)
        return tr("%n d", minutes / 1440);
    if (minutes % 60 == 0)
        return tr("%n h", minutes / 60);
    return tr("%n min", minutes);
}

QString reminders(const QList<int> &minutes) {
    if (minutes.isEmpty())
        return tr("No reminder");
    QStringList parts;
    for (int m : minutes)
        parts.append(reminderShort(m));
    return parts.join(", ");
}

QString timeRange(const Event &event) {
    if (event.allDay)
        return tr("All day");
    const QLocale locale;
    if (event.end <= event.start)
        return locale.toString(event.start, QLocale::ShortFormat);
    return tr("%1 – %2").arg(locale.toString(event.start, QLocale::ShortFormat),
                             locale.toString(event.end, QLocale::ShortFormat));
}

QString when(const Event &event, const QDate &occurrence, const QDateTime &now, bool compact) {
    const QLocale locale;
    const qint64 days = now.date().daysTo(occurrence);
    QString day;
    if (days == 0)
        day = tr("Today");
    else if (days == 1)
        day = tr("Tomorrow");
    else if (days == -1)
        day = tr("Yesterday");
    else if (compact)
        day = locale.toString(occurrence, "ddd, d MMM");
    else
        day = locale.toString(occurrence, QLocale::LongFormat);
    if (event.allDay)
        return day;
    const QString time = locale.toString(event.start, QLocale::ShortFormat);
    if (days == 0) {
        const qint64 minutes = now.secsTo(event.startAt(occurrence)) / 60;
        if (minutes > 0 && minutes < 60)
            return tr("Today at %1, in %n minutes", minutes).arg(time);
        if (minutes <= 0 && event.endAt(occurrence) > now)
            return tr("Now, since %1").arg(time);
    }
    return tr("%1 at %2").arg(day, time);
}

QString repeat(const QString &kind) {
    if (kind == "daily")
        return tr("Every day");
    if (kind == "weekly")
        return tr("Every week");
    if (kind == "monthly")
        return tr("Every month");
    if (kind == "yearly")
        return tr("Every year");
    return tr("Does not repeat");
}

}
