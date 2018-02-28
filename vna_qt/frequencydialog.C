#include "utility.H"
#include "frequencydialog.H"
#include "ui_frequencydialog.h"
#include <xavna/xavna_cpp.H>

using namespace xaxaxa;
FrequencyDialog::FrequencyDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FrequencyDialog)
{
    ui->setupUi(this);
}

FrequencyDialog::~FrequencyDialog()
{
    delete ui;
}

void FrequencyDialog::fromVNA(const VNADevice &dev) {
    ui->t_start->setText(qs(ssprintf(32,"%.2f",dev.startFreqHz*1e-6)));
    ui->t_step->setText(qs(ssprintf(32,"%.2f",dev.stepFreqHz*1e-6)));
    ui->t_points->setText(qs(to_string(dev.nPoints)));
    ui->slider_power->setRange(dev.maxPower()-40, dev.maxPower());
    ui->slider_power->setValue(dev.maxPower() - dev.attenuation1);
    emit on_slider_power_valueChanged(ui->slider_power->value());
}

bool FrequencyDialog::toVNA(VNADevice &dev) {
    double oldStartFreq = dev.startFreqHz;
    double oldStepFreq = dev.stepFreqHz;
    double oldNPoints = dev.nPoints;
    dev.startFreqHz = atof(ui->t_start->text().toUtf8().data())*1e6;
    dev.stepFreqHz = atof(ui->t_step->text().toUtf8().data())*1e6;
    dev.nPoints = atoi(ui->t_points->text().toUtf8().data());
    dev.attenuation1 = dev.attenuation2 = dev.maxPower() - ui->slider_power->value();

    return (dev.startFreqHz != oldStartFreq) || (dev.stepFreqHz != oldStepFreq) || (dev.nPoints != oldNPoints);
}

void FrequencyDialog::updateLabels() {
    double endFreq = atof(ui->t_start->text().toUtf8().data())
            + (atoi(ui->t_points->text().toUtf8().data())
               * atof(ui->t_step->text().toUtf8().data()));
    if(!isnan(endFreq))
        ui->l_end->setText(qs(ssprintf(32, "%.2f", endFreq)));
}

void FrequencyDialog::on_slider_power_valueChanged(int value) {
    ui->l_power->setText(qs(ssprintf(32, "%d dBm", value)));
}

void FrequencyDialog::on_t_start_textChanged(const QString &) {
    updateLabels();
}

void FrequencyDialog::on_t_step_textChanged(const QString &) {
    updateLabels();
}

void FrequencyDialog::on_t_points_textChanged(const QString &) {
    updateLabels();
}
