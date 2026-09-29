#include "ui/RadioSettingsDialog.h"
#include "core/ConnectionState.h"
#include "core/ControlState.h"
#include "core/StateSnapshot.h"
#include "io/SerialWorker.h"
#include "ui/SettingsChrome.h"
#include "ui/Theme.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {

constexpr int kSidebarWidth = 160;
constexpr int kDevices      = static_cast<int>(RadioParamClient::kDeviceCount);
constexpr int kMaxFolderDepth = 4;

using DeviceStatus = RadioParamClient::DeviceStatus;

QString qs(const std::string& s) { return QString::fromStdString(s); }

// Device index -> combo entry / status wording (0 = TX module, 1 = receiver;
// same order as RadioParamClient::kAddresses).
const char* deviceName(int device) { return device == 0 ? "TX module" : "Receiver"; }

QLabel* mutedLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setStyleSheet(Theme::colorSS(Theme::textMuted));
    return label;
}

bool hasVisibleChild(const RadioParamClient::DeviceSnapshot& dev, uint8_t folderId) {
    return std::any_of(dev.fields.begin(), dev.fields.end(), [folderId](const crsf::Field& f) {
        return f.loaded && !f.hidden && f.parent == folderId;
    });
}

// While a message box is open its nested event loop keeps firing our timer;
// onTick() stands down for the duration.
struct ModalGuard {
    bool& flag;
    explicit ModalGuard(bool& f) : flag(f) { flag = true; }
    ~ModalGuard() { flag = false; }
};

// Field-name heuristics for settings that matter to the rover's safety. ELRS
// field names and options vary between versions, so these match on substrings
// and are meant to be tightened against the parameter dump the client logs on
// the bench (see the plan's validation step). Not a substitute for that check.
namespace safety {

// A receiver failsafe that keeps replaying the last channel values would keep
// the last drive command alive: the ESP32 counts stale LinkStats as "link up"
// (ugv_manual_control.c), so it would never disarm. Such options are disabled.
bool isBlockedOption(int device, const std::string& fieldName, const std::string& option) {
    if (device != 1) return false;
    const QString f = qs(fieldName).toLower();
    const QString o = qs(option).toLower();
    return f.contains("fail") && (o.contains("last") || o.contains("hold"));
}

// Non-empty when changing this field needs an explicit confirmation.
QString writeWarning(int device, const crsf::Field& field) {
    const QString name = qs(field.name);
    const QString n = name.toLower();

    if (device == 1 && (n.contains("protocol") || n.contains("baud") ||
                        n.contains("invert") || n.contains("sbus")))
        return name + ": the ESP32 expects CRSF at 420000 baud from the receiver. Changing "
                      "this stops the rover receiving commands (it stops safely) until it "
                      "is changed back.";

    if (n.contains("model match"))
        return name + ": no handset sends a model ID, so turning this on can stop the "
                      "transmitter and receiver linking.";

    static const QRegularExpression ble("\\bble\\b");
    if (n.contains("packet rate") || n.contains("switch mode") || n.contains("antenna") ||
        n.contains("domain") || n.contains("regulat") || n.contains("wifi") ||
        n.contains(ble))
        return name + ": may drop the radio link for a few seconds. The rover will disarm "
                      "and must be re-armed.";
    return {};
}

} // namespace safety

} // namespace

// ---- Construction ----------------------------------------------------------

RadioSettingsDialog::RadioSettingsDialog(AppState& state, SerialWorker& worker, QWidget* parent)
    : QDialog(parent)
    , m_state(state)
    , m_radio(worker.radio())
{
    setWindowTitle("Radio Settings");
    setMinimumSize(900, 640);
    resize(900, 640);

    // Device selector, above the sidebar and styled to read as part of it.
    auto* deviceBox = new QWidget(this);
    deviceBox->setObjectName("deviceBox");
    deviceBox->setAttribute(Qt::WA_StyledBackground, true);
    deviceBox->setFixedWidth(kSidebarWidth);
    deviceBox->setStyleSheet(QString("#deviceBox { background: %1; border-right: 1px solid %2; }")
        .arg(SettingsChrome::sidebarBg.name(), SettingsChrome::sidebarBorder.name()));
    auto* deviceLayout = new QVBoxLayout(deviceBox);
    deviceLayout->setContentsMargins(8, 8, 8, 8);
    m_deviceCombo = new QComboBox(deviceBox);
    for (int i = 0; i < kDevices; ++i) m_deviceCombo->addItem(deviceName(i));
    deviceLayout->addWidget(m_deviceCombo);

    m_sidebar = new QListWidget(this);
    m_sidebar->setFixedWidth(kSidebarWidth);
    m_sidebar->setFrameShape(QFrame::NoFrame);
    m_sidebar->setStyleSheet(SettingsChrome::sidebarStyleSheet());

    m_stack = new QStackedWidget(this);
    connect(m_sidebar, &QListWidget::currentRowChanged,
            m_stack,   &QStackedWidget::setCurrentIndex);
    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,          &RadioSettingsDialog::onDeviceChanged);

    // Status strip above the pages.
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setMinimumHeight(44);
    m_status->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_refreshBtn = new QPushButton("Refresh", this);
    m_refreshBtn->setAutoDefault(false);
    connect(m_refreshBtn, &QPushButton::clicked, this, &RadioSettingsDialog::onRefresh);

    auto* statusRow = new QHBoxLayout;
    statusRow->setContentsMargins(16, 8, 16, 8);
    statusRow->addWidget(m_status, 1);
    statusRow->addWidget(m_refreshBtn, 0, Qt::AlignVCenter);

    auto* statusSep = new QFrame(this);
    statusSep->setFrameShape(QFrame::HLine);
    statusSep->setStyleSheet(Theme::colorSS(SettingsChrome::sidebarBorder));

    auto* buttons = new QDialogButtonBox(this);
    m_okBtn     = buttons->addButton(QDialogButtonBox::Ok);
    m_cancelBtn = buttons->addButton(QDialogButtonBox::Cancel);
    m_applyBtn  = buttons->addButton(QDialogButtonBox::Apply);
    // Enter in an editor must not silently apply radio settings. QDialogButtonBox
    // would make OK the default button unless one already exists, so park the
    // default on a hidden button that does nothing.
    for (auto* btn : {m_okBtn, m_cancelBtn, m_applyBtn}) btn->setAutoDefault(false);
    auto* noDefault = new QPushButton(this);
    noDefault->setDefault(true);
    noDefault->hide();

    // The app palette draws disabled widgets like enabled ones; dim them in
    // this dialog so a locked page (armed, loading, no link) is visibly locked.
    QPalette pal = palette();
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        pal.setColor(QPalette::Disabled, role, Theme::textMuted);
    setPalette(pal);

    connect(m_okBtn,     &QPushButton::clicked, this, &RadioSettingsDialog::onOk);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_applyBtn,  &QPushButton::clicked, this, &RadioSettingsDialog::onApply);

    auto* left = new QVBoxLayout;
    left->setContentsMargins(0, 0, 0, 0);
    left->setSpacing(0);
    left->addWidget(deviceBox);
    left->addWidget(m_sidebar, 1);

    auto* right = new QVBoxLayout;
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(0);
    right->addLayout(statusRow);
    right->addWidget(statusSep);
    right->addWidget(m_stack, 1);

    auto* body = new QHBoxLayout;
    body->setSpacing(0);
    body->setContentsMargins(0, 0, 0, 0);
    body->addLayout(left);
    body->addLayout(right, 1);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addLayout(body, 1);

    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(Theme::colorSS(SettingsChrome::sidebarBorder));
    root->addWidget(sep);

    auto* btnRow = new QHBoxLayout;
    btnRow->setContentsMargins(8, 8, 8, 8);
    btnRow->addStretch();
    btnRow->addWidget(buttons);
    root->addLayout(btnRow);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &RadioSettingsDialog::onTick);
    m_timer->start(Theme::kUiRefreshMs);
    onTick();   // starts the load and fills in the first state right away
}

// ---- Dialog lifecycle ------------------------------------------------------

void RadioSettingsDialog::reject() {
    if (m_applying) {
        m_note = "Please wait: the changes are still being sent.";
        return;
    }
    QDialog::reject();
}

void RadioSettingsDialog::done(int result) {
    // Stops any parameter traffic still queued for this dialog.
    m_timer->stop();
    m_radio.reset();
    QDialog::done(result);
}

// ---- Polling ---------------------------------------------------------------

void RadioSettingsDialog::onTick() {
    if (m_inModal) return;

    const auto conn = snapshot<ConnectionState>(m_state);
    const auto ctrl = snapshot<ControlState>(m_state);
    m_connected = conn.status == ConnectionStatus::Connected;
    m_armed     = ctrl.armed;

    // Reading (the receiver especially) costs radio bandwidth, so it only
    // starts while disarmed; it starts on its own once the vehicle is disarmed
    // or the link comes back.
    if (!m_connected) {
        m_loadStarted = false;
    } else if (!m_loadStarted && !m_armed) {
        m_radio.beginLoad();
        m_loadStarted = true;
    }

    if (m_radio.revision() != m_shownRevision) {
        m_snap = m_radio.snapshot();
        m_shownRevision = m_snap.revision;
    }

    if (m_snap.confirmation.pending) {
        showPendingConfirmation();
        return;
    }

    // All queued writes (and the read-back) have finished.
    if (m_applying && !m_snap.busy) {
        m_applying = false;
        if (m_acceptWhenIdle) {
            m_acceptWhenIdle = false;
            // Stay open if something needs the operator's attention.
            if (!m_snap.writesUnconfirmed) {
                accept();
                return;
            }
        }
    }

    syncPages();
    updateControls();
}

void RadioSettingsDialog::syncPages() {
    const int device = currentDevice();
    const auto& dev = m_snap.devices[static_cast<size_t>(device)];
    const uint32_t generation = dev.status == DeviceStatus::Ready ? dev.generation : 0u;
    if (device != m_builtDevice || generation != m_builtGeneration)
        buildDevicePages(device);
}

void RadioSettingsDialog::onDeviceChanged(int /*index*/) {
    syncPages();
    updateControls();
}

int RadioSettingsDialog::currentDevice() const {
    return std::clamp(m_deviceCombo->currentIndex(), 0, kDevices - 1);
}

// ---- State queries ---------------------------------------------------------

const crsf::Field* RadioSettingsDialog::originalField(const Key& key) const {
    if (key.first < 0 || key.first >= kDevices) return nullptr;
    const auto& dev = m_snap.devices[static_cast<size_t>(key.first)];
    if (dev.status != DeviceStatus::Ready) return nullptr;
    if (key.second == 0 || key.second > dev.fields.size()) return nullptr;
    return &dev.fields[static_cast<size_t>(key.second) - 1u];
}

bool RadioSettingsDialog::canEdit() const {
    const auto& dev = m_snap.devices[static_cast<size_t>(currentDevice())];
    return m_connected && !m_armed && !m_applying && !m_snap.busy &&
           dev.status == DeviceStatus::Ready;
}

bool RadioSettingsDialog::canRefresh() const {
    return m_connected && !m_armed && !m_applying && !m_snap.busy;
}

QString RadioSettingsDialog::statusText() const {
    if (!m_connected)
        return "Not connected. Connect to the TX module to read its settings.";

    QStringList lines;
    if (m_armed)
        lines << "The vehicle is armed. Disarm it to read or change radio settings.";

    const int device = currentDevice();
    const auto& dev = m_snap.devices[static_cast<size_t>(device)];
    const QString name = QString(deviceName(device)).toLower();
    switch (dev.status) {
        case DeviceStatus::Idle:
            if (!m_armed) lines << "Waiting to read settings...";
            break;
        case DeviceStatus::Pinging:
            lines << QString("Contacting the %1...").arg(name);
            break;
        case DeviceStatus::Loading:
            lines << QString("Reading %1 settings: %2 of %3")
                         .arg(name).arg(static_cast<int>(dev.loaded))
                         .arg(static_cast<int>(dev.fields.size()));
            break;
        case DeviceStatus::Ready:
            lines << QString("%1: %2 parameters")
                         .arg(dev.info.name.empty() ? QString(deviceName(device))
                                                    : qs(dev.info.name))
                         .arg(static_cast<int>(dev.fields.size()));
            break;
        case DeviceStatus::NoResponse:
            lines << QString("The %1 is not answering.").arg(name);
            break;
    }
    if (!m_snap.message.empty()) lines << qs(m_snap.message);
    if (!m_note.isEmpty())       lines << m_note;
    return lines.join('\n');
}

// ---- Page building ---------------------------------------------------------

void RadioSettingsDialog::buildDevicePages(int device) {
    const auto& dev = m_snap.devices[static_cast<size_t>(device)];
    const bool ready = dev.status == DeviceStatus::Ready;

    const QString previous = m_sidebar->currentItem() ? m_sidebar->currentItem()->text()
                                                      : QString();
    m_builtDevice     = device;
    m_builtGeneration = ready ? dev.generation : 0u;

    // A fresh load may have changed values or fields under edits kept from before
    // (a reconnect, say): drop staged edits that no longer differ from the device.
    if (ready) {
        for (auto it = m_staged.begin(); it != m_staged.end();) {
            bool keep = true;
            if (it->first.first == device) {
                const crsf::Field* f = originalField(it->first);
                keep = f != nullptr && f->loaded &&
                       (f->type == crsf::FieldType::String ? it->second.text != f->text
                                                           : it->second.number != f->value);
            }
            it = keep ? std::next(it) : m_staged.erase(it);
        }
    }

    m_labels.clear();
    {
        QSignalBlocker block(m_sidebar);
        m_sidebar->clear();
    }
    while (m_stack->count() > 0) {
        QWidget* old = m_stack->widget(0);
        m_stack->removeWidget(old);
        old->deleteLater();
    }

    const auto addPlaceholder = [this](const QString& text) {
        auto* page = new QWidget;
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(20, 20, 20, 20);
        auto* label = mutedLabel(text, page);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);
        m_stack->addWidget(page);
    };

    if (!ready) {
        addPlaceholder("Settings appear here once the device has been read.");
        return;
    }

    const bool hasGeneral = std::any_of(dev.fields.begin(), dev.fields.end(),
        [](const crsf::Field& f) {
            return f.loaded && !f.hidden && f.parent == 0 && f.type != crsf::FieldType::Folder;
        });
    if (hasGeneral) {
        m_sidebar->addItem("General");
        m_stack->addWidget(buildFolderPage(device, 0, false));
    }
    for (const auto& f : dev.fields) {
        if (!f.loaded || f.hidden || f.parent != 0 || f.type != crsf::FieldType::Folder ||
            !hasVisibleChild(dev, f.id))
            continue;
        m_sidebar->addItem(qs(f.name));
        m_stack->addWidget(buildFolderPage(device, f.id, true));
    }

    if (m_sidebar->count() == 0) {
        addPlaceholder("This device reports no settings.");
        return;
    }

    int row = 0;
    if (!previous.isEmpty()) {
        const auto matches = m_sidebar->findItems(previous, Qt::MatchExactly);
        if (!matches.isEmpty()) row = m_sidebar->row(matches.first());
    }
    m_sidebar->setCurrentRow(row);
    updateDirtyMarks();
}

QWidget* RadioSettingsDialog::buildFolderPage(int device, uint8_t folderId,
                                              bool includeSubfolders) {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto* form = new QFormLayout;
    form->setSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addLayout(form);

    addFieldsUnder(form, page, device, folderId, includeSubfolders, 0);

    layout->addWidget(mutedLabel(
        "Changes are sent when you press Apply or OK. Buttons run immediately.", page));
    layout->addStretch();

    auto* scroll = new QScrollArea(m_stack);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(page);
    return scroll;
}

void RadioSettingsDialog::addFieldsUnder(QFormLayout* form, QWidget* page, int device,
                                         uint8_t parentId, bool includeSubfolders, int depth) {
    const auto& dev = m_snap.devices[static_cast<size_t>(device)];
    for (const auto& f : dev.fields) {
        if (!f.loaded || f.hidden || f.parent != parentId) continue;

        if (f.type == crsf::FieldType::Folder) {
            if (!includeSubfolders || depth >= kMaxFolderDepth || !hasVisibleChild(dev, f.id))
                continue;
            auto* heading = new QLabel(qs(f.name), page);
            heading->setStyleSheet(Theme::colorSS(Theme::textDim) + "font-weight: bold;");
            form->addRow(heading);
            addFieldsUnder(form, page, device, f.id, true, depth + 1);
            continue;
        }
        addFieldRow(form, page, device, f);
    }
}

void RadioSettingsDialog::addFieldRow(QFormLayout* form, QWidget* page, int device,
                                      const crsf::Field& f) {
    const Key key{device, f.id};
    auto* label = new QLabel(qs(f.name) + ":", page);
    QWidget* input = nullptr;

    const auto stagedIt = m_staged.find(key);
    const bool hasStaged = stagedIt != m_staged.end();

    // Every editor is filled in before its change signal is connected, so
    // building a page never stages anything.
    switch (f.type) {
        case crsf::FieldType::Uint8:
        case crsf::FieldType::Int8:
        case crsf::FieldType::Uint16:
        case crsf::FieldType::Int16: {
            auto* spin = new QSpinBox(page);
            spin->setRange(std::min(f.min, f.max), std::max(f.min, f.max));
            if (!f.unit.empty()) spin->setSuffix(" " + qs(f.unit));
            spin->setValue(hasStaged ? stagedIt->second.number : f.value);
            spin->setMinimumWidth(110);
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                    [this, key](int v) { stageNumber(key, v); });
            input = spin;
            break;
        }
        case crsf::FieldType::Float: {
            const double scale = std::pow(10.0, f.decimals);
            auto* spin = new QDoubleSpinBox(page);
            spin->setDecimals(f.decimals);
            spin->setRange(std::min(f.min, f.max) / scale, std::max(f.min, f.max) / scale);
            spin->setSingleStep((f.step > 0 ? f.step : 1) / scale);
            if (!f.unit.empty()) spin->setSuffix(" " + qs(f.unit));
            spin->setValue((hasStaged ? stagedIt->second.number : f.value) / scale);
            spin->setMinimumWidth(110);
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                    [this, key, scale](double v) {
                        stageNumber(key, static_cast<int32_t>(std::lround(v * scale)));
                    });
            input = spin;
            break;
        }
        case crsf::FieldType::TextSelection: {
            auto* combo = new QComboBox(page);
            const int count = static_cast<int>(f.options.size());
            int lo = std::max(0, f.min);
            int hi = std::min(f.max, count - 1);
            if (hi < lo) { lo = 0; hi = count - 1; }   // implausible range: offer everything

            auto* model = qobject_cast<QStandardItemModel*>(combo->model());
            for (int i = lo; i <= hi; ++i) {
                const std::string& option = f.options[static_cast<size_t>(i)];
                combo->addItem(qs(option), i);
                if (model != nullptr && safety::isBlockedOption(device, f.name, option)) {
                    QStandardItem* item = model->item(combo->count() - 1);
                    item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                    item->setToolTip("Blocked: this option would keep replaying the last "
                                     "drive command if the radio link is lost.");
                }
            }
            const int current = hasStaged ? stagedIt->second.number : f.value;
            int idx = combo->findData(current);
            if (idx < 0 && current >= 0 && current < count) {
                // The device's current value lies outside its own allowed range: show it anyway.
                combo->addItem(qs(f.options[static_cast<size_t>(current)]), current);
                idx = combo->count() - 1;
            }
            combo->setCurrentIndex(idx);
            combo->setMinimumWidth(200);
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                    [this, key, combo](int i) {
                        if (i >= 0) stageNumber(key, combo->itemData(i).toInt());
                    });
            input = combo;
            break;
        }
        case crsf::FieldType::String: {
            auto* edit = new QLineEdit(page);
            if (f.maxLen > 0) edit->setMaxLength(f.maxLen);
            edit->setText(qs(hasStaged ? stagedIt->second.text : f.text));
            edit->setMinimumWidth(220);
            connect(edit, &QLineEdit::textChanged, this,
                    [this, key](const QString& t) { stageText(key, t.toStdString()); });
            input = edit;
            break;
        }
        case crsf::FieldType::Info:
            input = mutedLabel(qs(f.text), page);
            break;
        case crsf::FieldType::Command: {
            auto* button = new QPushButton("Run", page);
            button->setAutoDefault(false);
            button->setFixedWidth(100);
            if (!f.text.empty()) button->setToolTip(qs(f.text));
            const QString name = qs(f.name);
            connect(button, &QPushButton::clicked, this,
                    [this, device, id = f.id, name]() { onRunCommand(device, id, name); });
            input = button;
            break;
        }
        default:
            input = mutedLabel("Not editable here", page);
            break;
    }

    m_labels[key] = label;
    form->addRow(label, input);
}

// ---- Staging ---------------------------------------------------------------

void RadioSettingsDialog::stageNumber(const Key& key, int32_t value) {
    const crsf::Field* f = originalField(key);
    if (f == nullptr) return;
    if (value == f->value) m_staged.erase(key);
    else                   m_staged[key] = Staged{ value, {} };
    updateDirtyMarks();
    updateControls();
}

void RadioSettingsDialog::stageText(const Key& key, const std::string& value) {
    const crsf::Field* f = originalField(key);
    if (f == nullptr) return;
    if (value == f->text) m_staged.erase(key);
    else                  m_staged[key] = Staged{ 0, value };
    updateDirtyMarks();
    updateControls();
}

// Edited rows get a bold accent label and a trailing "*".
void RadioSettingsDialog::updateDirtyMarks() {
    for (const auto& [key, label] : m_labels) {
        const crsf::Field* f = originalField(key);
        if (f == nullptr) continue;
        const bool dirty = m_staged.count(key) != 0;
        label->setText(qs(f->name) + (dirty ? " *:" : ":"));
        label->setStyleSheet(dirty ? Theme::colorSS(Theme::accent) + "font-weight: bold;"
                                   : QString());
    }
}

void RadioSettingsDialog::updateControls() {
    const bool editable = canEdit();
    m_stack->setEnabled(editable);
    m_applyBtn->setEnabled(editable && !m_staged.empty());
    m_okBtn->setEnabled(!m_applying);
    m_cancelBtn->setEnabled(!m_applying);
    m_refreshBtn->setEnabled(canRefresh());
    m_status->setText(statusText());
}

// ---- Actions ---------------------------------------------------------------

void RadioSettingsDialog::onApply() {
    if (!canEdit() || m_staged.empty()) return;
    m_note.clear();
    queueApply();
    updateControls();
}

void RadioSettingsDialog::onOk() {
    if (m_staged.empty()) {
        accept();
        return;
    }
    if (!canEdit()) {
        ModalGuard modal(m_inModal);
        QMessageBox::information(this, "Radio settings",
            "The changes can't be applied right now.\n\n" + statusText());
        return;
    }
    m_note.clear();
    // Stay open until everything has been sent and read back, then close.
    if (queueApply()) m_acceptWhenIdle = true;
    updateControls();
}

void RadioSettingsDialog::onRefresh() {
    if (!canRefresh()) return;
    if (!m_staged.empty()) {
        ModalGuard modal(m_inModal);
        if (QMessageBox::question(this, "Refresh", "Discard the changes you have not applied?",
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No) != QMessageBox::Yes)
            return;
    }
    m_staged.clear();
    m_note.clear();
    m_radio.beginLoad();
    m_loadStarted = true;
}

// Sends every staged change. The receiver goes first: its writes travel over
// the RF link, which a change to the TX module (packet rate, ...) can interrupt.
bool RadioSettingsDialog::queueApply() {
    std::vector<RadioParamClient::DeviceWrites> batches;
    QStringList warnings;

    for (int device = kDevices - 1; device >= 0; --device) {
        RadioParamClient::DeviceWrites batch;
        batch.address = RadioParamClient::kAddresses[static_cast<size_t>(device)];
        for (const auto& [key, staged] : m_staged) {
            if (key.first != device) continue;
            const crsf::Field* f = originalField(key);
            if (f == nullptr) continue;

            RadioParamClient::WriteRequest write;
            write.fieldId = f->id;
            write.value = f->type == crsf::FieldType::String
                              ? crsf::encodeText(staged.text)
                              : crsf::encodeNumber(f->type, staged.number);
            if (write.value.empty()) continue;
            batch.writes.push_back(std::move(write));

            const QString warning = safety::writeWarning(device, *f);
            if (!warning.isEmpty()) warnings << warning;
        }
        if (!batch.writes.empty()) batches.push_back(std::move(batch));
    }
    if (batches.empty()) return false;

    if (!warnings.isEmpty()) {
        ModalGuard modal(m_inModal);
        const QString text =
            "These changes can interrupt the radio link or change how the rover receives "
            "commands:\n\n\xE2\x80\xA2 " + warnings.join("\n\xE2\x80\xA2 ") + "\n\nApply anyway?";
        if (QMessageBox::warning(this, "Apply radio settings", text,
                                 QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No) != QMessageBox::Yes)
            return false;
    }

    if (!m_radio.applyWrites(batches)) {
        m_note = "The changes could not be sent: the device is busy or not ready.";
        return false;
    }
    m_staged.clear();
    updateDirtyMarks();
    m_applying = true;
    return true;
}

void RadioSettingsDialog::onRunCommand(int device, uint8_t fieldId, const QString& name) {
    if (!canEdit()) return;
    {
        ModalGuard modal(m_inModal);
        const QString text = QString("Run \"%1\" on the %2 now?\n\nSome actions interrupt the "
                                     "radio link; the rover will disarm and must be re-armed.")
                                 .arg(name, QString(deviceName(device)).toLower());
        if (QMessageBox::question(this, "Run action", text,
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No) != QMessageBox::Yes)
            return;
    }
    if (m_radio.runCommand(RadioParamClient::kAddresses[static_cast<size_t>(device)], fieldId))
        m_note.clear();
    else
        m_note = "The action could not be started: the device is busy or not ready.";
    updateControls();
}

// A running command (Bind, WiFi update, ...) asked the operator to confirm.
void RadioSettingsDialog::showPendingConfirmation() {
    const QString title  = qs(m_snap.confirmation.fieldName);
    const QString prompt = m_snap.confirmation.prompt.empty()
                               ? QString("Confirm this action?")
                               : qs(m_snap.confirmation.prompt);
    bool confirmed = false;
    {
        ModalGuard modal(m_inModal);
        confirmed = QMessageBox::question(this, title, prompt,
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No) == QMessageBox::Yes;
    }
    m_radio.answerConfirmation(confirmed);
}
