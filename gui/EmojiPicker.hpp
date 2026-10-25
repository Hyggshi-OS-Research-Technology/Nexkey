// SPDX-License-Identifier: MIT
// NexKey - Smart Emoji Picker with fuzzy matching and 9 recent emojis history.

#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>
#include <vector>

class QLineEdit;
class QGridLayout;
class QScrollArea;
class QHBoxLayout;
class QLabel;

struct EmojiItem {
    QString character;
    QString name;
    QString keywords;
    QString shortcut;
};

class EmojiPicker : public QDialog {
    Q_OBJECT

public:
    explicit EmojiPicker(QWidget* parent = nullptr);
    ~EmojiPicker() override = default;

signals:
    void emojiSelected(const QString& emoji);

private slots:
    void onSearchChanged(const QString& query);
    void selectEmoji(const QString& emoji);

private:
    void buildUi();
    void loadEmojiData();
    void renderRecentRow();
    void filterEmojis(const QString& query);

    QLineEdit* m_searchEdit{};
    QHBoxLayout* m_recentLayout{};
    QGridLayout* m_grid{};
    QWidget* m_gridContainer{};
    QLabel* m_noResultsLabel{};

    std::vector<EmojiItem> m_allEmojis;
    static QStringList s_recentEmojis;
};
