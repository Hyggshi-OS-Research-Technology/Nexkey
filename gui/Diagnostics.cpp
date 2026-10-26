// SPDX-License-Identifier: MIT
// NexKey - environment and runtime diagnostics for the settings dialog.

#include "Diagnostics.hpp"

#include "QtHelpers.hpp"
#include "nexkey/Settings.hpp"
#include "nexkey/Status.hpp"
#include "nexkey/Types.hpp"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>

namespace nexkey::gui {

namespace {

QString env_or_unset(const char* name)
{
    const QString value = qEnvironmentVariable(name);
    return value.isEmpty() ? QStringLiteral("(chưa đặt)") : value;
}

QString age_text(qint64 seconds)
{
    if (seconds < 0) {
        seconds = 0;
    }
    if (seconds < 60) {
        return QStringLiteral("%1 giây trước").arg(seconds);
    }
    if (seconds < 3600) {
        return QStringLiteral("%1 phút trước").arg(seconds / 60);
    }
    return QStringLiteral("%1 giờ trước").arg(seconds / 3600);
}

QString file_line(const QFileInfo& info)
{
    return QStringLiteral("%1  (sửa lần cuối %2)")
        .arg(info.absoluteFilePath(), info.lastModified().toString(QStringLiteral("dd/MM/yyyy HH:mm")));
}

/// Every existing copy of the Fcitx5 addon library in the usual locations.
QList<QFileInfo> find_addon_copies()
{
    const QString home = QDir::homePath();
    const QStringList directories = {
        QStringLiteral("/usr/local/lib/fcitx5"),
        QStringLiteral("/usr/local/lib64/fcitx5"),
        QStringLiteral("/usr/local/lib/x86_64-linux-gnu/fcitx5"),
        QStringLiteral("/usr/lib/fcitx5"),
        QStringLiteral("/usr/lib64/fcitx5"),
        QStringLiteral("/usr/lib/x86_64-linux-gnu/fcitx5"),
        QStringLiteral("/usr/lib/aarch64-linux-gnu/fcitx5"),
        home + QStringLiteral("/.local/lib/fcitx5"),
        home + QStringLiteral("/.local/lib64/fcitx5"),
        home + QStringLiteral("/.local/lib/x86_64-linux-gnu/fcitx5"),
    };

    QList<QFileInfo> found;
    QSet<QString> seen;
    for (const auto& directory : directories) {
        const QFileInfo info(QDir(directory).filePath(QStringLiteral("nexkey.so")));
        if (!info.exists()) {
            continue;
        }
        const QString canonical = info.canonicalFilePath();
        if (seen.contains(canonical)) {
            continue;
        }
        seen.insert(canonical);
        found.append(info);
    }
    return found;
}

/// Every distinct nexkey-gui executable on PATH, in PATH order.
QList<QFileInfo> find_gui_binaries()
{
    QList<QFileInfo> found;
    QSet<QString> seen;
    const auto directories = qEnvironmentVariable("PATH").split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const auto& directory : directories) {
        const QFileInfo info(QDir(directory).filePath(QStringLiteral("nexkey-gui")));
        if (!info.exists() || !info.isExecutable()) {
            continue;
        }
        const QString canonical = info.canonicalFilePath();
        if (seen.contains(canonical)) {
            continue;
        }
        seen.insert(canonical);
        found.append(info);
    }
    return found;
}

} // namespace

DiagnosticsReport collect_diagnostics()
{
    DiagnosticsReport report;
    QStringList lines;
    const auto section = [&lines](const QString& title) {
        if (!lines.isEmpty()) {
            lines << QString{};
        }
        lines << QStringLiteral("== %1 ==").arg(title);
    };

    const QString version = to_qstring(kVersion);
    const QString gui_build = to_qstring(build_id());

    // --- NexKey itself --------------------------------------------------
    section(QStringLiteral("NexKey"));
    lines << QStringLiteral("Phiên bản: %1 (build %2)").arg(version, gui_build);
    lines << QStringLiteral("GUI đang chạy: %1").arg(QCoreApplication::applicationFilePath());
    lines << QStringLiteral("File cấu hình: %1").arg(QString::fromStdString(default_config_path().string()));

    const auto settings = load_settings();
    lines << QStringLiteral("Gõ trực tiếp: %1")
                 .arg(settings.options.direct_typing ? QStringLiteral("bật") : QStringLiteral("tắt"));
    if (settings.app_rules.empty()) {
        lines << QStringLiteral("Quy tắc ứng dụng: (không có)");
    } else {
        lines << QStringLiteral("Quy tắc ứng dụng:");
        for (const auto& [app, mode] : settings.app_rules) {
            lines << QStringLiteral("  %1 → %2").arg(QString::fromStdString(app), QString::fromStdString(mode));
        }
    }

    // --- Session ---------------------------------------------------------
    section(QStringLiteral("Phiên làm việc"));
    const QString session = qEnvironmentVariable("XDG_SESSION_TYPE");
    lines << QStringLiteral("Kiểu phiên: %1").arg(session.isEmpty() ? QStringLiteral("(không rõ)") : session);
    lines << QStringLiteral("Desktop: %1").arg(env_or_unset("XDG_CURRENT_DESKTOP"));
    lines << QStringLiteral("GTK_IM_MODULE=%1").arg(env_or_unset("GTK_IM_MODULE"));
    lines << QStringLiteral("QT_IM_MODULE=%1").arg(env_or_unset("QT_IM_MODULE"));
    lines << QStringLiteral("XMODIFIERS=%1").arg(env_or_unset("XMODIFIERS"));
    if (session == QLatin1String("x11") &&
        !qEnvironmentVariable("XMODIFIERS").contains(QLatin1String("fcitx"), Qt::CaseInsensitive)) {
        report.issues << QStringLiteral(
            "Phiên X11 nhưng XMODIFIERS không trỏ tới fcitx (cần @im=fcitx) — một số ứng dụng sẽ không nhận bộ gõ.");
    }

    // --- Fcitx5 ----------------------------------------------------------
    section(QStringLiteral("Fcitx5"));
    bool fcitx_running = false;
    if (auto* bus = QDBusConnection::sessionBus().interface(); bus != nullptr) {
        fcitx_running = bus->isServiceRegistered(QStringLiteral("org.fcitx.Fcitx5"));
    }
    lines << QStringLiteral("Trạng thái: %1")
                 .arg(fcitx_running ? QStringLiteral("đang chạy") : QStringLiteral("KHÔNG chạy"));
    if (fcitx_running) {
        QDBusInterface controller(QStringLiteral("org.fcitx.Fcitx5"), QStringLiteral("/controller"),
                                  QStringLiteral("org.fcitx.Fcitx.Controller1"));
        controller.setTimeout(500);
        const QDBusReply<QString> current = controller.call(QStringLiteral("CurrentInputMethod"));
        if (current.isValid()) {
            lines << QStringLiteral("Bộ gõ đang dùng: %1").arg(current.value());
        }
    } else {
        report.issues << QStringLiteral("Fcitx5 không chạy — chạy `fcitx5 -d` hoặc đăng nhập lại.");
    }

    // --- Installed copies -----------------------------------------------
    section(QStringLiteral("Addon nexkey.so"));
    const auto addons = find_addon_copies();
    if (addons.isEmpty()) {
        lines << QStringLiteral("(không tìm thấy)");
        report.issues << QStringLiteral("Không tìm thấy nexkey.so — chưa cài addon fcitx5.");
    }
    for (const auto& info : addons) {
        lines << file_line(info);
    }
    if (addons.size() > 1) {
        report.issues << QStringLiteral(
            "Có %1 bản nexkey.so — fcitx5 có thể nạp bản cũ thay vì bản mới. Xóa các bản thừa rồi chạy `fcitx5 -r -d`.")
                             .arg(addons.size());
    }

    section(QStringLiteral("nexkey-gui trong PATH"));
    const auto binaries = find_gui_binaries();
    if (binaries.isEmpty()) {
        lines << QStringLiteral("(không tìm thấy)");
    }
    for (const auto& info : binaries) {
        lines << file_line(info);
    }
    if (binaries.size() > 1) {
        report.issues << QStringLiteral(
            "Có %1 bản nexkey-gui trong PATH — lệnh `nexkey-gui` chạy bản đầu tiên, có thể là bản cũ.")
                             .arg(binaries.size());
    }

    // --- What the addon reports -----------------------------------------
    section(QStringLiteral("Ứng dụng gõ gần nhất"));
    RuntimeStatus status;
    if (!read_status(status)) {
        lines << QStringLiteral("Chưa có dữ liệu. Gõ thử vài chữ trong một ứng dụng khác rồi bấm Làm mới.");
        report.issues << QStringLiteral(
            "Addon chưa báo trạng thái — addon chưa nạp, hoặc đang là bản cũ chưa có tính năng này.");
    } else {
        const qint64 age = QDateTime::currentSecsSinceEpoch() - status.updated;
        lines << QStringLiteral("Ứng dụng: %1").arg(QString::fromStdString(status.program));
        lines << QStringLiteral("Chế độ: %1").arg(QString::fromStdString(status.mode));
        lines << QStringLiteral("Lý do: %1").arg(QString::fromStdString(status.reason));
        lines << QStringLiteral("Hỗ trợ surrounding text: %1")
                     .arg(status.surrounding_capability
                              ? (status.surrounding_valid ? QStringLiteral("có, dữ liệu hợp lệ")
                                                          : QStringLiteral("khai báo nhưng chưa có dữ liệu"))
                              : QStringLiteral("không"));
        lines << QStringLiteral("Lệch gần đây (60 giây): %1").arg(status.recent_mismatches);
        lines << QStringLiteral("Cập nhật: %1").arg(age_text(age));
        lines << QStringLiteral("Addon: %1 (build %2)")
                     .arg(QString::fromStdString(status.addon_version),
                          QString::fromStdString(status.addon_build));
        if (!status.last_learned.empty()) {
            lines << QStringLiteral("Tự chuyển sang preedit gần nhất: %1 (%2)")
                         .arg(QString::fromStdString(status.last_learned),
                              age_text(QDateTime::currentSecsSinceEpoch() - status.last_learned_at));
        }

        if (status.addon_build != std::string(build_id())) {
            report.issues << QStringLiteral(
                "Addon đang chạy khác bản build với GUI — fcitx5 còn nạp addon cũ. Chạy `fcitx5 -r -d`.");
        }
        if (status.mode == "direct" && !status.surrounding_capability) {
            report.issues << QStringLiteral(
                "%1 không hỗ trợ surrounding text nhưng đang gõ trực tiếp — xóa/thay chữ có thể lỗi. "
                "Thêm quy tắc \"preedit\" cho ứng dụng này.")
                                 .arg(QString::fromStdString(status.program));
        }
        if (status.recent_mismatches > 0) {
            report.issues << QStringLiteral(
                "%1: văn bản thực tế lệch với những gì NexKey đã gõ %2 lần gần đây.")
                                 .arg(QString::fromStdString(status.program))
                                 .arg(status.recent_mismatches);
        }
    }

    report.text = lines.join(QLatin1Char('\n'));
    if (!report.issues.isEmpty()) {
        report.text += QStringLiteral("\n\n== Vấn đề phát hiện ==\n");
        for (const auto& issue : report.issues) {
            report.text += QStringLiteral("- %1\n").arg(issue);
        }
    }
    return report;
}

} // namespace nexkey::gui
