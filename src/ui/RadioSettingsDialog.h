#pragma once
#include "core/AppState.h"
#include "io/RadioParamClient.h"
#include <QDialog>
#include <QString>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

class QComboBox;
class QFormLayout;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTimer;
class QWidget;
class SerialWorker;

// Radio settings popup (gear button next to Connect): the menu an EdgeTX
// handset shows through the ExpressLRS Lua script, for the Nomad TX module and
// the XR4 receiver, read and written over CRSF by RadioParamClient.
//
// Laid out like the Preferences dialog: sidebar + stacked pages + OK / Cancel /
// Apply. The sidebar lists the selected device's folders and is built from what
// the device reports, so it follows whatever the installed ELRS version offers.
//
// Edits are staged locally and only sent on Apply / OK, so Cancel or Esc
// discards them. Editing is disabled while the vehicle is armed, while the
// device is (re)loading, and when the serial link is down.
class RadioSettingsDialog : public QDialog {
    Q_OBJECT
public:
    RadioSettingsDialog(AppState& state, SerialWorker& worker, QWidget* parent = nullptr);

    // Ignored while changes are being sent, so a half-applied set is never left behind.
    void reject() override;
    void done(int result) override;

private slots:
    void onTick();
    void onApply();
    void onOk();
    void onRefresh();
    void onDeviceChanged(int index);

private:
    // (device index, field id)
    using Key = std::pair<int, uint8_t>;

    struct Staged {
        int32_t     number = 0;
        std::string text;
    };

    const crsf::Field* originalField(const Key& key) const;
    bool    canEdit() const;
    bool    canRefresh() const;
    QString statusText() const;
    int     currentDevice() const;

    void syncPages();
    void buildDevicePages(int device);
    QWidget* buildFolderPage(int device, uint8_t folderId, bool includeSubfolders);
    void addFieldsUnder(QFormLayout* form, QWidget* page, int device,
                        uint8_t parentId, bool includeSubfolders, int depth);
    void addFieldRow(QFormLayout* form, QWidget* page, int device, const crsf::Field& field);

    void stageNumber(const Key& key, int32_t value);
    void stageText(const Key& key, const std::string& value);
    void updateDirtyMarks();
    void updateControls();

    bool queueApply();
    void onRunCommand(int device, uint8_t fieldId, const QString& name);
    void showPendingConfirmation();

    AppState&         m_state;
    RadioParamClient& m_radio;

    QComboBox*      m_deviceCombo = nullptr;
    QListWidget*    m_sidebar     = nullptr;
    QStackedWidget* m_stack       = nullptr;
    QLabel*         m_status      = nullptr;
    QPushButton*    m_refreshBtn  = nullptr;
    QPushButton*    m_okBtn       = nullptr;
    QPushButton*    m_cancelBtn   = nullptr;
    QPushButton*    m_applyBtn    = nullptr;
    QTimer*         m_timer       = nullptr;

    RadioParamClient::Snapshot m_snap;
    uint32_t m_shownRevision = 0;

    // Which device / load generation the visible pages were built from
    // (generation 0 = placeholder page).
    int      m_builtDevice     = -1;
    uint32_t m_builtGeneration = 0;

    std::map<Key, Staged>  m_staged;
    std::map<Key, QLabel*> m_labels;   // row labels of the visible pages, for dirty marks

    bool    m_applying        = false;
    bool    m_acceptWhenIdle  = false;
    bool    m_loadStarted     = false;
    bool    m_inModal         = false;   // a message box is open: onTick stands down
    bool    m_connected       = false;
    bool    m_armed           = false;
    QString m_note;                      // local status line (e.g. an apply that could not start)
};
