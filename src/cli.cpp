// The command line follows Hype's (MIT, David Heinemeier Hansson): every
// command takes --json, exits 0 or 1, and writes errors to stderr.
#include "cli.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QTextStream>
#include <algorithm>

#include "backend.h"
#include "format.h"
#include "reminders.h"
#include "store.h"

namespace {

QTextStream &out() {
    static QTextStream stream(stdout);
    return stream;
}

QTextStream &err() {
    static QTextStream stream(stderr);
    return stream;
}

const char *usage =
    "Usage: agenda [command] [options]\n"
    "\n"
    "Commands:\n"
    "  open                       Open the window (the default)\n"
    "  list [--from DATE] [--days N]\n"
    "                             Appointments from DATE (today) for N days (7)\n"
    "  add --title T --date DATE [--start HH:MM] [--end HH:MM] [--all-day]\n"
    "      [--remind 15,60] [--repeat none|daily|weekly|monthly|yearly]\n"
    "      [--location L] [--notes N]\n"
    "                             Add an appointment; reminders in minutes before\n"
    "  edit ID [same options as add]\n"
    "                             Change an appointment\n"
    "  remove ID                  Delete an appointment\n"
    "  remind                     Send the reminders that are due (run by\n"
    "                             agenda-reminders.timer every minute)\n"
    "\n"
    "Every command takes --json. Dates are YYYY-MM-DD or the locale's short date.\n"
    "The calendar lives in ~/.local/share/agenda/events.json.\n";

struct Options {
    QStringList positional;
    QHash<QString, QString> values;
    bool json = false;
    bool allDay = false;
    QString error;
};

Options parse(const QStringList &arguments) {
    static const QStringList valued{"--from", "--days", "--title", "--date", "--start", "--end", "--remind",
                                    "--repeat", "--location", "--notes"};
    Options options;
    for (int i = 0; i < arguments.size(); ++i) {
        QString argument = arguments[i];
        QString value;
        const int equals = argument.indexOf('=');
        if (argument.startsWith("--") && equals > 0) {
            value = argument.mid(equals + 1);
            argument = argument.left(equals);
            if (valued.contains(argument)) {
                options.values.insert(argument, value);
                continue;
            }
        }
        if (argument == "--json")
            options.json = true;
        else if (argument == "--all-day")
            options.allDay = true;
        else if (valued.contains(argument)) {
            if (i + 1 >= arguments.size()) {
                options.error = QString("%1 needs a value").arg(argument);
                return options;
            }
            options.values.insert(argument, arguments[++i]);
        } else if (argument.startsWith("--")) {
            options.error = QString("Unknown option %1").arg(argument);
            return options;
        } else
            options.positional.append(argument);
    }
    return options;
}

int fail(const Options &options, const QString &message) {
    if (options.json)
        out() << QJsonDocument(QJsonObject{{"ok", false}, {"error", message}}).toJson(QJsonDocument::Compact) << "\n";
    err() << "agenda: " << message << "\n";
    return 1;
}

QJsonObject occurrenceJson(const Event &event, const QDate &occurrence) {
    QJsonObject object = event.toJson();
    object.insert("occurrence", occurrence.toString(Qt::ISODate));
    object.insert("startsAt", event.startAt(occurrence).toString(Qt::ISODate));
    return object;
}

int list(const Options &options) {
    Store store;
    QString error;
    if (!store.load(&error))
        return fail(options, error);
    const QDate from = options.values.contains("--from") ? parseDate(options.values["--from"]) : QDate::currentDate();
    if (!from.isValid())
        return fail(options, QString("“%1” isn't a date").arg(options.values["--from"]));
    bool ok = true;
    const int days = options.values.value("--days", "7").toInt(&ok);
    if (!ok || days < 1)
        return fail(options, "--days takes a positive number");

    QList<QPair<QDateTime, QPair<Event, QDate>>> items;
    for (const Event &event : store.events)
        for (const QDate &day : event.occurrencesBetween(from, from.addDays(days - 1)))
            items.append({event.allDay ? QDateTime(day, QTime(0, 0)) : event.startAt(day), {event, day}});
    std::stable_sort(items.begin(), items.end(), [](const auto &a, const auto &b) { return a.first < b.first; });

    if (options.json) {
        QJsonArray array;
        for (const auto &item : items)
            array.append(occurrenceJson(item.second.first, item.second.second));
        out() << QJsonDocument(QJsonObject{{"ok", true}, {"events", array}}).toJson(QJsonDocument::Compact) << "\n";
        return 0;
    }
    const QLocale locale;
    QDate shown;
    for (const auto &item : items) {
        const Event &event = item.second.first;
        if (item.second.second != shown) {
            shown = item.second.second;
            out() << locale.toString(shown, QLocale::LongFormat) << "\n";
        }
        out() << "  " << Format::timeRange(event) << "  " << event.title;
        if (!event.location.isEmpty())
            out() << " · " << event.location;
        out() << "  [" << event.id.left(8) << "]\n";
    }
    if (items.isEmpty())
        out() << "No appointments.\n";
    return 0;
}

QString resolveId(const Store &store, const QString &prefix, QString *error) {
    QStringList matches;
    for (const Event &event : store.events)
        if (event.id.startsWith(prefix))
            matches.append(event.id);
    if (matches.size() == 1)
        return matches.first();
    *error = matches.isEmpty() ? QString("No appointment with id %1").arg(prefix)
                               : QString("Id %1 matches more than one appointment").arg(prefix);
    return {};
}

int addOrEdit(const Options &options, bool edit) {
    Backend backend;
    if (!backend.problem().isEmpty())
        return fail(options, backend.problem());

    QVariantMap fields;
    if (edit) {
        if (options.positional.size() != 2)
            return fail(options, "edit takes one appointment id");
        QString error;
        const QString id = resolveId(backend.store(), options.positional[1], &error);
        if (id.isEmpty())
            return fail(options, error);
        fields = backend.eventById(id);
    } else {
        fields = backend.draft();
        fields["date"] = QString();
        fields["start"] = QString();
        fields["end"] = QString();
    }

    auto take = [&](const char *option, const char *field) {
        if (options.values.contains(option))
            fields[field] = options.values[option];
    };
    take("--title", "title");
    take("--date", "date");
    take("--start", "start");
    take("--end", "end");
    take("--location", "location");
    take("--notes", "notes");
    take("--repeat", "repeat");
    if (options.values.contains("--repeat") && !Event::repeatKinds().contains(options.values["--repeat"]))
        return fail(options, QString("--repeat takes one of %1").arg(Event::repeatKinds().join(", ")));
    if (options.allDay)
        fields["allDay"] = true;
    else if (options.values.contains("--start"))
        fields["allDay"] = false;
    else if (!edit)
        fields["allDay"] = true;
    if (options.values.contains("--start") && !options.values.contains("--end") && !edit)
        fields["end"] = QString();
    if (options.values.contains("--remind")) {
        QVariantList minutes;
        const QString text = options.values["--remind"].trimmed();
        if (text != "none" && !text.isEmpty()) {
            for (const QString &part : text.split(',', Qt::SkipEmptyParts)) {
                bool ok = false;
                const int value = part.trimmed().toInt(&ok);
                if (!ok || value < 0)
                    return fail(options, "--remind takes minutes before the start, like 15,60, or none");
                minutes.append(value);
            }
        }
        fields["reminders"] = minutes;
    }
    if (!edit && fields.value("date").toString().isEmpty())
        return fail(options, "add needs --date");

    const QString error = backend.save(fields);
    if (!error.isEmpty())
        return fail(options, error);
    const Event event = backend.store().events[backend.store().indexOf(backend.lastSavedId())];
    if (options.json)
        out() << QJsonDocument(QJsonObject{{"ok", true}, {"event", event.toJson()}}).toJson(QJsonDocument::Compact) << "\n";
    else
        out() << (edit ? "Changed " : "Added ") << event.title << " on "
              << QLocale().toString(event.date, QLocale::LongFormat) << ", " << Format::timeRange(event) << " ["
              << event.id.left(8) << "]\n";
    return 0;
}

int remove(const Options &options) {
    if (options.positional.size() != 2)
        return fail(options, "remove takes one appointment id");
    Backend backend;
    if (!backend.problem().isEmpty())
        return fail(options, backend.problem());
    QString error;
    const QString id = resolveId(backend.store(), options.positional[1], &error);
    if (id.isEmpty())
        return fail(options, error);
    const QString title = backend.store().events[backend.store().indexOf(id)].title;
    backend.remove(id);
    if (!backend.problem().isEmpty())
        return fail(options, backend.problem());
    if (options.json)
        out() << QJsonDocument(QJsonObject{{"ok", true}, {"id", id}}).toJson(QJsonDocument::Compact) << "\n";
    else
        out() << "Removed " << title << "\n";
    return 0;
}

int remind(const Options &options) {
    Store store;
    QString error;
    if (!store.load(&error))
        return fail(options, error);
    SentLog log;
    const QString sentPath = Store::defaultSentPath();
    log.load(sentPath);
    const QDateTime now = QDateTime::currentDateTime();
    const QList<DueReminder> due = dueReminders(store.events, now, log);

    QJsonArray sent;
    QStringList failures;
    for (const DueReminder &reminder : due) {
        QString why;
        if (notify(reminder, now, &why))
            sent.append(QJsonObject{{"id", reminder.event.id}, {"title", reminder.event.title},
                                    {"occurrence", reminder.occurrence.toString(Qt::ISODate)},
                                    {"minutesBefore", reminder.minutes}});
        else
            failures.append(QString("%1: %2").arg(reminder.event.title, why));
    }
    log.prune(now);
    if (!log.save(sentPath))
        failures.append(QString("Can't save %1").arg(sentPath));
    if (!failures.isEmpty())
        return fail(options, failures.join("; "));
    if (options.json)
        out() << QJsonDocument(QJsonObject{{"ok", true}, {"sent", sent}}).toJson(QJsonDocument::Compact) << "\n";
    else
        for (const auto &value : sent)
            out() << "Reminded: " << value.toObject().value("title").toString() << "\n";
    return 0;
}

}

bool isGuiCommand(const QStringList &arguments) {
    return arguments.size() <= 1 || arguments[1] == "open";
}

int runCli(const QStringList &arguments) {
    if (arguments.contains("--help") || arguments.contains("-h") || arguments.value(1) == "help") {
        out() << usage;
        return 0;
    }
    if (arguments.contains("--version")) {
        out() << "agenda " << QCoreApplication::applicationVersion() << "\n";
        return 0;
    }
    const Options options = parse(arguments.mid(1));
    if (!options.error.isEmpty())
        return fail(options, options.error);
    const QString command = options.positional.value(0);
    if (command == "list")
        return list(options);
    if (command == "add")
        return addOrEdit(options, false);
    if (command == "edit")
        return addOrEdit(options, true);
    if (command == "remove" || command == "rm")
        return remove(options);
    if (command == "remind")
        return remind(options);
    err() << usage;
    return 1;
}
