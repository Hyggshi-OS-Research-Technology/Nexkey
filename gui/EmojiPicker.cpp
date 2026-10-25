// SPDX-License-Identifier: MIT
// NexKey - Smart Emoji Picker.

#include "EmojiPicker.hpp"

#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

QStringList EmojiPicker::s_recentEmojis = {
    QStringLiteral("😊"), QStringLiteral("😃"), QStringLiteral("❤️"),
    QStringLiteral("👍"), QStringLiteral("🔥"), QStringLiteral("⭐"),
    QStringLiteral("✅"), QStringLiteral("🎉"), QStringLiteral("😭")
};

EmojiPicker::EmojiPicker(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("NexKey - Emoji Picker"));
    setFixedSize(420, 480);
    loadEmojiData();
    buildUi();
}

void EmojiPicker::loadEmojiData()
{
    m_allEmojis = {
        // Faces & Smiles
        {QStringLiteral("😊"), tr("Smiling Face"), QStringLiteral("smile cuoi hanh phuc vui"), QStringLiteral(":)")},
        {QStringLiteral("😃"), tr("Grinning Face with Big Eyes"), QStringLiteral("grin cuoi to tuoi vui ve"), QStringLiteral(":D")},
        {QStringLiteral("😂"), tr("Face with Tears of Joy"), QStringLiteral("joy khoc cuoi vui haha lol"), QStringLiteral(":joy:")},
        {QStringLiteral("🤣"), tr("Rolling on the Floor Laughing"), QStringLiteral("rofl cuoi bo lan haha"), QStringLiteral(":rofl:")},
        {QStringLiteral("😉"), tr("Winking Face"), QStringLiteral("wink nhay mat da long nheo"), QStringLiteral(";-)")},
        {QStringLiteral("😍"), tr("Heart Eyes"), QStringLiteral("love yeu thich me man"), QStringLiteral(":heart_eyes:")},
        {QStringLiteral("🥰"), tr("Smiling Face with Hearts"), QStringLiteral("love yeu thuong hanh phuc"), QStringLiteral(":loved:")},
        {QStringLiteral("😎"), tr("Smiling Face with Sunglasses"), QStringLiteral("cool ngau kinh ram sunglasses"), QStringLiteral(":cool:")},
        {QStringLiteral("🥳"), tr("Partying Face"), QStringLiteral("party tiec tung sinh nhat an mung"), QStringLiteral(":party:")},
        {QStringLiteral("🥺"), tr("Pleading Face"), QStringLiteral("pleading nan ni long lanh thuong"), QStringLiteral(":plead:")},
        {QStringLiteral("😭"), tr("Loudly Crying Face"), QStringLiteral("cry khoc buon sad hu hu"), QStringLiteral(":cry:")},
        {QStringLiteral("🤔"), tr("Thinking Face"), QStringLiteral("think suy nghi suy tu think"), QStringLiteral(":think:")},
        {QStringLiteral("😴"), tr("Sleeping Face"), QStringLiteral("sleep ngu say kho kho"), QStringLiteral(":sleep:")},
        {QStringLiteral("😇"), tr("Smiling Face with Halo"), QStringLiteral("angel thien than ngoan hien"), QStringLiteral(":angel:")},
        {QStringLiteral("😈"), tr("Smiling Face with Horns"), QStringLiteral("devil ac quy tinh nghich"), QStringLiteral(":devil:")},
        {QStringLiteral("🤐"), tr("Zipper-Mouth Face"), QStringLiteral("secret bi mat im lang ngam mieng"), QStringLiteral(":zipper:")},
        {QStringLiteral("😱"), tr("Face Screaming in Fear"), QStringLiteral("scream hoang hot so hai shock"), QStringLiteral(":shock:")},
        {QStringLiteral("😡"), tr("Pouting Face"), QStringLiteral("angry tuc gian gian du cau"), QStringLiteral(":angry:")},

        // Gestures & Body
        {QStringLiteral("👍"), tr("Thumbs Up"), QStringLiteral("like thich dong y ok good tuyet"), QStringLiteral("(y)")},
        {QStringLiteral("👎"), tr("Thumbs Down"), QStringLiteral("dislike khong thich che bad"), QStringLiteral("(n)")},
        {QStringLiteral("👌"), tr("OK Hand"), QStringLiteral("ok chuan duoc perfectly"), QStringLiteral(":ok:")},
        {QStringLiteral("✌️"), tr("Victory Hand"), QStringLiteral("peace victory chao chien thang"), QStringLiteral(":v:")},
        {QStringLiteral("👋"), tr("Waving Hand"), QStringLiteral("wave chao tam biet bye hello"), QStringLiteral(":wave:")},
        {QStringLiteral("👏"), tr("Clapping Hands"), QStringLiteral("clap vo tay chuc mung hoan ho"), QStringLiteral(":clap:")},
        {QStringLiteral("🙏"), tr("Folded Hands"), QStringLiteral("pray cam on xin loi pray thanks please"), QStringLiteral(":pray:")},
        {QStringLiteral("💪"), tr("Flexed Biceps"), QStringLiteral("muscle co bap khoe manh quyet tam strong"), QStringLiteral(":flex:")},
        {QStringLiteral("🤝"), tr("Handshake"), QStringLiteral("handshake bat tay hop tac dong y"), QStringLiteral(":handshake:")},
        {QStringLiteral("🙌"), tr("Raising Hands"), QStringLiteral("hooray tung ho an mung"), QStringLiteral(":celebrate:")},

        // Hearts & Symbols
        {QStringLiteral("❤️"), tr("Red Heart"), QStringLiteral("heart tim do tinh yeu love"), QStringLiteral("<3")},
        {QStringLiteral("💖"), tr("Sparkling Heart"), QStringLiteral("heart tim lap lanh sparkling love"), QStringLiteral(":sparkle_heart:")},
        {QStringLiteral("💔"), tr("Broken Heart"), QStringLiteral("broken tan vo that tinh buon"), QStringLiteral("</3")},
        {QStringLiteral("🔥"), tr("Fire"), QStringLiteral("fire lua hot chay chay no trending"), QStringLiteral(":fire:")},
        {QStringLiteral("⭐"), tr("Star"), QStringLiteral("star ngoi sao star danh gia"), QStringLiteral(":star:")},
        {QStringLiteral("✨"), tr("Sparkles"), QStringLiteral("sparkles lap lanh sang shiny magic"), QStringLiteral(":sparkles:")},
        {QStringLiteral("✅"), tr("Check Mark"), QStringLiteral("check tich dung xong done complete ok"), QStringLiteral(":check:")},
        {QStringLiteral("❌"), tr("Cross Mark"), QStringLiteral("cross sai bo qua cancel x"), QStringLiteral(":cross:")},
        {QStringLiteral("💯"), tr("Hundred Points"), QStringLiteral("100 perfect diem tuyet doi chuan"), QStringLiteral(":100:")},
        {QStringLiteral("⚡"), tr("High Voltage"), QStringLiteral("lightning sam set nhanh set"), QStringLiteral(":lightning:")},
        {QStringLiteral("🎉"), tr("Party Popper"), QStringLiteral("popper tiec chuc mung party confetti"), QStringLiteral(":tada:")},
        {QStringLiteral("🚀"), tr("Rocket"), QStringLiteral("rocket ten lua launch nhanh speed phi thuyen"), QStringLiteral(":rocket:")},
        {QStringLiteral("💡"), tr("Light Bulb"), QStringLiteral("idea y tuong sang kien sang tao"), QStringLiteral(":bulb:")},
        {QStringLiteral("☕"), tr("Hot Beverage"), QStringLiteral("coffee ca phe tea uong nong"), QStringLiteral(":coffee:")},
        {QStringLiteral("🇻🇳"), tr("Vietnam Flag"), QStringLiteral("vietnam co to quoc viet nam vn"), QStringLiteral(":flag_vn:")}
    };
}

void EmojiPicker::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Tìm kiếm emoji (fuzzy search)..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &EmojiPicker::onSearchChanged);
    mainLayout->addWidget(m_searchEdit);

    auto* recentHeader = new QLabel(tr("Gần đây (9 gần nhất):"), this);
    QFont headerFont = recentHeader->font();
    headerFont.setBold(true);
    headerFont.setPointSize(10);
    recentHeader->setFont(headerFont);
    mainLayout->addWidget(recentHeader);

    auto* recentContainer = new QWidget(this);
    m_recentLayout = new QHBoxLayout(recentContainer);
    m_recentLayout->setContentsMargins(0, 0, 0, 0);
    m_recentLayout->setSpacing(6);
    mainLayout->addWidget(recentContainer);

    renderRecentRow();

    auto* allHeader = new QLabel(tr("Tất cả Emoji:"), this);
    allHeader->setFont(headerFont);
    mainLayout->addWidget(allHeader);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    m_gridContainer = new QWidget(scrollArea);
    m_grid = new QGridLayout(m_gridContainer);
    m_grid->setSpacing(6);
    m_grid->setContentsMargins(0, 0, 0, 0);
    scrollArea->setWidget(m_gridContainer);

    m_noResultsLabel = new QLabel(tr("Không tìm thấy emoji phù hợp"), m_gridContainer);
    m_noResultsLabel->setAlignment(Qt::AlignCenter);
    m_noResultsLabel->setVisible(false);
    m_grid->addWidget(m_noResultsLabel, 0, 0, 1, 6);

    mainLayout->addWidget(scrollArea, 1);

    filterEmojis(QString{});
    m_searchEdit->setFocus();
}

void EmojiPicker::renderRecentRow()
{
    while (QLayoutItem* item = m_recentLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    const int maxRecent = 9;
    int count = 0;
    for (const auto& emoji : s_recentEmojis) {
        if (count >= maxRecent) break;
        auto* btn = new QPushButton(emoji, this);
        btn->setFixedSize(36, 36);
        QFont f = btn->font();
        f.setPixelSize(20);
        btn->setFont(f);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [this, emoji] { selectEmoji(emoji); });
        m_recentLayout->addWidget(btn);
        ++count;
    }
    m_recentLayout->addStretch();
}

void EmojiPicker::filterEmojis(const QString& query)
{
    // Clear grid
    while (QLayoutItem* item = m_grid->takeAt(0)) {
        if (item->widget() != m_noResultsLabel) {
            delete item->widget();
        }
        delete item;
    }

    const QString lowerQuery = query.trimmed().toLower();
    int row = 0;
    int col = 0;
    const int maxCols = 6;
    int matchCount = 0;

    for (const auto& item : m_allEmojis) {
        bool matches = lowerQuery.isEmpty();
        if (!matches) {
            matches = item.character.contains(lowerQuery) ||
                      item.name.toLower().contains(lowerQuery) ||
                      item.keywords.toLower().contains(lowerQuery) ||
                      item.shortcut.toLower().contains(lowerQuery);
            if (!matches) {
                // Fuzzy: check if all query chars appear in order in keywords or name
                int qi = 0;
                const QString combined = (item.name + " " + item.keywords + " " + item.shortcut).toLower();
                for (int ci = 0; ci < combined.length() && qi < lowerQuery.length(); ++ci) {
                    if (combined[ci] == lowerQuery[qi]) ++qi;
                }
                if (qi == lowerQuery.length()) {
                    matches = true;
                }
            }
        }

        if (matches) {
            auto* btn = new QPushButton(item.character, m_gridContainer);
            btn->setFixedSize(54, 46);
            QFont f = btn->font();
            f.setPixelSize(24);
            btn->setFont(f);
            btn->setToolTip(QStringLiteral("%1\n%2 (%3)").arg(item.name, item.shortcut, item.keywords));
            btn->setCursor(Qt::PointingHandCursor);

            const QString character = item.character;
            connect(btn, &QPushButton::clicked, this, [this, character] { selectEmoji(character); });

            m_grid->addWidget(btn, row, col);
            ++col;
            if (col >= maxCols) {
                col = 0;
                ++row;
            }
            ++matchCount;
        }
    }

    m_noResultsLabel->setVisible(matchCount == 0);
    if (matchCount == 0) {
        m_grid->addWidget(m_noResultsLabel, 0, 0, 1, maxCols);
    }
}

void EmojiPicker::onSearchChanged(const QString& query)
{
    filterEmojis(query);
}

void EmojiPicker::selectEmoji(const QString& emoji)
{
    s_recentEmojis.removeAll(emoji);
    s_recentEmojis.prepend(emoji);
    while (s_recentEmojis.size() > 9) {
        s_recentEmojis.removeLast();
    }
    renderRecentRow();

    QGuiApplication::clipboard()->setText(emoji);
    emit emojiSelected(emoji);
    accept();
}
