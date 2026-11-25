#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

#include "event.h"

// Words for times and reminders, shared by the window and the notifications
// so both say the same thing in the user's language.
namespace Format {
QString reminder(int minutes);
QString reminderShort(int minutes);
QString reminders(const QList<int> &minutes);
QString timeRange(const Event &event);
// Compact names the day as "Tue, 24 Nov" instead of in full.
QString when(const Event &event, const QDate &occurrence, const QDateTime &now, bool compact = false);
QString repeat(const QString &kind);
QList<int> reminderChoices();
}
