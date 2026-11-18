#pragma once

#include <QStringList>

// Commands that need no display. Returns the exit code, or -1 when the
// arguments ask for the window instead.
namespace Cli {
int run(const QStringList &arguments);
bool wantsWindow(const QStringList &arguments);
}
