#pragma once

#include <QStringList>

// Commands that need no display, for scripts, and the reminder timer.
bool isGuiCommand(const QStringList &arguments);
int runCli(const QStringList &arguments);
