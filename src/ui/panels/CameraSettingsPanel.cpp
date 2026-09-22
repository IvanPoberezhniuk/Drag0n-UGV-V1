#include "ui/panels/CameraSettingsPanel.h"

#include "core/AppState.h"
#include "ui/Theme.h"
#include "ui/widgets/ToggleSwitch.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSlider>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

template <typename Widget>
Widget* compactField(Widget* widget) {
    widget->setMinimumWidth(120);
    widget->setMaximumWidth(190);
    return widget;
}

QComboBox* combo(QWidget* parent, const QStringList& items, int current = 0) {
    auto* field = compactField(new QComboBox(parent));
    field->addItems(items);
    field->setCurrentIndex(current);
    return field;
}

QFormLayout* compactForm() {
    auto* form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(7);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return form;
}

struct FormColumn {
    QWidget* widget;
    QFormLayout* form;
};

FormColumn formColumn(QWidget* parent, const QString& title) {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(7, 5, 7, 7);
    layout->setSpacing(6);

    auto* heading = new QLabel(title, widget);
    heading->setStyleSheet(Theme::colorSS(Theme::textMuted));
    layout->addWidget(heading);

    auto* form = compactForm();
    layout->addLayout(form);
    layout->addStretch();
    return {widget, form};
}

QWidget* sliderField(QWidget* parent, int minimum, int maximum, int value,
                     double divisor = 1.0, const QString& suffix = {},
                     const QString& prefix = {}, int decimals = 0) {
    auto* field = new QWidget(parent);
    auto* row = new QHBoxLayout(field);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(7);

    auto* slider = new QSlider(Qt::Horizontal, field);
    slider->setRange(minimum, maximum);
    slider->setValue(value);
    slider->setMinimumWidth(105);

    auto* valueLabel = new QLabel(field);
    valueLabel->setMinimumWidth(66);
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    const auto updateLabel = [=](int raw) {
        valueLabel->setText(prefix
            + QString::number(static_cast<double>(raw) / divisor, 'f', decimals)
            + suffix);
    };
    QObject::connect(slider, &QSlider::valueChanged, field, updateLabel);
    updateLabel(value);

    row->addWidget(slider, 1);
    row->addWidget(valueLabel);
    field->setMinimumWidth(190);
    return field;
}

QScrollArea* scrollable(QWidget* page, QWidget* parent) {
    auto* area = new QScrollArea(parent);
    area->setWidget(page);
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    return area;
}

} // namespace

CameraSettingsPanel::CameraSettingsPanel(AppState& state, QWidget* parent)
    : IPanel(parent), m_state(state)
{
    setMinimumWidth(300);
    setMinimumHeight(170);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(7, 5, 7, 6);
    root->setSpacing(4);

    auto* notice = new QLabel("LOCAL DRAFT  •  not sent to rover", this);
    notice->setStyleSheet(Theme::colorSS(Theme::textMuted));
    notice->setToolTip("TODO: send validated camera presets to a Raspberry Pi control service.");
    root->addWidget(notice);

    auto* tabs = new QTabWidget(this);
    tabs->setDocumentMode(true);
    tabs->addTab(scrollable(buildEssentialPage(), tabs), "S Tier");
    tabs->addTab(scrollable(buildAdvancedPage(), tabs), "A Tier");
    root->addWidget(tabs, 1);

    // TODO(camera-control): connect these draft fields to an authenticated
    // Raspberry Pi control API. Applying a preset must validate the complete
    // combination, update MediaMTX atomically, restart only the camera source,
    // and report success/failure back to this panel.
    // TODO(camera-control): populate the fields from the Pi's active camera
    // configuration rather than from the known-good defaults below.
}

QWidget* CameraSettingsPanel::buildEssentialPage() {
    auto* page = new QWidget(this);
    auto* grid = new QGridLayout(page);
    grid->setContentsMargins(4, 3, 4, 4);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(4);

    auto video = formColumn(page, "VIDEO");
    auto image = formColumn(page, "IMAGE");
    auto optics = formColumn(page, "OPTICS");

    video.form->addRow("Resolution:", combo(page,
        {"1280 x 720", "1920 x 1080", "2304 x 1296"}, 2));
    video.form->addRow("Quality:", combo(page,
        {"Low (2 Mbit/s)", "Balanced (5 Mbit/s)", "High (8 Mbit/s)"}, 2));
    auto* stream = new QCheckBox("Enabled", page);
    stream->setChecked(true);
    video.form->addRow("Camera stream:", stream);

    image.form->addRow("Exposure:",
        sliderField(page, -20, 20, 0, 2.0, {}, "EV ", 1));
    auto* hdr = new QCheckBox("Enabled", page);
    image.form->addRow("HDR:", hdr);
    image.form->addRow("Denoising:", combo(page,
        {"High quality", "Fast", "Off"}));

    optics.form->addRow("Digital zoom:",
        sliderField(page, 4, 16, 4, 4.0, "x", {}, 2));
    optics.form->addRow("Focus:", combo(page,
        {"Continuous", "Center auto", "Fixed"}));

    grid->addWidget(video.widget, 0, 0);
    grid->addWidget(image.widget, 0, 1);
    grid->addWidget(optics.widget, 0, 2);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    grid->setRowStretch(1, 1);
    return page;
}

QWidget* CameraSettingsPanel::buildAdvancedPage() {
    auto* page = new QWidget(this);
    auto* grid = new QGridLayout(page);
    grid->setContentsMargins(4, 3, 4, 4);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(4);

    auto capture = formColumn(page, "CAPTURE");
    auto color = formColumn(page, "COLOR");
    auto sensor = formColumn(page, "SENSOR");
    auto output = formColumn(page, "OUTPUT");

    capture.form->addRow("Frame rate:",
        sliderField(page, 1, 30, 30, 1.0, " fps"));
    capture.form->addRow("Brightness:",
        sliderField(page, -10, 10, 0, 10.0, {}, {}, 1));
    capture.form->addRow("Contrast:",
        sliderField(page, 0, 160, 10, 10.0, {}, {}, 1));

    color.form->addRow("Saturation:",
        sliderField(page, 0, 160, 10, 10.0, {}, {}, 1));
    color.form->addRow("Sharpness:",
        sliderField(page, 0, 160, 10, 10.0, {}, {}, 1));
    color.form->addRow("White balance:", combo(page,
        {"Auto", "Daylight", "Cloudy", "Indoor", "Fluorescent", "Tungsten"}));

    sensor.form->addRow("Shutter:", combo(page,
        {"Auto", "1/30 s", "1/60 s", "1/120 s", "1/250 s"}));
    sensor.form->addRow("Sensor gain:", combo(page,
        {"Auto", "1x", "2x", "4x", "8x"}));
    sensor.form->addRow("Anti-flicker:", combo(page,
        {"Auto", "50 Hz", "60 Hz"}));

    auto* flipRow = new QWidget(page);
    auto* flips = new QHBoxLayout(flipRow);
    flips->setContentsMargins(0, 0, 0, 0);
    auto* horizontal = new ToggleSwitch("Horizontal", flipRow);
    auto* vertical = new ToggleSwitch("Vertical", flipRow);
    flips->addWidget(horizontal);
    flips->addWidget(vertical);
    flips->addStretch();
    output.form->addRow("Flip:", flipRow);
    output.form->addRow("Keyframe interval:",
        sliderField(page, 1, 120, 30, 1.0, " frames"));

    grid->addWidget(capture.widget, 0, 0);
    grid->addWidget(color.widget, 0, 1);
    grid->addWidget(sensor.widget, 0, 2);
    grid->addWidget(output.widget, 0, 3);
    for (int column = 0; column < 4; ++column)
        grid->setColumnStretch(column, 1);
    grid->setRowStretch(1, 1);
    return page;
}

void CameraSettingsPanel::refresh() {
    // Intentionally empty until the Pi control/status API exists. Keeping the
    // draft independent from CameraState prevents live telemetry refreshes
    // from overwriting values while the operator explores a future preset.
    (void)m_state;
}

QSize CameraSettingsPanel::sizeHint() const {
    return {360, 220};
}
