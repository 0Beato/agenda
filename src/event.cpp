#include "event.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QLocale>
#include <QRegularExpression>
#include <QUuid>
#include <algorithm>

QJsonObject Event::toJson() const {
    QJsonArray offsets;
    for (int minutes : reminders)
        offsets.append(minutes);
    QJsonObject object{
        {"id", id},
        {"title", title},
        {"date", date.toString(Qt::ISODate)},
        {"allDay", allDay},
        {"reminders", offsets},
        {"repeat", repeat},
    };
    if (!allDay) {
        object.insert("start", start.toString("HH:mm"));
        object.insert("end", end.toString("HH:mm"));
    }
    if (!location.isEmpty())
        object.insert("location", location);
    if (!notes.isEmpty())
        object.insert("notes", notes);
    return object;
}

Event Event::fromJson(const QJsonObject &object) {
    Event event;
    event.id = object.value("id").toString();
    if (event.id.isEmpty())
        event.id = newId();
    event.title = object.value("title").toString();
    event.date = QDate::fromString(object.value("date").toString(), Qt::ISODate);
    event.allDay = object.value("allDay").toBool();
    event.start = QTime::fromString(object.value("start").toString(), "HH:mm");
    event.end = QTime::fromString(object.value("end").toString(), "HH:mm");
    if (!event.allDay && !event.start.isValid())
        event.allDay = true;
    if (!event.allDay && (!event.end.isValid() || event.end < event.start))
        event.end = event.start;
    event.location = object.value("location").toString();
    event.notes = object.value("notes").toString();
    for (const auto &value : object.value("reminders").toArray())
        if (value.toInt(-1) >= 0 && !event.reminders.contains(value.toInt()))
            event.reminders.append(value.toInt());
    std::sort(event.reminders.begin(), event.reminders.end());
    event.repeat = object.value("repeat").toString("none");
    if (!repeatKinds().contains(event.repeat))
        event.repeat = "none";
    return event;
}

QStringList Event::repeatKinds() {
    return {"none", "daily", "weekly", "monthly", "yearly"};
}

QString Event::newId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool Event::occursOn(const QDate &day) const {
    if (!date.isValid() || !day.isValid() || day < date)
        return false;
    if (repeat == "daily")
        return true;
    if (repeat == "weekly")
        return date.daysTo(day) % 7 == 0;
    // A monthly meeting on the 31st skips the months that have no 31st, and a
    // yearly one on 29 February skips the years that aren't leap years.
    if (repeat == "monthly")
        return day.day() == date.day();
    if (repeat == "yearly")
        return day.day() == date.day() && day.month() == date.month();
    return day == date;
}

QList<QDate> Event::occurrencesBetween(const QDate &from, const QDate &to) const {
    QList<QDate> days;
    if (repeat == "none") {
        if (date >= from && date <= to)
            days.append(date);
        return days;
    }
    for (QDate day = std::max(from, date); day <= to; day = day.addDays(1))
        if (occursOn(day))
            days.append(day);
    return days;
}

QDateTime Event::startAt(const QDate &occurrence) const {
    return QDateTime(occurrence, allDay ? QTime(9, 0) : start);
}

QDateTime Event::endAt(const QDate &occurrence) const {
    return allDay ? QDateTime(occurrence.addDays(1), QTime(0, 0)) : QDateTime(occurrence, end);
}

QTime parseTime(const QString &input) {
    const QString text = input.trimmed().toLower();
    static const QRegularExpression pattern(R"(^(\d{1,2})(?:\s*[:h.]\s*(\d{2})?)?$)");
    static const QRegularExpression compact(R"(^(\d{1,2})(\d{2})$)");
    QRegularExpressionMatch match = compact.match(text);
    if (!match.hasMatch())
        match = pattern.match(text);
    if (!match.hasMatch())
        return {};
    const int hours = match.captured(1).toInt();
    const int minutes = match.captured(2).isEmpty() ? 0 : match.captured(2).toInt();
    return QTime(hours, minutes);
}

static QString plain(const QString &text) {
    QString result;
    for (const QChar c : text.normalized(QString::NormalizationForm_D))
        if (c.category() != QChar::Mark_NonSpacing)
            result += c;
    return result.toLower();
}

QDate parseDate(const QString &input, const QDate &today) {
    const QString text = input.trimmed();
    if (text.isEmpty())
        return {};
    // "today" and "tomorrow" in English and the user's language, with or
    // without accents ("amanha").
    const QString word = plain(text);
    if (word == "today" || word == plain(QCoreApplication::translate("Agenda", "today")))
        return today;
    if (word == "tomorrow" || word == plain(QCoreApplication::translate("Agenda", "tomorrow")))
        return today.addDays(1);
    QDate date = QDate::fromString(text, Qt::ISODate);
    if (date.isValid())
        return date;
    date = QLocale().toDate(text, QLocale::ShortFormat);
    if (date.isValid())
        return date;

    // Day first, as most of the world writes it, unless the locale's own short
    // format puts the month first (en-US).
    const bool monthFirst = QLocale().dateFormat(QLocale::ShortFormat).indexOf('M')
                          < QLocale().dateFormat(QLocale::ShortFormat).indexOf('d');
    static const QRegularExpression pattern(R"(^(\d{1,2})\s*[/.\-]\s*(\d{1,2})(?:\s*[/.\-]\s*(\d{2}|\d{4}))?$)");
    const auto match = pattern.match(text);
    if (!match.hasMatch())
        return {};
    int day = match.captured(1).toInt(), month = match.captured(2).toInt();
    if (monthFirst)
        std::swap(day, month);
    int year = today.year();
    if (!match.captured(3).isEmpty()) {
        year = match.captured(3).toInt();
        if (year < 100)
            year += 2000;
    }
    return QDate(year, month, day);
}
