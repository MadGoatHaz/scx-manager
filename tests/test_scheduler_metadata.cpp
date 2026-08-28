// Copyright (C) 2026 SCX Scheduler Manager contributors
//
// This file is part of SCX Scheduler Manager.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include "scheduler-metadata.hpp"

#include <cstdio>
#include <cstdlib>

#include <QString>
#include <QStringList>

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const QString &what) {
    ++g_checks;
    if (condition) {
        std::printf("PASS: %s\n", what.toUtf8().constData());
    } else {
        ++g_failures;
        std::printf("FAIL: %s\n", what.toUtf8().constData());
    }
}

}  // namespace

int main() {
    const scxctl::SchedulerMetadata metadata;

    // 1. Resource present and parses.
    check(metadata.isValid(), "metadata resource loads and parses (isValid)");

    // 2. Full scheduler set and the six UI-gating names.
    check(metadata.schedulerCount() >= 13, "schedulerCount() >= 13");
    const QStringList gating = {"scx_bpfland",
                               "scx_lavd",
                               "scx_p2dq",
                               "scx_tickless",
                               "scx_cosmos",
                               "scx_cake"};
    for (const QString &name : gating) {
        check(metadata.hasScheduler(name), QStringLiteral("hasScheduler(%1)").arg(name));
    }

    // 3. Sample scheduler fields are populated.
    const scxctl::SchedulerInfo lavd = metadata.scheduler("scx_lavd");
    check(lavd.found, "scheduler(scx_lavd).found");
    check(!lavd.title.isEmpty(), "scx_lavd title is non-empty");
    check(!lavd.tagline.isEmpty(), "scx_lavd tagline is non-empty");
    check(!lavd.summary.isEmpty(), "scx_lavd summary is non-empty");
    check(!lavd.hardware.isEmpty(), "scx_lavd hardware is non-empty");
    check(!lavd.workloads.isEmpty(), "scx_lavd workloads is non-empty");

    // 4. Unknown scheduler falls back gracefully.
    check(!metadata.hasScheduler("totally_custom_scheduler"), "unknown scheduler is not listed");
    check(!metadata.scheduler("totally_custom_scheduler").found, "unknown scheduler().found == false");

    // 5. Profiles: the five UI keys exist, resolve for an active scheduler, and
    //    byScheduler overrides take precedence over the generic description.
    check(metadata.profileCount() >= 5, "profileCount() >= 5");
    const QStringList profiles = {"Auto", "Gaming", "Powersave", "Lowlatency", "Server"};
    for (const QString &name : profiles) {
        const scxctl::ProfileInfo info = metadata.profile(name, "scx_lavd");
        check(info.found, QStringLiteral("profile(%1, scx_lavd).found").arg(name));
        check(!info.description.isEmpty(), QStringLiteral("profile(%1, scx_lavd) description non-empty").arg(name));
    }

    const scxctl::ProfileInfo generic = metadata.profile("Gaming", "totally_custom_scheduler");
    const scxctl::ProfileInfo specific = metadata.profile("Gaming", "scx_lavd");
    check(generic.found && !generic.description.isEmpty(), "profile(Gaming) generic fallback available");
    check(specific.found && !specific.description.isEmpty() && specific.description != generic.description,
          "profile(Gaming, scx_lavd) resolves its byScheduler override");

    if (g_failures > 0) {
        std::printf("RESULT: FAILED (%d of %d checks failed)\n", g_failures, g_checks);
        return EXIT_FAILURE;
    }

    std::printf("RESULT: PASS (%d checks)\n", g_checks);
    return EXIT_SUCCESS;
}
