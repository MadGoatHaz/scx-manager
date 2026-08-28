// Copyright (C) 2022-2025 Vladislav Nepogodin
//
// This file is part of CachyOS kernel manager.
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

#ifndef SCHEDEXT_WINDOW_INTERNAL_HPP_
#define SCHEDEXT_WINDOW_INTERNAL_HPP_

#include "schedext-window.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wfloat-conversion"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wsuggest-final-methods"
#pragma GCC diagnostic ignored "-Wsuggest-attribute=pure"
#endif

#include <ui_schedext-window.h>

#include "scheduler-metadata.hpp"
#include "scx_utils.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <QFrame>
#include <QLabel>
#include <QMainWindow>
#include <QTimer>
#include <QWidget>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace scxctl::impl {

/// Smallest width (px) the scheduler_info_card can reach: the main window
/// opens 887px wide and its vertical layout margins are at most 11px per
/// side (style dependent), so the card is never narrower than this. All
/// fixed-band height measurements are taken at this width, which keeps the
/// no-clipping guarantee valid for every window size the user can reach.
inline constexpr int kInfoCardDesignWidth = 865;

/// Fixed height (px) of the scheduler_info_card band. Determined
/// empirically (see tests/test_info_panel.cpp): the maximum required content
/// height over every scheduler x profile combination at
/// kInfoCardDesignWidth measured offscreen was 247px; the band is
/// 263px = 247 + 16px padding, which also keeps extra headroom for platform
/// font-metric differences (the shown-state measurement was 230px). The band
/// never resizes, so dynamic content can neither clip nor jitter the window.
inline constexpr int kInfoCardFixedHeight = 263;

/// Live handles into the widgets built inside scheduler_info_card.
/// Widgets stay parented to the card; the panel only carries pointers.
struct InfoPanel {
    QLabel* title = nullptr;             // info_sched_title
    QLabel* tagline = nullptr;           // info_sched_tagline
    QLabel* summary = nullptr;           // info_sched_summary
    QWidget* hw_row = nullptr;           // info_hw_row (wrapping chip container)
    QWidget* wl_row = nullptr;           // info_wl_row (wrapping chip container)
    QWidget* divider = nullptr;          // info_divider
    QWidget* profile_section = nullptr;  // info_profile_section
    QLabel* profile_name = nullptr;      // info_profile_name
    QLabel* profile_desc = nullptr;      // info_profile_desc
    QLabel* fallback = nullptr;          // info_fallback

    bool ready() const noexcept { return title != nullptr; }
};

/// Builds the card's internal content (title, tagline, summary, hardware /
/// workload chip rows, divider, profile section, fallback notice) inside
/// `card`, applies the self-contained dark stylesheet, and returns handles
/// to the widgets. All content starts hidden/empty (neutral state).
auto build_info_card(QFrame* card) -> InfoPanel;

/// Populates (or clears) the scheduler half of the panel from one
/// SchedulerMetadata lookup. With `found == false` the fields are cleared,
/// the chip rows hidden, and the fallback notice shown instead.
void apply_scheduler_info(InfoPanel& panel, const scxctl::SchedulerInfo& info, bool found);

/// Populates (or hides) the profile half of the panel. With `visible ==
/// false` (profile combo hidden for this scheduler) or a missing/empty
/// description the section and divider are hidden gracefully.
void apply_profile_info(InfoPanel& panel, const QString& profile_name, const scxctl::ProfileInfo& info, bool visible);

class SchedExtWindow final : public QMainWindow {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SchedExtWindow)
 public:
    explicit SchedExtWindow(QWidget* parent = nullptr);
    ~SchedExtWindow() = default;

 protected:
    void closeEvent(QCloseEvent* event) override;

 private:
    void on_apply() noexcept;
    void on_disable() noexcept;
    void on_sched_changed() noexcept;
    void on_sched_profile_changed() noexcept;

    /// Rebuilds the card's scheduler half (title/tagline/summary/chips or the
    /// fallback notice) from the currently selected scheduler, then re-syncs
    /// the profile half.
    void update_info_panel() noexcept;
    /// Rebuilds the card's profile half from the currently selected profile,
    /// mirroring the profile combo's per-scheduler visibility.
    void update_profile_section() noexcept;
    void update_current_sched() noexcept;

    const std::string_view m_config_path{"/etc/scx_loader.toml"};
    scx::loader::ConfigPtr m_scx_config;
    std::vector<std::string> m_previously_set_options{};
    std::unique_ptr<Ui::SchedExtWindow> m_ui = std::make_unique<Ui::SchedExtWindow>();
    QTimer* m_sched_timer                    = nullptr;

    scxctl::SchedulerMetadata m_metadata;  // loads ":/scheduler-metadata.json" once
    InfoPanel m_info_panel;                // filled by build_info_card() in the constructor
};

}  // namespace scxctl::impl

#endif  // SCHEDEXT_WINDOW_INTERNAL_HPP_
