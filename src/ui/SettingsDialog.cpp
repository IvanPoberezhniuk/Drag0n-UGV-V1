#include "ui/SettingsDialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QFontComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QApplication>
#include <QSettings>
#include <QFrame>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
    , m_originalFont(qApp->font())
{
    setWindowTitle("Preferences");
    setMinimumSize(520, 360);

    // ── Sidebar ────────────────────────────────────────────────────────────
    m_sidebar = new QListWidget(this);
    m_sidebar->setFixedWidth(120);
    m_sidebar->setFrameShape(QFrame::NoFrame);
    m_sidebar->setStyleSheet(
        "QListWidget { background: #2a2a2a; border-right: 1px solid #444; }"
        "QListWidget::item { padding: 10px 12px; color: #ccc; }"
        "QListWidget::item:selected { background: #3a3a3a; color: white; "
        "  border-left: 3px solid #00c850; }");
    m_sidebar->addItem("UI");
    m_sidebar->setCurrentRow(0);

    // ── Content stack ──────────────────────────────────────────────────────
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildUiPage());

    connect(m_sidebar, &QListWidget::currentRowChanged,
            m_stack,   &QStackedWidget::setCurrentIndex);

    // ── Buttons ────────────────────────────────────────────────────────────
    auto* buttons = new QDialogButtonBox(this);
    auto* okBtn     = buttons->addButton(QDialogButtonBox::Ok);
    auto* cancelBtn = buttons->addButton(QDialogButtonBox::Cancel);
    auto* applyBtn  = buttons->addButton(QDialogButtonBox::Apply);

    connect(okBtn,     &QPushButton::clicked, this, &SettingsDialog::onOk);
    connect(cancelBtn, &QPushButton::clicked, this, &SettingsDialog::onCancel);
    connect(applyBtn,  &QPushButton::clicked, this, &SettingsDialog::onApply);

    // ── Main layout ────────────────────────────────────────────────────────
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

    layout->addLayout(form);

    // Preview box
    auto* previewLabel = new QLabel("Preview:", page);
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

    updatePreview();
    return page;
}

void SettingsDialog::updatePreview() {
    QFont f = m_fontCombo->currentFont();
    f.setPointSize(m_fontSize->value());
    m_preview->setFont(f);
}

void SettingsDialog::onApply() {
    QFont f = m_fontCombo->currentFont();
    f.setPointSize(m_fontSize->value());
    qApp->setFont(f);

    QSettings s("UGVControlStation", "UGVControlStation");
    s.setValue("ui/fontFamily", f.family());
    s.setValue("ui/fontSize",   f.pointSize());
}

void SettingsDialog::onOk() {
    onApply();
    accept();
}

void SettingsDialog::onCancel() {
    qApp->setFont(m_originalFont);
    reject();
}
