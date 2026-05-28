#include "ui/SettingsDialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QFontComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QLabel>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QApplication>
#include <QSettings>
#include <QFrame>
#include <QTableWidget>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMessageBox>
#include <QKeySequence>

// ── Key capture dialog ────────────────────────────────────────────────────────
class KeyCaptureDialog : public QDialog {
public:
    int capturedKey = 0;

    explicit KeyCaptureDialog(QWidget* parent) : QDialog(parent) {
        setWindowTitle("Capture Key");
        setFixedSize(220, 80);
        auto* lbl = new QLabel("Press any key\n(Esc to cancel)", this);
        lbl->setAlignment(Qt::AlignCenter);
        auto* lay = new QVBoxLayout(this);
        lay->addWidget(lbl);
    }

protected:
    void keyPressEvent(QKeyEvent* e) override {
        int k = e->key();
        if (k == Qt::Key_unknown) return;
        if (k == Qt::Key_Escape)  { reject(); return; }
        capturedKey = k;
        accept();
    }
};

static QString keyName(int k) {
    if (k == 0) return {};
    return QKeySequence(k).toString();
}

// ── SettingsDialog ────────────────────────────────────────────────────────────
SettingsDialog::SettingsDialog(AppState& state, QWidget* parent)
    : QDialog(parent)
    , m_appState(state)
    , m_originalFont(qApp->font())
    , m_originalWheelSize(state.wheelSizePercent.load())
    , m_editedBindings(state.keyBindings)
    , m_originalBindings(state.keyBindings)
{
    setWindowTitle("Preferences");
    setMinimumSize(560, 420);

    m_sidebar = new QListWidget(this);
    m_sidebar->setFixedWidth(120);
    m_sidebar->setFrameShape(QFrame::NoFrame);
    m_sidebar->setStyleSheet(
        "QListWidget { background: #2a2a2a; border-right: 1px solid #444; }"
        "QListWidget::item { padding: 10px 12px; color: #ccc; }"
        "QListWidget::item:selected { background: #3a3a3a; color: white; "
        "  border-left: 3px solid #00c850; }");
    m_sidebar->addItem("UI");
    m_sidebar->addItem("Controls");
    m_sidebar->setCurrentRow(0);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildUiPage());
    m_stack->addWidget(buildControlsPage());

    connect(m_sidebar, &QListWidget::currentRowChanged,
            m_stack,   &QStackedWidget::setCurrentIndex);

    auto* buttons   = new QDialogButtonBox(this);
    auto* okBtn     = buttons->addButton(QDialogButtonBox::Ok);
    auto* cancelBtn = buttons->addButton(QDialogButtonBox::Cancel);
    auto* applyBtn  = buttons->addButton(QDialogButtonBox::Apply);

    connect(okBtn,     &QPushButton::clicked, this, &SettingsDialog::onOk);
    connect(cancelBtn, &QPushButton::clicked, this, &SettingsDialog::onCancel);
    connect(applyBtn,  &QPushButton::clicked, this, &SettingsDialog::onApply);

    auto* body = new QHBoxLayout;
    body->setSpacing(0);
    body->setContentsMargins(0, 0, 0, 0);
    body->addWidget(m_sidebar);
    body->addWidget(m_stack, 1);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addLayout(body, 1);

    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #444;");
    root->addWidget(sep);

    auto* btnRow = new QHBoxLayout;
    btnRow->setContentsMargins(8, 8, 8, 8);
    btnRow->addStretch();
    btnRow->addWidget(buttons);
    root->addLayout(btnRow);
}

QWidget* SettingsDialog::buildUiPage() {
    auto* page   = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto* form = new QFormLayout;
    form->setSpacing(10);

    m_fontCombo = new QFontComboBox(page);
    m_fontCombo->setCurrentFont(m_originalFont);
    form->addRow("Font family:", m_fontCombo);

    m_fontSize = new QSpinBox(page);
    m_fontSize->setRange(7, 28);
    m_fontSize->setValue(m_originalFont.pointSize());
    m_fontSize->setSuffix(" pt");
    m_fontSize->setFixedWidth(80);
    form->addRow("Font size:", m_fontSize);

    auto* wheelRow = new QHBoxLayout;
    m_wheelSlider = new QSlider(Qt::Horizontal, page);
    m_wheelSlider->setRange(25, 300);
    m_wheelSlider->setValue(m_originalWheelSize);
    m_wheelSlider->setTickInterval(25);
    m_wheelSlider->setTickPosition(QSlider::TicksBelow);

    m_wheelSize = new QSpinBox(page);
    m_wheelSize->setRange(25, 300);
    m_wheelSize->setValue(m_originalWheelSize);
    m_wheelSize->setSuffix(" %");
    m_wheelSize->setFixedWidth(72);

    wheelRow->addWidget(m_wheelSlider, 1);
    wheelRow->addWidget(m_wheelSize);
    form->addRow("Wheel diagram:", wheelRow);

    layout->addLayout(form);

    auto* previewLabel = new QLabel("Font preview:", page);
    layout->addWidget(previewLabel);

    m_preview = new QLabel("The quick brown fox jumps over the lazy dog.\n"
                           "UGV Control Station  |  0123456789", page);
    m_preview->setFrameShape(QFrame::StyledPanel);
    m_preview->setWordWrap(true);
    m_preview->setMinimumHeight(60);
    m_preview->setStyleSheet("background: #1e1e1e; padding: 8px; color: #e0e0e0;");
    layout->addWidget(m_preview);

    layout->addStretch();

    connect(m_fontCombo, &QFontComboBox::currentFontChanged,
            this, &SettingsDialog::updatePreview);
    connect(m_fontSize, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SettingsDialog::updatePreview);

    connect(m_wheelSlider, &QSlider::valueChanged,
            m_wheelSize, &QSpinBox::setValue);
    connect(m_wheelSize, QOverload<int>::of(&QSpinBox::valueChanged),
            m_wheelSlider, &QSlider::setValue);
    connect(m_wheelSize, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v) { m_appState.wheelSizePercent.store(v); });

    updatePreview();
    return page;
}

QWidget* SettingsDialog::buildControlsPage() {
    auto* page   = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto* hint = new QLabel("Click Key 1 or Key 2 to reassign.", page);
    hint->setStyleSheet("color: #888;");
    layout->addWidget(hint);

    m_bindingsTable = new QTableWidget(KeyBindings::Count, 3, page);
    m_bindingsTable->setHorizontalHeaderLabels({ "Action", "Key 1", "Key 2" });
    m_bindingsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_bindingsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_bindingsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_bindingsTable->setColumnWidth(1, 110);
    m_bindingsTable->setColumnWidth(2, 110);
    m_bindingsTable->verticalHeader()->setVisible(false);
    m_bindingsTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_bindingsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_bindingsTable->setShowGrid(true);

    populateBindingsTable();
    layout->addWidget(m_bindingsTable, 1);

    auto* resetBtn = new QPushButton("Reset to Defaults", page);
    resetBtn->setFixedWidth(140);
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(resetBtn);
    layout->addLayout(btnRow);

    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        m_editedBindings = KeyBindings{};
        populateBindingsTable();
    });

    connect(m_bindingsTable, &QTableWidget::cellClicked, this,
            [this](int row, int col) {
        if (col == 0) return;

        KeyCaptureDialog dlg(this);
        if (dlg.exec() != QDialog::Accepted) return;
        int newKey = dlg.capturedKey;
        if (newKey == 0) return;

        // Find conflicts in other actions
        int conflictAction = -1;
        for (int i = 0; i < KeyBindings::Count; ++i) {
            if (i == row) continue;
            auto& act = m_editedBindings.actions[i];
            if (act.key1 == newKey || act.key2 == newKey) {
                conflictAction = i;
                break;
            }
        }

        if (conflictAction >= 0) {
            QString keyStr  = keyName(newKey);
            QString actName = m_editedBindings.actions[conflictAction].name;
            QString msg = QString("\"%1\" is assigned to \"%2\".\nDo you want to override it?")
                          .arg(keyStr, actName);
            if (QMessageBox::question(this, "Key Conflict", msg,
                                      QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
                return;

            // Clear all uses of this key in other actions
            for (int i = 0; i < KeyBindings::Count; ++i) {
                if (i == row) continue;
                if (m_editedBindings.actions[i].key1 == newKey) m_editedBindings.actions[i].key1 = 0;
                if (m_editedBindings.actions[i].key2 == newKey) m_editedBindings.actions[i].key2 = 0;
            }
        }

        auto& target = m_editedBindings.actions[row];
        if (col == 1) target.key1 = newKey;
        else          target.key2 = newKey;

        populateBindingsTable();
    });

    return page;
}

void SettingsDialog::populateBindingsTable() {
    m_bindingsTable->blockSignals(true);
    for (int i = 0; i < KeyBindings::Count; ++i) {
        const auto& act = m_editedBindings.actions[i];

        auto* nameItem = new QTableWidgetItem(act.name);
        nameItem->setFlags(Qt::ItemIsEnabled);
        m_bindingsTable->setItem(i, 0, nameItem);

        auto makeKeyItem = [](int k) {
            auto* item = new QTableWidgetItem(keyName(k));
            item->setTextAlignment(Qt::AlignCenter);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            if (k == 0)
                item->setForeground(QColor(80, 80, 80));
            return item;
        };
        m_bindingsTable->setItem(i, 1, makeKeyItem(act.key1));
        m_bindingsTable->setItem(i, 2, makeKeyItem(act.key2));
    }
    m_bindingsTable->blockSignals(false);
}

void SettingsDialog::updatePreview() {
    QFont f = m_fontCombo->currentFont();
    f.setPointSize(m_fontSize->value());
    m_preview->setFont(f);
}

void SettingsDialog::onApply() {
    // Font
    QFont f = m_fontCombo->currentFont();
    f.setPointSize(m_fontSize->value());
    qApp->setFont(f);

    // Wheel size
    int wheelPct = m_wheelSize->value();
    m_appState.wheelSizePercent.store(wheelPct);

    // Key bindings — write to app state and persist
    m_appState.keyBindings = m_editedBindings;

    QSettings s("UGVControlStation", "UGVControlStation");
    s.setValue("ui/fontFamily", f.family());
    s.setValue("ui/fontSize",   f.pointSize());
    s.setValue("ui/wheelSize",  wheelPct);
    for (int i = 0; i < KeyBindings::Count; ++i) {
        s.setValue(QString("keybindings/%1/key1").arg(i), m_editedBindings.actions[i].key1);
        s.setValue(QString("keybindings/%1/key2").arg(i), m_editedBindings.actions[i].key2);
    }
}

void SettingsDialog::onOk() {
    onApply();
    accept();
}

void SettingsDialog::onCancel() {
    qApp->setFont(m_originalFont);
    m_appState.wheelSizePercent.store(m_originalWheelSize);
    m_appState.keyBindings = m_originalBindings;
    reject();
}
