#pragma once

#include <QDate>
#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include "store.h"

// The calendar the window shows: which day is picked, what happens on it,
// and the edits, each one saved straight away and undoable.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString selectedDate READ selectedDate WRITE select NOTIFY changed)
    Q_PROPERTY(QString monthTitle READ monthTitle NOTIFY changed)
    Q_PROPERTY(QString dayTitle READ dayTitle NOTIFY changed)
    Q_PROPERTY(QVariantList weekdays READ weekdays NOTIFY changed)
    Q_PROPERTY(QVariantList monthCells READ monthCells NOTIFY changed)
    Q_PROPERTY(QVariantList dayEvents READ dayEvents NOTIFY changed)
    Q_PROPERTY(QVariantList upcoming READ upcoming NOTIFY changed)
    Q_PROPERTY(QVariantList reminderChoices READ reminderChoices NOTIFY changed)
    Q_PROPERTY(QVariantList repeatChoices READ repeatChoices NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY problemChanged)

public:
    explicit Backend(const QString &path = Store::defaultPath(), QObject *parent = nullptr);

    QString selectedDate() const { return m_selected.toString(Qt::ISODate); }
    QString monthTitle() const;
    QString dayTitle() const;
    QVariantList weekdays() const;
    QVariantList monthCells() const;
    QVariantList dayEvents() const;
    QVariantList upcoming() const;
    QVariantList reminderChoices() const;
    QVariantList repeatChoices() const;
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    QString problem() const { return m_problem; }

    Q_INVOKABLE void select(const QString &isoDate);
    Q_INVOKABLE void moveDays(int days);
    Q_INVOKABLE void moveMonths(int months);
    Q_INVOKABLE void goToday();

    // A new appointment on the picked day, an hour from the next full hour
    // when that day is today, with the reminders last used.
    Q_INVOKABLE QVariantMap draft() const;
    Q_INVOKABLE QVariantMap eventById(const QString &id) const;
    // Returns an empty string on success, or what's wrong with the input.
    Q_INVOKABLE QString save(const QVariantMap &fields);
    Q_INVOKABLE bool remove(const QString &id);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    Q_INVOKABLE QString formatDate(const QString &isoDate) const;
    Q_INVOKABLE QString reminderLabel(int minutes) const;

    // Turns on the minutely reminder check (a systemd user timer the package
    // installs) and reports in the window when it can't.
    Q_INVOKABLE void ensureReminders();

    Store &store() { return m_store; }
    QString lastSavedId() const { return m_lastSavedId; }

public slots:
    void refresh();

signals:
    void changed();
    void problemChanged();

private:
    QVariantMap occurrenceMap(const Event &event, const QDate &occurrence) const;
    void commit(const QList<Event> &before);
    void setProblem(const QString &problem);
    void watch();

    Store m_store;
    QDate m_selected = QDate::currentDate();
    QDate m_today = QDate::currentDate();
    QList<QList<Event>> m_undo, m_redo;
    QString m_problem, m_loadProblem, m_reminderProblem, m_lastSavedId;
    QFileSystemWatcher m_watcher;
    QTimer m_reload, m_clock;
};
