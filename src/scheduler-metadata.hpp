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

#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace scxctl {

struct SchedulerInfo {
    bool found = false;
    QString title;
    QString tagline;
    QString summary;
    QStringList hardware;   // chip labels
    QStringList workloads;  // chip labels
};

struct ProfileInfo {
    bool found = false;
    QString description;  // resolves to byScheduler[activeScheduler] if present, else the generic description
};

class SchedulerMetadata {
 public:
    // Loads and parses ":/scheduler-metadata.json" from the Qt resource system.
    // Safe to call once; if the resource is missing or invalid, isValid() returns false
    // and all lookups return found=false (graceful fallback, never crashes).
    SchedulerMetadata();

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool hasScheduler(const QString& name) const;
    [[nodiscard]] SchedulerInfo scheduler(const QString& name) const;                              // found=false if not listed
    [[nodiscard]] ProfileInfo profile(const QString& name, const QString& activeScheduler) const;  // found=false if not listed

    [[nodiscard]] int schedulerCount() const;
    [[nodiscard]] int profileCount() const;

 private:
    struct ProfileEntry {
        QString description;
        QHash<QString, QString> byScheduler;
    };
    bool m_valid = false;
    QHash<QString, SchedulerInfo> m_schedulers;
    QHash<QString, ProfileEntry> m_profiles;
};

}  // namespace scxctl
