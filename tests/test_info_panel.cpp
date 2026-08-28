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

// Offscreen verification for the in-app scheduler info card (Chunk 2):
//  - tier 1 (always runs): builds the card standalone and drives the fill
//    API for every scheduler in the metadata x every profile, plus the
//    unlisted-scheduler fallback; checks content, chip counts, profile
//    overrides, and that kInfoCardFixedHeight covers the worst-case
//    required content height at the design width (no clipping);
//  - tier 2 (guarded by scx_loader availability): constructs the real
//    SchedExtWindow offscreen, verifies the card is placed between the
//    scheduler grid and the flex spacer, drives the real combo boxes
//    (exercising the reactive slots), and asserts zero clipping and stable
//    window size for every combination.
//
// Run offscreen: the test forces QT_QPA_PLATFORM=offscreen itself.

#include "schedext-window-internal.hpp"
#include "scheduler-metadata.hpp"

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QSize>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace {

int g_checks   = 0;
int g_failures = 0;

void check(bool condition, const QString& what) {
    ++g_checks;
    if (condition) {
        std::printf("PASS: %s\n", what.toUtf8().constData());
    } else {
        ++g_failures;
        std::printf("FAIL: %s\n", what.toUtf8().constData());
    }
}

auto read_json_names(const QString& key) -> QStringList {
    QFile file(":/scheduler-metadata.json");
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    QStringList names;
    const QJsonObject object = document.object().value(key).toObject();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        names << it.key();
    }
    return names;
}

/// Tier-2 guard: the real window constructor only succeeds when the
/// scx_loader configuration file is readable and the scx_loader executable
/// is on PATH (both are preconditions the constructor checks before it
/// would otherwise show a blocking message box).
auto scx_loader_available() -> bool {
    std::ifstream config("/etc/scx_loader.toml");
    if (!config.is_open()) {
        return false;
    }
    return !QStandardPaths::findExecutable("scx_loader").isEmpty();
}

}  // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    const scxctl::SchedulerMetadata metadata;
    check(metadata.isValid(), "metadata resource loads and parses (isValid)");
    if (!metadata.isValid()) {
        std::printf("RESULT: FAILED (%d of %d checks failed)\n", g_failures, g_checks);
        return EXIT_FAILURE;
    }

    const QStringList scheduler_names = read_json_names("schedulers");
    const QStringList profile_names   = read_json_names("profiles");
    check(!scheduler_names.isEmpty() && !profile_names.isEmpty(), "metadata JSON lists schedulers and profiles");

    using scxctl::impl::kInfoCardDesignWidth;
    using scxctl::impl::kInfoCardFixedHeight;

    // ------------------------------------------------------------------
    // Tier 1: standalone card, every listed scheduler x every profile,
    // plus the unlisted-scheduler fallback, at the card's design width.
    // ------------------------------------------------------------------
    int tier1_max_required = 0;
    {
        QFrame card;
        card.setObjectName("scheduler_info_card");
        auto panel = scxctl::impl::build_info_card(&card);
        card.resize(kInfoCardDesignWidth, 1000);
        card.show();  // effective visibility (isVisible) needs a shown top-level

        auto measure = [&]() {
            const int required = card.layout()->heightForWidth(kInfoCardDesignWidth);
            tier1_max_required = std::max(tier1_max_required, required);
        };

        for (const QString& name : scheduler_names) {
            const auto info = metadata.scheduler(name);
            check(info.found, QStringLiteral("tier1: metadata lists %1").arg(name));
            scxctl::impl::apply_scheduler_info(panel, info, true);
            check(!panel.title->text().isEmpty(), QStringLiteral("tier1: %1 title populated").arg(name));
            check(!panel.tagline->text().isEmpty(), QStringLiteral("tier1: %1 tagline populated").arg(name));
            check(!panel.summary->text().isEmpty(), QStringLiteral("tier1: %1 summary populated").arg(name));
            check(!panel.fallback->isVisible(), QStringLiteral("tier1: %1 fallback hidden").arg(name));
            check(panel.hw_row->findChildren<QLabel*>(QString{}, Qt::FindDirectChildrenOnly).size() == info.hardware.size(),
                  QStringLiteral("tier1: %1 hardware chip count matches metadata").arg(name));
            check(panel.wl_row->findChildren<QLabel*>(QString{}, Qt::FindDirectChildrenOnly).size() == info.workloads.size(),
                  QStringLiteral("tier1: %1 workload chip count matches metadata").arg(name));

            for (const QString& profile : profile_names) {
                const auto profile_info = metadata.profile(profile, name);
                scxctl::impl::apply_profile_info(panel, profile, profile_info, true);
                check(panel.profile_section->isVisible(),
                      QStringLiteral("tier1: %1/%2 profile section visible").arg(name, profile));
                check(!panel.profile_name->text().isEmpty(),
                      QStringLiteral("tier1: %1/%2 profile name visible").arg(name, profile));
                check(!panel.profile_desc->text().isEmpty(),
                      QStringLiteral("tier1: %1/%2 profile description visible").arg(name, profile));
                measure();
            }

            // Profile combo hidden for this scheduler -> section hidden.
            scxctl::impl::apply_profile_info(panel, QString{}, scxctl::ProfileInfo{}, false);
            check(!panel.profile_section->isVisible(),
                  QStringLiteral("tier1: %1 profile section hidden when combo hidden").arg(name));
            measure();
        }

        // Unlisted scheduler: fields cleared, chips hidden, fallback shown.
        scxctl::impl::apply_scheduler_info(panel, scxctl::SchedulerInfo{}, false);
        check(panel.fallback->isVisible(), "tier1: fallback shown for unlisted scheduler");
        check(panel.title->text().isEmpty() && !panel.title->isVisible(), "tier1: title cleared for unlisted scheduler");
        check(panel.tagline->text().isEmpty() && !panel.tagline->isVisible(), "tier1: tagline cleared for unlisted scheduler");
        check(panel.summary->text().isEmpty() && !panel.summary->isVisible(), "tier1: summary cleared for unlisted scheduler");
        check(panel.hw_row->findChildren<QLabel*>(QString{}, Qt::FindDirectChildrenOnly).isEmpty(),
              "tier1: hardware chips cleared for unlisted scheduler");
        check(panel.wl_row->findChildren<QLabel*>(QString{}, Qt::FindDirectChildrenOnly).isEmpty(),
              "tier1: workload chips cleared for unlisted scheduler");
        measure();

        // Profile description must resolve the per-scheduler override.
        const auto generic_profile = metadata.profile("Gaming", "totally_custom_scheduler");
        const auto specific_profile = metadata.profile("Gaming", "scx_lavd");
        check(generic_profile.found && specific_profile.found && specific_profile.description != generic_profile.description,
              "tier1: profile description resolves the per-scheduler override");
    }
    check(kInfoCardFixedHeight >= tier1_max_required,
          QStringLiteral("tier1: fixed band height %1px covers worst-case content %2px at design width %3px")
              .arg(kInfoCardFixedHeight)
              .arg(tier1_max_required)
              .arg(kInfoCardDesignWidth));
    std::printf("STATS: tier1 worst-case required height = %d px at design width %d px (fixed band = %d px)\n",
                tier1_max_required, kInfoCardDesignWidth, kInfoCardFixedHeight);

    // ------------------------------------------------------------------
    // Tier 2: real window (guarded by scx_loader availability on the host).
    // ------------------------------------------------------------------
    if (!scx_loader_available()) {
        std::printf("SKIP: tier-2 real-window check (scx_loader configuration or executable unavailable on this host)\n");
    } else {
        // Construct the real window through the exported pimpl class and
        // reach the native QMainWindow via nativeWidget(); this exercises the
        // actual constructor (metadata load, combo wiring, initial
        // on_sched_changed) without linking implementation symbols.
        scxctl::SchedExtWindow window;
        auto* main_window = qobject_cast<QMainWindow*>(window.nativeWidget());
        check(main_window != nullptr, "tier2: native window present");
        if (main_window == nullptr) {
            std::printf("SKIP: tier-2 real-window check (no native window)\n");
        } else {
            main_window->resize(887, 544);
            main_window->show();

            auto* card = main_window->findChild<QFrame*>("scheduler_info_card");
            check(card != nullptr, "tier2: scheduler_info_card exists in the window");
            check(card != nullptr && card->height() == kInfoCardFixedHeight, "tier2: card is the fixed band height");
            check(card != nullptr && card->width() >= kInfoCardDesignWidth, "tier2: card is at least the design width");
            check(main_window->size() == QSize(887, 544), "tier2: window opens at its default 887x544 size");

            auto* central_layout = qobject_cast<QVBoxLayout*>(main_window->centralWidget()->layout());
            auto* grid           = main_window->centralWidget()->findChild<QGridLayout*>("gridLayout");
            check(central_layout != nullptr && grid != nullptr, "tier2: central layout and scheduler grid present");
            if (central_layout != nullptr && grid != nullptr && card != nullptr) {
                int card_index = -1;
                int grid_index = -1;
                for (int i = 0; i < central_layout->count(); ++i) {
                    auto* item = central_layout->itemAt(i);
                    if (item == nullptr) {
                        continue;
                    }
                    if (item->widget() == static_cast<QWidget*>(card)) {
                        card_index = i;
                    }
                    if (item->layout() == static_cast<QLayout*>(grid)) {
                        grid_index = i;
                    }
                }
                check(card_index > 0 && grid_index == card_index - 1, "tier2: card is placed directly after the scheduler grid");
                auto* next_item = card_index + 1 < central_layout->count() ? central_layout->itemAt(card_index + 1) : nullptr;
                check(next_item != nullptr && next_item->spacerItem() != nullptr,
                      "tier2: the flex vertical spacer sits directly after the card");
            }

            auto* sched_combo   = main_window->findChild<QComboBox*>("schedext_combo_box");
            auto* profile_combo = main_window->findChild<QComboBox*>("schedext_profile_combo_box");
            check(sched_combo != nullptr && sched_combo->count() > 0, "tier2: scheduler combo populated");

            // The panel must already reflect the initial selection: the
            // constructor calls on_sched_changed() after wiring the combos.
            if (sched_combo != nullptr) {
                const QString current = sched_combo->currentText();
                auto* title           = main_window->findChild<QLabel*>("info_sched_title");
                auto* fallback        = main_window->findChild<QLabel*>("info_fallback");
                check(title != nullptr && fallback != nullptr, "tier2: card title and fallback labels present");
                if (title != nullptr && fallback != nullptr) {
                    if (metadata.hasScheduler(current)) {
                        check(!title->text().isEmpty() && !fallback->isVisible(),
                              QStringLiteral("tier2: initial panel shows listed scheduler %1").arg(current));
                    } else {
                        check(fallback->isVisible(),
                              QStringLiteral("tier2: initial panel shows fallback for unlisted scheduler %1").arg(current));
                    }
                }
            }

            // Drive the real combo boxes; the connected reactive slots update
            // the card. Measure the required content height after every step
            // and verify the window size never changes.
            int tier2_max_required = 0;
            bool size_stable       = true;
            if (sched_combo != nullptr && card != nullptr) {
                for (int i = 0; i < sched_combo->count(); ++i) {
                    sched_combo->setCurrentIndex(i);  // fires on_sched_changed
                    if (profile_combo != nullptr && profile_combo->isVisible()) {
                        for (int j = 0; j < profile_combo->count(); ++j) {
                            profile_combo->setCurrentIndex(j);  // fires on_sched_profile_changed
                        }
                    }
                    tier2_max_required = std::max(tier2_max_required, card->layout()->heightForWidth(card->width()));
                    if (main_window->size() != QSize(887, 544)) {
                        size_stable = false;
                    }
                }
            }
            check(card != nullptr && card->height() >= tier2_max_required,
                  QStringLiteral("tier2: zero clipping across all driven combinations (max required %1px, band %2px)")
                      .arg(tier2_max_required)
                      .arg(card != nullptr ? card->height() : 0));
            check(size_stable, "tier2: window size stable while the panel updates");
            check(central_layout != nullptr && central_layout->minimumSize().height() <= 544,
                  "tier2: content including the card fits the default 544px window height");

            std::printf("STATS: tier2 worst-case required height = %d px at card width %d px (fixed band = %d px)\n",
                        tier2_max_required, card != nullptr ? card->width() : 0, kInfoCardFixedHeight);
        }
    }

    if (g_failures > 0) {
        std::printf("RESULT: FAILED (%d of %d checks failed)\n", g_failures, g_checks);
        return EXIT_FAILURE;
    }

    std::printf("RESULT: PASS (%d checks)\n", g_checks);
    return EXIT_SUCCESS;
}
