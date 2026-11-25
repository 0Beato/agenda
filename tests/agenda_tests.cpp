#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "event.h"
#include "format.h"
#include "reminders.h"
#include "store.h"
#include "theme.h"
#include "translator.h"

class AgendaTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QLocale::setDefault(QLocale(QLocale::Portuguese, QLocale::Brazil)); }

    void parsesTimesPeopleType()
    {
        QCOMPARE(parseTime("14"), QTime(14, 0));
        QCOMPARE(parseTime("14:30"), QTime(14, 30));
        QCOMPARE(parseTime("1430"), QTime(14, 30));
        QCOMPARE(parseTime("9h"), QTime(9, 0));
        QCOMPARE(parseTime("9h15"), QTime(9, 15));
        QVERIFY(!parseTime("25:00").isValid());
        QVERIFY(!parseTime("soon").isValid());
    }

    void parsesDatesDayFirst()
    {
        const QDate today(2026, 11, 22);
        QCOMPARE(parseDate("2026-12-03", today), QDate(2026, 12, 3));
        QCOMPARE(parseDate("03/12/2026", today), QDate(2026, 12, 3));
        QCOMPARE(parseDate("3/12", today), QDate(2026, 12, 3));
        QCOMPARE(parseDate("3/12/27", today), QDate(2027, 12, 3));
        QCOMPARE(parseDate("tomorrow", today), QDate(2026, 11, 23));
        QVERIFY(!parseDate("31/02", today).isValid());
    }

    void repeatsFollowTheCalendar()
    {
        Event weekly;
        weekly.date = QDate(2026, 11, 24);
        weekly.repeat = "weekly";
        QVERIFY(weekly.occursOn(QDate(2026, 12, 1)));
        QVERIFY(!weekly.occursOn(QDate(2026, 12, 2)));
        QVERIFY(!weekly.occursOn(QDate(2026, 11, 17)));

        Event monthly;
        monthly.date = QDate(2026, 1, 31);
        monthly.repeat = "monthly";
        QVERIFY(monthly.occursOn(QDate(2026, 3, 31)));
        QCOMPARE(monthly.occurrencesBetween(QDate(2026, 2, 1), QDate(2026, 4, 30)).size(), 1);
    }

    void storeRoundTripsAtomically()
    {
        QTemporaryDir dir;
        Store store(dir.filePath("nested/events.json"));
        Event event;
        event.id = "a";
        event.title = "Reunião";
        event.date = QDate(2026, 11, 24);
        event.start = QTime(10, 0);
        event.end = QTime(11, 0);
        event.reminders = {15, 1440};
        event.repeat = "weekly";
        store.events.append(event);
        store.defaultReminders = {30};
        QVERIFY(store.save());

        Store loaded(store.path());
        QVERIFY(loaded.load());
        QCOMPARE(loaded.events.size(), 1);
        QCOMPARE(loaded.events[0].title, QString("Reunião"));
        QCOMPARE(loaded.events[0].reminders, QList<int>({15, 1440}));
        QCOMPARE(loaded.events[0].repeat, QString("weekly"));
        QCOMPARE(loaded.defaultReminders, QList<int>({30}));
    }

    void damagedFileIsReported()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("events.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{not json");
        file.close();
        Store store(file.fileName());
        QString error;
        QVERIFY(!store.load(&error));
        QVERIFY(!error.isEmpty());
    }

    void remindersFireOnceAtTheirTime()
    {
        Event event;
        event.id = "m";
        event.title = "Meeting";
        event.date = QDate(2026, 11, 24);
        event.start = QTime(10, 0);
        event.end = QTime(11, 0);
        event.reminders = {15, 1440};
        SentLog log;

        QVERIFY(dueReminders({event}, QDateTime(QDate(2026, 11, 23), QTime(9, 59)), log).isEmpty());
        auto due = dueReminders({event}, QDateTime(QDate(2026, 11, 23), QTime(10, 0)), log);
        QCOMPARE(due.size(), 1);
        QCOMPARE(due[0].minutes, 1440);
        QVERIFY(dueReminders({event}, QDateTime(QDate(2026, 11, 23), QTime(10, 1)), log).isEmpty());
        due = dueReminders({event}, QDateTime(QDate(2026, 11, 24), QTime(9, 45)), log);
        QCOMPARE(due.size(), 1);
        QCOMPARE(due[0].minutes, 15);
    }

    void missedRemindersArriveOnceUntilTheEnd()
    {
        Event event;
        event.id = "m";
        event.title = "Meeting";
        event.date = QDate(2026, 11, 24);
        event.start = QTime(10, 0);
        event.end = QTime(11, 0);
        event.reminders = {0, 15, 60};

        // Off all morning, back at 10:05: one notification, not three.
        SentLog log;
        auto due = dueReminders({event}, QDateTime(QDate(2026, 11, 24), QTime(10, 5)), log);
        QCOMPARE(due.size(), 1);
        QCOMPARE(due[0].minutes, 0);
        QVERIFY(dueReminders({event}, QDateTime(QDate(2026, 11, 24), QTime(10, 6)), log).isEmpty());

        // Back after it ended: nothing.
        SentLog late;
        QVERIFY(dueReminders({event}, QDateTime(QDate(2026, 11, 24), QTime(11, 0)), late).isEmpty());
    }

    void movingAnAppointmentRearmsItsReminders()
    {
        Event event;
        event.id = "m";
        event.title = "Meeting";
        event.date = QDate(2026, 11, 24);
        event.start = QTime(10, 0);
        event.end = QTime(11, 0);
        event.reminders = {15};
        SentLog log;
        QCOMPARE(dueReminders({event}, QDateTime(QDate(2026, 11, 24), QTime(9, 45)), log).size(), 1);
        event.start = QTime(10, 30);
        event.end = QTime(11, 30);
        QCOMPARE(dueReminders({event}, QDateTime(QDate(2026, 11, 24), QTime(10, 15)), log).size(), 1);
    }

    void allDayRemindersCountFromNine()
    {
        Event event;
        event.id = "b";
        event.title = "Birthday";
        event.date = QDate(2026, 11, 28);
        event.allDay = true;
        event.repeat = "yearly";
        event.reminders = {1440};
        SentLog log;
        QVERIFY(dueReminders({event}, QDateTime(QDate(2027, 11, 27), QTime(8, 59)), log).isEmpty());
        QCOMPARE(dueReminders({event}, QDateTime(QDate(2027, 11, 27), QTime(9, 0)), log).size(), 1);
    }

    void sentLogSurvivesRestarts()
    {
        QTemporaryDir dir;
        SentLog log;
        log.insert("k", QDateTime::currentDateTime());
        QVERIFY(log.save(dir.filePath("state/sent.json")));
        SentLog again;
        QVERIFY(again.load(dir.filePath("state/sent.json")));
        QVERIFY(again.contains("k"));
        again.prune(QDateTime::currentDateTime().addDays(60));
        QCOMPARE(again.size(), 0);
    }

    void backendSavesValidatesAndUndoes()
    {
        QTemporaryDir dir;
        Backend backend(dir.filePath("events.json"));
        QVERIFY(!backend.save({{"title", ""}, {"date", "2026-11-24"}, {"start", "10:00"}}).isEmpty());
        QVERIFY(!backend.save({{"title", "X"}, {"date", "2026-11-24"}, {"start", "11:00"}, {"end", "10:00"}}).isEmpty());
        QCOMPARE(backend.save({{"title", "Dentista"}, {"date", "24/11/2026"}, {"start", "14h"},
                               {"reminders", QVariantList{60, 15}}}), QString());
        QCOMPARE(backend.selectedDate(), QString("2026-11-24"));
        QCOMPARE(backend.dayEvents().size(), 1);
        const QVariantMap saved = backend.eventById(backend.lastSavedId());
        QCOMPARE(saved.value("end").toString(), QString("15:00"));
        QCOMPARE(saved.value("reminders").toList(), QVariantList({15, 60}));
        // The next new appointment starts with the same reminders.
        QCOMPARE(backend.draft().value("reminders").toList(), QVariantList({15, 60}));

        QVERIFY(backend.remove(backend.lastSavedId()));
        QCOMPARE(backend.dayEvents().size(), 0);
        backend.undo();
        QCOMPARE(backend.dayEvents().size(), 1);
        backend.redo();
        QCOMPARE(backend.dayEvents().size(), 0);

        Store onDisk(dir.filePath("events.json"));
        QVERIFY(onDisk.load());
        QCOMPARE(onDisk.events.size(), 0);
    }

    void backendPicksUpOutsideChanges()
    {
        QTemporaryDir dir;
        Backend backend(dir.filePath("events.json"));
        backend.select("2026-11-24");
        Store other(dir.filePath("events.json"));
        Event event;
        event.id = "x";
        event.title = "From the CLI";
        event.date = QDate(2026, 11, 24);
        event.allDay = true;
        other.events.append(event);
        QVERIFY(other.save());
        QTRY_COMPARE(backend.dayEvents().size(), 1);
    }

    void monthGridHasSixWeeks()
    {
        QTemporaryDir dir;
        Backend backend(dir.filePath("events.json"));
        backend.select("2026-11-22");
        const QVariantList cells = backend.monthCells();
        QCOMPARE(cells.size(), 42);
        // Brazil starts the week on Sunday, and 1 November 2026 is a Sunday.
        QCOMPARE(cells.first().toMap().value("date").toString(), QString("2026-11-01"));
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");
        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void shippedCatalogsTranslateReminders()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        Translator translator(dir.path() + "/language", QString(SRCDIR) + "/../src/i18n");
        QCoreApplication::installTranslator(&translator);
        QCOMPARE(Format::reminder(15), QString("15 minutos antes"));
        QCOMPARE(Format::reminder(1440), QString("1 dia antes"));
        QCOMPARE(Format::reminder(0), QString("Na hora"));
        QCoreApplication::removeTranslator(&translator);

        // English carries its own plural forms.
        write(dir.path() + "/language", "en\n");
        Translator english(dir.path() + "/language", QString(SRCDIR) + "/../src/i18n");
        QCoreApplication::installTranslator(&english);
        QCOMPARE(Format::reminder(60), QString("1 hour before"));
        QCOMPARE(Format::reminder(120), QString("2 hours before"));
        QCoreApplication::removeTranslator(&english);
        QLocale::setDefault(QLocale(QLocale::Portuguese, QLocale::Brazil));
    }

    void catalogsAreValidJson()
    {
        for (const QString &name : QDir(QString(SRCDIR) + "/../src/i18n").entryList({"*.json"})) {
            QFile file(QString(SRCDIR) + "/../src/i18n/" + name);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QJsonParseError error;
            QJsonDocument::fromJson(file.readAll(), &error);
            QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(name));
        }
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(AgendaTests)
#include "agenda_tests.moc"
