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

// NOLINTBEGIN(bugprone-unhandled-exception-at-new)

#include "schedext-window-internal.hpp"
#include "schedext-window.hpp"
#include "scx_utils.hpp"

#include <algorithm>    // for any_of
#include <array>        // for array
#include <fstream>      // for ifstream
#include <ranges>       // for ranges::*
#include <string>       // for string
#include <string_view>  // for string_view

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsuggest-final-types"
#pragma GCC diagnostic ignored "-Wsuggest-attribute=pure"
#pragma GCC diagnostic ignored "-Wconversion"
#endif

#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <fmt/compile.h>
#include <fmt/core.h>

namespace {
auto read_kernel_file(std::string_view file_path) noexcept -> std::string {
    std::string file_content{};

    // Skip if failed to open file descriptor.
    std::ifstream file_stream{std::string{file_path}};
    if (!file_stream.is_open()) {
        return {};
    }
    if (!std::getline(file_stream, file_content)) {
        fmt::print(stderr, "Failed to read := '{}'\n", file_path);
        return {};
    }

    return file_content;
}

auto get_current_scheduler() noexcept -> std::string {
    using namespace std::string_view_literals;

    // NOTE: we assume that window won't be launched on kernel without sched_ext
    // e.g we won't show window at all in that case.
    const auto& current_state = read_kernel_file("/sys/kernel/sched_ext/state"sv);
    if (current_state != "enabled"sv) {
        return current_state;
    }
    const auto& current_sched = read_kernel_file("/sys/kernel/sched_ext/root/ops"sv);
    if (current_sched.empty()) {
        return std::string{"unknown"sv};
    }
    return current_sched;
}

constexpr auto get_scx_mode_from_str(std::string_view scx_mode) noexcept -> scx::SchedMode {
    using namespace std::string_view_literals;

    if (scx_mode == "Gaming"sv) {
        return scx::SchedMode::Gaming;
    } else if (scx_mode == "Lowlatency"sv) {
        return scx::SchedMode::LowLatency;
    } else if (scx_mode == "Powersave"sv) {
        return scx::SchedMode::PowerSave;
    } else if (scx_mode == "Server"sv) {
        return scx::SchedMode::Server;
    }
    return scx::SchedMode::Auto;
}

}  // namespace

// The project builds with hidden symbol visibility (see
// cmake/StandardProjectSettings.cmake) so that shared libraries only export
// intended API symbols. The card build / fill helpers below are used by the
// offscreen test (tests/test_info_panel.cpp) outside the library, so every
// out-of-line definition is explicitly marked for export, mirroring
// scheduler-metadata.cpp.
#if defined(__GNUC__) || defined(__clang__)
#define SCXINFO_EXPORT __attribute__((visibility("default")))
#else
#define SCXINFO_EXPORT
#endif

namespace {

/// Minimal left-to-right wrapping layout used for the hardware / workload
/// chip rows. Chips that do not fit on one line wrap to the next;
/// heightForWidth() reports the wrapped height so the outer layout (and the
/// no-clipping measurement) accounts for wrapping. Item ownership stays with
/// QLayout; the vector below is re-created on every chip rebuild, so it can
/// never hold a dangling item pointer.
class FlowLayout final : public QLayout {
 public:
    explicit FlowLayout(int h_spacing = 6, int v_spacing = 6)
      : QLayout(), m_h_spacing(h_spacing), m_v_spacing(v_spacing) {}

    ~FlowLayout() override = default;

    void addItem(QLayoutItem* item) override { m_items.push_back(item); }

    int count() const override { return static_cast<int>(m_items.size()); }

    // Marked pure: const, no I/O, no allocation, only reads the (const) item
    // list via `this`, so it has no observable side effects. The bounds check
    // also guarantees m_items[] is only reached in-range (so even libstdc++'s
    // _GLIBCXX_ASSERTIONS abort path is unreachable here). This suppresses
    // -Wsuggest-attribute=pure in Release builds compiled with makepkg
    // CXXFLAGS (-Wp,-D_GLIBCXX_ASSERTIONS).
    QLayoutItem* itemAt(int index) const override Q_DECL_PURE_FUNCTION {
        if (index < 0 || index >= static_cast<int>(m_items.size())) {
            return nullptr;
        }
        return m_items[static_cast<std::size_t>(index)];
    }

    QLayoutItem* takeAt(int index) override {
        if (index < 0 || index >= static_cast<int>(m_items.size())) {
            return nullptr;
        }
        QLayoutItem* item = m_items[static_cast<std::size_t>(index)];
        m_items.erase(m_items.begin() + static_cast<std::size_t>(index));
        return item;
    }

    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return compute_height(width); }

    QSize minimumSize() const override {
        QSize size;
        for (const QLayoutItem* item : m_items) {
            size = size.expandedTo(item->minimumSize());
        }
        return size;
    }

    QSize sizeHint() const override { return minimumSize(); }

    void setGeometry(const QRect& rect) override { do_layout(rect, true); }

 private:
    auto compute_height(int width) const -> int {
        if (m_items.empty()) {
            return 0;
        }
        int y      = 0;
        int row_x  = 0;
        int row_h  = 0;
        int height = 0;
        for (const QLayoutItem* item : m_items) {
            const int item_w = item->sizeHint().width();
            if (row_x > 0 && row_x + item_w > width) {
                y += row_h + m_v_spacing;
                row_x = 0;
                row_h = 0;
            }
            row_x += item_w + m_h_spacing;
            row_h = std::max(row_h, item->sizeHint().height());
        }
        height = y + row_h;
        return height;
    }

    void do_layout(const QRect& rect, bool save) {
        int left  = 0;
        int top   = 0;
        int x     = left;
        int y     = top;
        int row_h = 0;
        for (QLayoutItem* item : m_items) {
            const int item_w = item->sizeHint().width();
            if (x > left && x + item_w > rect.right()) {
                y += row_h + m_v_spacing;
                x = left;
                row_h = 0;
            }
            if (save) {
                item->setGeometry(QRect(QPoint(x, y), item->sizeHint()));
            }
            x += item_w + m_h_spacing;
            row_h = std::max(row_h, item->sizeHint().height());
        }
    }

    std::vector<QLayoutItem*> m_items;
    int m_h_spacing = 0;
    int m_v_spacing = 0;
};

auto make_card_font(const QFont& base, bool bold, bool italic, double delta_point) -> QFont {
    QFont font(base);
    font.setBold(bold);
    font.setItalic(italic);
    // Skip the delta for pixel-sized fonts (pointSizeF() == -1) so we never
    // end up with a nonsensical point size.
    if (font.pointSizeF() > 0.0) {
        font.setPointSizeF(font.pointSizeF() + delta_point);
    }
    return font;
}

/// Replaces the chip row's contents with one pill per entry in `labels`.
/// The previous flow layout is destroyed first (QLayout frees its items),
/// then any leftover chip widgets, so the new layout starts clean.
auto rebuild_chip_row(QWidget* row, const QStringList& labels) -> void {
    if (row->layout() != nullptr) {
        delete row->layout();
    }
    const auto old_chips = row->findChildren<QLabel*>(QString{}, Qt::FindDirectChildrenOnly);
    for (QLabel* chip : old_chips) {
        delete chip;
    }
    auto* flow = new FlowLayout();
    row->setLayout(flow);
    for (const QString& label : labels) {
        auto* chip = new QLabel(label, row);
        chip->setObjectName("info_chip");
        chip->setFont(make_card_font(chip->font(), false, false, -1.0f));
        flow->addWidget(chip);
    }
}

}  // namespace

namespace scxctl::impl {

SCXINFO_EXPORT auto build_info_card(QFrame* card) -> InfoPanel {
    InfoPanel panel;
    if (card == nullptr) {
        return panel;
    }

    auto* body = new QVBoxLayout(card);
    body->setContentsMargins(12, 10, 12, 10);
    body->setSpacing(6);

    // 1. Scheduler title: bold, +2pt.
    panel.title = new QLabel(card);
    panel.title->setObjectName("info_sched_title");
    panel.title->setFont(make_card_font(card->font(), true, false, 2.0f));
    panel.title->setVisible(false);
    body->addWidget(panel.title);

    // 2. Tagline: muted, italic.
    panel.tagline = new QLabel(card);
    panel.tagline->setObjectName("info_sched_tagline");
    panel.tagline->setFont(make_card_font(card->font(), false, true, 0.0f));
    panel.tagline->setVisible(false);
    body->addWidget(panel.tagline);

    // 3. Technical summary: word-wrapped body text.
    panel.summary = new QLabel(card);
    panel.summary->setObjectName("info_sched_summary");
    panel.summary->setWordWrap(true);
    panel.summary->setVisible(false);
    body->addWidget(panel.summary);

    // 4. Hardware chips row (wrapping).
    panel.hw_row = new QWidget(card);
    panel.hw_row->setObjectName("info_hw_row");
    panel.hw_row->setLayout(new FlowLayout());
    panel.hw_row->setVisible(false);
    body->addWidget(panel.hw_row);

    // 5. Workload chips row (wrapping).
    panel.wl_row = new QWidget(card);
    panel.wl_row->setObjectName("info_wl_row");
    panel.wl_row->setLayout(new FlowLayout());
    panel.wl_row->setVisible(false);
    body->addWidget(panel.wl_row);

    // 6. Thin divider between scheduler info and profile info.
    panel.divider = new QWidget(card);
    panel.divider->setObjectName("info_divider");
    panel.divider->setFixedHeight(1);
    panel.divider->setVisible(false);
    body->addWidget(panel.divider);

    // 7. Active profile section: "Active Profile" header + bold name,
    //    then the (word-wrapped) description.
    panel.profile_section = new QWidget(card);
    panel.profile_section->setObjectName("info_profile_section");
    auto* profile_body = new QVBoxLayout(panel.profile_section);
    profile_body->setContentsMargins(0, 0, 0, 0);
    profile_body->setSpacing(4);

    auto* header_row   = new QWidget(panel.profile_section);
    auto* header_layout = new QHBoxLayout(header_row);
    header_layout->setContentsMargins(0, 0, 0, 0);
    header_layout->setSpacing(6);
    auto* header = new QLabel(QObject::tr("Active Profile"), header_row);
    header->setObjectName("info_profile_header");
    header->setFont(make_card_font(card->font(), false, false, -1.0f));
    header_layout->addWidget(header);
    panel.profile_name = new QLabel(header_row);
    panel.profile_name->setObjectName("info_profile_name");
    panel.profile_name->setFont(make_card_font(card->font(), true, false, 0.0f));
    header_layout->addWidget(panel.profile_name);
    header_layout->addStretch(1);
    profile_body->addWidget(header_row);

    panel.profile_desc = new QLabel(panel.profile_section);
    panel.profile_desc->setObjectName("info_profile_desc");
    panel.profile_desc->setWordWrap(true);
    profile_body->addWidget(panel.profile_desc);
    panel.profile_section->setVisible(false);
    body->addWidget(panel.profile_section);

    // 8. Fallback notice for schedulers without metadata (hidden by default).
    panel.fallback = new QLabel(QObject::tr("Custom scheduler detected — detailed metadata is unavailable for this scheduler."), card);
    panel.fallback->setObjectName("info_fallback");
    panel.fallback->setWordWrap(true);
    panel.fallback->setFont(make_card_font(card->font(), false, true, 0.0f));
    panel.fallback->setVisible(false);
    body->addWidget(panel.fallback);

    // Self-contained dark stylesheet, scoped to this card so the rest of the
    // window keeps whatever theme the platform provides.
    card->setStyleSheet(QStringLiteral(R"qss(
        #scheduler_info_card {
            background-color: #232433;
            border: 1px solid #3b3d52;
            border-radius: 8px;
        }
        #info_sched_title { color: #f2f3f8; }
        #info_sched_tagline { color: #a6a9c0; }
        #info_sched_summary { color: #c8cbdd; }
        #info_chip {
            background-color: #2f3149;
            border: 1px solid #4a4d70;
            border-radius: 8px;
            padding: 3px 8px;
            color: #c3c6e0;
        }
        #info_divider { background-color: #3b3d52; border: none; }
        #info_profile_header { color: #8a8da8; }
        #info_profile_name { color: #e8e9f2; }
        #info_profile_desc { color: #c8cbdd; }
        #info_fallback { color: #9a9db5; }
    )qss"));

    return panel;
}

SCXINFO_EXPORT void apply_scheduler_info(InfoPanel& panel, const scxctl::SchedulerInfo& info, bool found) {
    if (!panel.ready()) {
        return;
    }
    if (found) {
        panel.title->setText(info.title);
        panel.tagline->setText(info.tagline);
        panel.summary->setText(info.summary);
        panel.title->setVisible(true);
        panel.tagline->setVisible(true);
        panel.summary->setVisible(true);
        rebuild_chip_row(panel.hw_row, info.hardware);
        panel.hw_row->setVisible(!info.hardware.isEmpty());
        rebuild_chip_row(panel.wl_row, info.workloads);
        panel.wl_row->setVisible(!info.workloads.isEmpty());
        panel.fallback->setVisible(false);
    } else {
        panel.title->clear();
        panel.tagline->clear();
        panel.summary->clear();
        panel.title->setVisible(false);
        panel.tagline->setVisible(false);
        panel.summary->setVisible(false);
        rebuild_chip_row(panel.hw_row, QStringList{});
        rebuild_chip_row(panel.wl_row, QStringList{});
        panel.hw_row->setVisible(false);
        panel.wl_row->setVisible(false);
        panel.fallback->setVisible(true);
    }
}

SCXINFO_EXPORT void apply_profile_info(InfoPanel& panel, const QString& profile_name, const scxctl::ProfileInfo& info, bool visible) {
    if (!panel.ready()) {
        return;
    }
    if (!visible || !info.found || info.description.isEmpty()) {
        panel.profile_section->setVisible(false);
        panel.divider->setVisible(false);
        return;
    }
    panel.profile_name->setText(profile_name);
    panel.profile_desc->setText(info.description);
    panel.profile_section->setVisible(true);
    panel.divider->setVisible(true);
}

SchedExtWindow::SchedExtWindow(QWidget* parent)
  : QMainWindow(parent), m_sched_timer(new QTimer(this)) {
    m_ui->setupUi(this);

    // Build the fixed-height scheduler info band (between the flags row and
    // the bottom buttons) before any early return below, so the card always
    // exists; it stays in its neutral state when scx_loader is unavailable.
    m_info_panel = build_info_card(m_ui->scheduler_info_card);
    m_ui->scheduler_info_card->setFixedHeight(kInfoCardFixedHeight);

    setAttribute(Qt::WA_NativeWindow);
    setWindowFlags(Qt::Window);  // for the close, min and max buttons

    {
        auto loader_config = scx::loader::Config::init_config(m_config_path);
        if (loader_config.has_value()) {
            m_scx_config = std::make_unique<scx::loader::Config>(std::move(*loader_config));
        } else {
            QMessageBox::critical(this, "SCX Scheduler Manager", tr("Cannot initialize scx_loader configuration"));
            return;
        }
    }

    // Timer updates information about currently running scheduler even without scx_loader,
    // as it reads information reported by scx scheduler.
    using namespace std::chrono_literals;  // NOLINT
    connect(m_sched_timer, &QTimer::timeout, this, &SchedExtWindow::update_current_sched);
    m_sched_timer->start(1s);

    // Selecting the scheduler
    auto supported_scheds = scx::loader::get_supported_scheds();
    if (supported_scheds.has_value()) {
        m_ui->schedext_combo_box->addItems(*supported_scheds);
    } else {
        QMessageBox::critical(this, "SCX Scheduler Manager", tr("Cannot get information from scx_loader!\nIs it working?\nThis is needed for the app to work properly"));

        // hide all components which depends on scheduler management
        m_ui->schedext_combo_box->setHidden(true);
        m_ui->scheduler_select_label->setHidden(true);

        m_ui->schedext_profile_combo_box->setHidden(true);
        m_ui->scheduler_profile_select_label->setHidden(true);

        m_ui->schedext_flags_edit->setHidden(true);
        m_ui->scheduler_set_flags_label->setHidden(true);
        return;
    }

    // Set currently running scheduler
    auto current_sched = m_scx_config->get_current_sched();
    if (current_sched.has_value()) {
        m_ui->schedext_combo_box->setCurrentText(QString::fromStdString(*current_sched));
    }

    // Selecting the performance profile
    QStringList sched_profiles;
    sched_profiles << "Auto"
                   << "Gaming"
                   << "Powersave"
                   << "Lowlatency"
                   << "Server";
    m_ui->schedext_profile_combo_box->addItems(sched_profiles);
    connect(m_ui->schedext_profile_combo_box,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        &SchedExtWindow::on_sched_profile_changed);

    // Set currently running scheduler mode
    auto current_mode = m_scx_config->get_current_mode();
    if (current_mode.has_value()) {
        // NOTE: the index of profiles and scxmode values MUST match
        m_ui->schedext_profile_combo_box->setCurrentIndex(static_cast<std::uint8_t>(*current_mode));
    }

    m_ui->current_sched_label->setText(QString::fromStdString(get_current_scheduler()));

    connect(m_ui->schedext_combo_box,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        &SchedExtWindow::on_sched_changed);
    // Initialize the visibility of the profile selection box
    on_sched_changed();

    // Connect buttons signal
    connect(m_ui->apply_button, &QPushButton::clicked, this, &SchedExtWindow::on_apply);
    connect(m_ui->disable_button, &QPushButton::clicked, this, &SchedExtWindow::on_disable);
    connect(m_ui->cancel_button, &QPushButton::clicked, this, &SchedExtWindow::close);
}

void SchedExtWindow::closeEvent(QCloseEvent* event) {
    QWidget::closeEvent(event);
}

void SchedExtWindow::update_current_sched() noexcept {
    m_ui->current_sched_label->setText(QString::fromStdString(get_current_scheduler()));
}

void SchedExtWindow::on_disable() noexcept {
    m_ui->disable_button->setEnabled(false);
    m_ui->apply_button->setEnabled(false);

    if (!m_scx_config->disable_scheduler(m_config_path)) {
        QMessageBox::critical(this, "SCX Scheduler Manager", tr("Cannot disable scx_loader"));
    }

    m_ui->disable_button->setEnabled(true);
    m_ui->apply_button->setEnabled(true);
}

void SchedExtWindow::on_sched_profile_changed() noexcept {
    const auto& current_selected = m_ui->schedext_combo_box->currentText().toStdString();
    const auto& current_profile  = m_ui->schedext_profile_combo_box->currentText().toStdString();
    const auto& scx_mode         = get_scx_mode_from_str(current_profile);

    auto sched_args = QStringList();
    if (auto scx_flags_for_mode = m_scx_config->scx_flags_for_mode(current_selected, scx_mode); scx_flags_for_mode) {
        if (!scx_flags_for_mode->empty()) {
            sched_args << std::move(*scx_flags_for_mode);
        }
    } else {
        QMessageBox::critical(this, "SCX Scheduler Manager", tr("Cannot get scx flags from scx_loader configuration!"));
    }

    m_ui->schedext_flags_edit->setText(sched_args.join(' '));
    update_profile_section();
}

void SchedExtWindow::on_sched_changed() noexcept {
    const auto& scheduler = m_ui->schedext_combo_box->currentText();

    // Show or hide the profile selection UI based on the selected scheduler
    //
    // NOTE: only some support different preset profiles at
    // the moment.
    using namespace std::string_view_literals;
    constexpr std::array supported_scheds{"scx_bpfland"sv, "scx_lavd"sv, "scx_p2dq"sv, "scx_tickless"sv, "scx_cosmos"sv, "scx_cake"sv};
    if (std::ranges::any_of(supported_scheds, [&](auto&& supported_sched) { return supported_sched == scheduler; })) {
        m_ui->scheduler_profile_select_label->setVisible(true);
        m_ui->schedext_profile_combo_box->setVisible(true);
    } else {
        m_ui->scheduler_profile_select_label->setVisible(false);
        m_ui->schedext_profile_combo_box->setVisible(false);
    }
    on_sched_profile_changed();
    update_info_panel();
}

void SchedExtWindow::update_info_panel() noexcept {
    const auto& name = m_ui->schedext_combo_box->currentText();
    const auto& info = m_metadata.scheduler(name);
    apply_scheduler_info(m_info_panel, info, m_metadata.isValid() && info.found);
    update_profile_section();
}

void SchedExtWindow::update_profile_section() noexcept {
    const bool combo_visible = m_ui->schedext_profile_combo_box->isVisible();
    const auto& profile_name = m_ui->schedext_profile_combo_box->currentText();
    const auto& info = m_metadata.profile(profile_name, m_ui->schedext_combo_box->currentText());
    apply_profile_info(m_info_panel, profile_name, info,
        combo_visible && m_metadata.isValid() && info.found && !info.description.isEmpty());
}

void SchedExtWindow::on_apply() noexcept {
    m_ui->disable_button->setEnabled(false);
    m_ui->apply_button->setEnabled(false);

    // TODO(vnepogodin): refactor that
    const auto& current_selected = m_ui->schedext_combo_box->currentText().toStdString();
    const auto& current_profile  = m_ui->schedext_profile_combo_box->currentText().toStdString();
    const auto& extra_flags      = m_ui->schedext_flags_edit->text().trimmed().toStdString();
    const auto& scx_mode         = get_scx_mode_from_str(current_profile);

    if (!m_scx_config->apply_scheduler_change(current_selected, scx_mode, extra_flags, m_config_path)) {
        QMessageBox::critical(this, "SCX Scheduler Manager", tr("Cannot set default scx scheduler with mode! Scheduler %1 with mode %2").arg(QString::fromStdString(current_selected), QString::fromStdString(current_profile)));
    }

    m_ui->disable_button->setEnabled(true);
    m_ui->apply_button->setEnabled(true);
}

}  // namespace scxctl::impl

// NOLINTEND(bugprone-unhandled-exception-at-new)
