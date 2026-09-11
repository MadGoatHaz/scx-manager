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

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// The project builds with hidden symbol visibility (see
// cmake/StandardProjectSettings.cmake) so that shared libraries only export
// intended API symbols. Every out-of-line definition of SchedulerMetadata is
// explicitly marked for export so that consumers outside the library (the
// unit test, and any future out-of-process users) can link against it.
#if defined(__GNUC__) || defined(__clang__)
#define SCXMETADATA_EXPORT __attribute__((visibility("default")))
#else
#define SCXMETADATA_EXPORT
#endif

namespace scxctl {

// The read-only getters below are candidates for the 'pure' attribute, which
// this project's warning set would flag as a suggestion. They are plain
// accessors over immutable-after-construction state, so the suggestion is
// suppressed locally.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=pure"
#endif

SCXMETADATA_EXPORT SchedulerMetadata::SchedulerMetadata() {
    QFile file(QStringLiteral(":/scheduler-metadata.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    const QByteArray data = file.readAll();

    QJsonParseError parse_error{};
    const QJsonDocument document = QJsonDocument::fromJson(data, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        return;
    }

    const QJsonObject root = document.object();

    const QJsonObject schedulers = root.value(QStringLiteral("schedulers")).toObject();
    for (const QString& name : schedulers.keys()) {
        const QJsonObject entry = schedulers.value(name).toObject();

        SchedulerInfo info;
        info.found   = true;
        info.title   = entry.value(QStringLiteral("title")).toString();
        info.tagline = entry.value(QStringLiteral("tagline")).toString();
        info.summary = entry.value(QStringLiteral("summary")).toString();

        const QJsonArray hardware = entry.value(QStringLiteral("hardware")).toArray();
        for (const auto& value : hardware) {
            info.hardware.append(value.toString());
        }

        const QJsonArray workloads = entry.value(QStringLiteral("workloads")).toArray();
        for (const auto& value : workloads) {
            info.workloads.append(value.toString());
        }

        m_schedulers.insert(name, info);
    }

    const QJsonObject profiles = root.value(QStringLiteral("profiles")).toObject();
    for (const QString& name : profiles.keys()) {
        const QJsonObject entry = profiles.value(name).toObject();

        ProfileEntry profile;
        profile.description = entry.value(QStringLiteral("description")).toString();

        const QJsonObject by_scheduler = entry.value(QStringLiteral("byScheduler")).toObject();
        for (const QString& scheduler : by_scheduler.keys()) {
            profile.byScheduler.insert(scheduler, by_scheduler.value(scheduler).toString());
        }

        m_profiles.insert(name, profile);
    }

    m_valid = true;
}

SCXMETADATA_EXPORT bool SchedulerMetadata::isValid() const {
    return m_valid;
}

SCXMETADATA_EXPORT bool SchedulerMetadata::hasScheduler(const QString& name) const {
    return m_schedulers.contains(name);
}

SCXMETADATA_EXPORT SchedulerInfo SchedulerMetadata::scheduler(const QString& name) const {
    return m_schedulers.value(name);
}

SCXMETADATA_EXPORT ProfileInfo SchedulerMetadata::profile(const QString& name, const QString& activeScheduler) const {
    ProfileInfo info;

    const auto it = m_profiles.constFind(name);
    if (it == m_profiles.constEnd()) {
        return info;
    }

    info.found             = true;
    const auto override_it = it->byScheduler.constFind(activeScheduler);
    if (override_it != it->byScheduler.constEnd()) {
        info.description = override_it.value();
    } else {
        info.description = it->description;
    }

    return info;
}

SCXMETADATA_EXPORT int SchedulerMetadata::schedulerCount() const {
    return static_cast<int>(m_schedulers.size());
}

SCXMETADATA_EXPORT int SchedulerMetadata::profileCount() const {
    return static_cast<int>(m_profiles.size());
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

}  // namespace scxctl
