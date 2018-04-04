#include <map>
#include <QLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <xavna/calibration.H>
#include "calkitsettingsdialog.H"
#include "ui_calkitsettingsdialog.h"
#include "ui_calkitsettingswidget.h"
#include "utility.H"
#include "touchstone.H"

using namespace xaxaxa;
using namespace std;

CalKitSettingsDialog::CalKitSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CalKitSettingsDialog)
{
    ui->setupUi(this);
}

CalKitSettingsDialog::~CalKitSettingsDialog()
{
    delete ui;
}

void CalKitSettingsDialog::fromSettings(const CalKitSettings &settings) {
    info.clear();
    map<string, string> calStdDesc;
    for(const VNACalibration* cal: calibrationTypes) {
        for(auto tmp:cal->getRequiredStandards()) {
            calStdDesc[tmp[0]] = tmp[1];
        }
    }

    QBoxLayout* layout = new QBoxLayout(QBoxLayout::TopToBottom);
    //layout->setMargin(0);
    delete ui->w_content->layout();
    ui->w_content->setLayout(layout);

    for(auto& item:idealCalStds) {
        string name = item.first;
        string desc = name;
        if(calStdDesc.find(name) != calStdDesc.end())
            desc = calStdDesc[name];

        Ui::CalKitSettingsWidget ui1;
        QWidget* w = new QWidget();
        ui1.setupUi(w);
        layout->addWidget(w);

        ui1.l_desc->setText(qs(desc));

        auto it = settings.calKitModels.find(name);
        if(it != settings.calKitModels.end()) {
            ui1.r_s_param->setChecked(true);
            info[name].data = (*it).second;
            info[name].useIdeal = false;
        } else info[name].useIdeal = true;

        connect(ui1.r_ideal, &QRadioButton::clicked, [this, ui1, name](){
            info[name].useIdeal = true;
            ui1.l_status->setText("");
        });

        connect(ui1.r_s_param, &QRadioButton::clicked, [this, ui1, name](){
            QString fileName = QFileDialog::getOpenFileName(this,
                    tr("Open S parameters file"), "",
                    tr("S parameters (*.s1p, *.s2p);;All Files (*)"));
            if (fileName.isEmpty()) goto fail;
            {
                QFile file(fileName);
                if (!file.open(QIODevice::ReadOnly)) {
                    QMessageBox::warning(this, tr("Unable to open file"), file.errorString());
                    goto fail;
                }
                {
                    QTextStream stream(&file);
                    string data = stream.readAll().toStdString();

                    SParamSeries series;
                    int nPorts;
                    try {
                        QFileInfo fileInfo(fileName);
                        parseTouchstone(data,series.startFreqHz, series.stepFreqHz,nPorts,series.values);
                        info[name].useIdeal = false;
                        info[name].data = series;
                        ui1.l_status->setText(fileInfo.fileName());
                    } catch(exception& ex) {
                        QMessageBox::warning(this, tr("Error parsing S parameter file"), ex.what());
                        goto fail;
                    }
                }
            }
            return;
        fail:
            // revert radiobutton state
            ui1.r_ideal->setChecked(info[name].useIdeal);
        });
    }
}

void CalKitSettingsDialog::toSettings(CalKitSettings &settings) {
    settings.calKitModels.clear();
    for(auto& item:idealCalStds) {
        string name = item.first;
        auto it = info.find(name);
        if(it == info.end()) continue;
        if(!(*it).second.useIdeal) {
            settings.calKitModels[name] = (*it).second.data;
        }
    }
}
