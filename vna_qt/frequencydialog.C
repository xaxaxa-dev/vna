#include "utility.H"
#include "frequencydialog.H"
#include "ui_frequencydialog.h"
#include <xavna/xavna_cpp.H>
#include <xavna/workarounds.H>

using namespace xaxaxa;
FrequencyDialog::FrequencyDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FrequencyDialog)
{
    ui->setupUi(this);
    ui->w_advanced->setVisible(false);
    this->resize(this->width(),0);
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
    ui->t_nValues->setText(qs(ssprintf(32, "%d", dev.nValues)));
    ui->t_nWait->setText(qs(ssprintf(32, "%d", dev.nWait)));
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
    dev.nValues = atoi(ui->t_nValues->text().toUtf8().data());
    dev.nWait = atoi(ui->t_nWait->text().toUtf8().data());
    return (dev.startFreqHz != oldStartFreq) || (dev.stepFreqHz != oldStepFreq) || (dev.nPoints != oldNPoints);
}

void FrequencyDialog::updateLabels() {
    double endFreq = atof(ui->t_start->text().toUtf8().data())
            + (atoi(ui->t_points->text().toUtf8().data())
               * atof(ui->t_step->text().toUtf8().data()));
    if(!std::isnan(endFreq))
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

void FrequencyDialog::on_c_advanced_stateChanged(int) {
    ui->w_advanced->setVisible(ui->c_advanced->isChecked());
}
