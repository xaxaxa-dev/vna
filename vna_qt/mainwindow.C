#define _USE_MATH_DEFINES

#include <QMetaType>

#include "mainwindow.H"
#include "ui_mainwindow.h"
#include "polarview.H"
#include "markerslider.H"
#include "ui_markerslider.h"
#include "impedancedisplay.H"
#include "frequencydialog.H"
#include "graphpanel.H"
#include "utility.H"
#include "touchstone.H"
#include "calkitsettingsdialog.H"
#include <xavna/calibration.H>
#include <xavna/xavna_cpp.H>
#include <xavna/xavna_generic.H>
#include <xavna/workarounds.H>
#include <iostream>
#include <stdexcept>

#include <QString>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>
#include <QGraphicsLayout>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QSettings>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QScatterSeries>

using namespace std;
using namespace QtCharts;



MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    vna = new VNADevice();
    polarView = new PolarView();
    impdisp = new ImpedanceDisplay();
    timer = new QTimer();

    nv.init(ui->w_bottom->layout());
    nv.xAxisValueStr = [](double val) {
        return ssprintf(32, "%.2f MHz", val);
    };
    connect(&nv, &NetworkView::markerChanged, [this](int marker, int /*index*/) {
        if(marker == 0) updateValueDisplays();
    });

    connect(&dtf,&DTFWindow::hidden, [this]() {
        ui->actionTime_to_fault->setChecked(false);
    });

    loadSettings();
    setCallbacks();
    setupViews();
    updateSweepParams();
    populateCalTypes();
    populateDevicesMenu();


    ui->w_polar->layout()->addWidget(polarView);

    ui->dock_bottom->setTitleBarWidget(new QWidget());

    ((QBoxLayout*)ui->dock_impedance_contents->layout())->insertWidget(0,impdisp);

    connect(timer, &QTimer::timeout, [this](){
        this->polarView->repaint();
        //this->chartView->update();
        nv.updateMarkerViews();
        updateValueDisplays();
        nv.updateBottomLabels();
    });
    timer->start(200);

    setAttribute(Qt::WA_DeleteOnClose);
    nv.addMarker(false);

    enableUI(false);
}

MainWindow::~MainWindow()
{
    printf("aaaaa\n");
    fflush(stdout);

    vna->stopScan();
    vna->close();

    delete timer;
    //delete chartView;
    //delete polarView;
    delete vna;
    delete ui;
    printf("ccccc\n");
    fflush(stdout);
}

void MainWindow::loadSettings() {
    QSettings settings;
    recentFiles = settings.value("recentFiles").toStringList();
    refreshRecentFiles();
    cks = settings.value("calkits").value<CalKitSettings>();

    nv.graphLimits = {
        {-1000,-999},
        {-80, 30},      //TYPE_MAG=1
        {-180, 180},    //TYPE_PHASE
        {0, 50},        //TYPE_GRPDELAY
        {-1000,-999}    //TYPE_COMPLEX
    };
}

void MainWindow::populateCalTypes() {
    ui->d_caltype->clear();
    for(const VNACalibration* cal:calibrationTypes) {
        ui->d_caltype->addItem(QString::fromStdString(cal->description()));
    }
}

void MainWindow::setupViews() {
    nv.views.push_back(SParamView{
                        {0,0,
                        SParamViewSource::TYPE_COMPLEX},
                        this->polarView, nullptr,
                        {}, nullptr
                    });


    QPushButton* polarView_maximize = new QPushButton();
    polarView_maximize->setIcon(QIcon(":/icons/maximize"));
    polarView_maximize->setFlat(true);
    createTopRightFloat(polarView)->layout()->addWidget(polarView_maximize);
    connect(polarView_maximize, &QPushButton::clicked, [this, polarView_maximize](){
        if(maximizePane(polarView))
            polarView_maximize->setIcon(QIcon(":/icons/unmaximize"));
        else polarView_maximize->setIcon(QIcon(":/icons/maximize"));
    });
    polarView->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    GraphPanel* gp = nv.createGraphView();
    ui->w_graph->layout()->addWidget(gp);
    connect(gp->maximizeButton(), &QPushButton::clicked, [this, gp](){
        if(maximizePane(gp))
            gp->maximizeButton()->setIcon(QIcon(":/icons/unmaximize"));
        else gp->maximizeButton()->setIcon(QIcon(":/icons/maximize"));
    });
}

void MainWindow::setCallbacks() {
    vna->frequencyCompletedCallback = [this](int freqIndex, VNARawValue val) {
        //printf("frequencyCompletedCallback: %d\n",freqIndex);
        //fflush(stdout);
        this->rawValues[freqIndex] = val;
        QMetaObject::invokeMethod(this, "updateViews", Qt::QueuedConnection, Q_ARG(int, freqIndex));
    };
    vna->sweepCompletedCallback = [this](const vector<VNARawValue>&) {

    };
    vna->backgroundErrorCallback = [this](const exception& exc) {
        fprintf(stderr,"background thread error: %s\n",exc.what());
        QString msg = exc.what();
        QMetaObject::invokeMethod(this, "handleBackgroundError", Qt::QueuedConnection, Q_ARG(QString, msg));
    };
}

void MainWindow::populateDevicesMenu() {
    bool remove = false;
    for(QAction* act: ui->menuDevice->actions()) {
        if(act == ui->actionRefresh) break;
        if(remove) ui->menuDevice->removeAction(act);
        if(act == ui->actionSelect_device) remove = true;
    }

    vector<string> devices;
    try {
        devices = vna->findDevices();
        for(string dev:devices) {
            QAction* action = new QAction(qs("   " + dev));
            connect(action, &QAction::triggered, [this,dev](){
                this->openDevice(dev);
            });
            ui->menuDevice->insertAction(ui->actionRefresh, action);
        }
    } catch(exception& ex) {
        cerr << ex.what() << endl;
    }
    if(devices.empty()) {
        QAction* action = new QAction("   No devices found; check dmesg or device manager");
        action->setEnabled(false);
        ui->menuDevice->insertAction(ui->actionRefresh, action);
    }
}

void MainWindow::openDevice(string dev) {
    try {
        vna->open(dev);
        vna->startScan();
        enableUI(true);
    } catch(exception& ex) {
        QMessageBox::critical(this,"Exception",ex.what());
    }
}


void MainWindow::handleBackgroundError(QString msg) {
    vna->close();
    QMessageBox::critical(this, "Error", msg);
    enableUI(false);
}

void MainWindow::s11MeasurementCompleted(QString fileName) {
    string data = serializeTouchstone(tmp_s11,vna->startFreqHz,vna->stepFreqHz);
    saveFile(fileName, data);
    enableUI(true);
}

void MainWindow::sMeasurementCompleted() {
    enableUI(true);
}

void MainWindow::updateSweepParams() {
    double maxGrpDelayNs = (1./vna->stepFreqHz)*.5*1e9;
    nv.graphLimits[SParamViewSource::TYPE_GRPDELAY] = {0., maxGrpDelayNs};
    nv.updateYAxis();
    rawValues.resize(vna->nPoints);
    nv.updateXAxis(vna->startFreqHz*freqScale, vna->stepFreqHz*freqScale, vna->nPoints);
    dtf.updateSweepParams(vna->stepFreqHz, vna->nPoints);
}

void MainWindow::updateValueDisplays() {
    int freqIndex = nv.markers.at(0).freqIndex;
    if(curCal) impdisp->setValue(nv.values.at(freqIndex)(0,0), vna->freqAt(freqIndex));
    else impdisp->clearValue();
}


void MainWindow::updateUIState() {
    bool enable = uiState.enabled;
    ui->dock_bottom->setEnabled(enable);
    ui->dock_cal->setEnabled(enable);
    ui->dock_impedance->setEnabled(enable);
    ui->centralWidget->setEnabled(enable);
    //ui->menuCalibration->setEnabled(enable);
    ui->menuS_parameters->setEnabled(enable && uiState.calEnabled);
}

void MainWindow::enableUI(bool enable) {
    uiState.enabled = enable;
    uiState.calEnabled = (curCal != nullptr);
    updateUIState();
}

bool MainWindow::maximizePane(QWidget *w) {
    QWidget* initialMaximizedPane = maximizedPane;
    if(maximizedPane == nullptr) {
        // we are going from normal view to maximized view
        unmaximizedLayoutState = this->saveState();
        for(QDockWidget* dock:findChildren<QDockWidget*>()) {
            if(dock != ui->dock_bottom)
                dock->hide();
        }
    } else {
        this->centralWidget()->layout()->removeWidget(maximizedPane);
        auto* layout = dynamic_cast<QBoxLayout*>(maximizedPaneParent->layout());
        layout->insertWidget(maximizedPaneIndex, maximizedPane);
        maximizedPane = nullptr;
        maximizedPaneParent = nullptr;
    }
    if(initialMaximizedPane == w) {
        // we are going from maximized view to normal view
        ui->w_content->show();
        this->restoreState(unmaximizedLayoutState);
        return false;
    }
    auto* layout = dynamic_cast<QBoxLayout*>(w->parentWidget()->layout());
    maximizedPaneIndex = layout->indexOf(w);
    maximizedPane = w;
    maximizedPaneParent = w->parentWidget();
    w->parentWidget()->layout()->removeWidget(w);
    this->centralWidget()->layout()->addWidget(w);
    ui->w_content->hide();
    return true;
}

QWidget *MainWindow::createTopRightFloat(QWidget *w) {
    QBoxLayout* layout = new QBoxLayout(QBoxLayout::TopToBottom);
    QBoxLayout* layout2 = new QBoxLayout(QBoxLayout::LeftToRight);

    QWidget* widget2 = new QWidget();
    layout2->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding));
    layout2->setMargin(0);
    widget2->setLayout(layout2);
    widget2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    layout->addWidget(widget2);
    layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding));
    w->setLayout(layout);
    layout->setMargin(4);
    return widget2;
}

static string calFileVer = "calFileVersion 1";
string MainWindow::serializeCalibration(const CalibrationInfo &cal) {
    string tmp;
    tmp.append(calFileVer);
    tmp += '\n';
    tmp.append(cal.calName + "\n");
    tmp.append(to_string(cal.nPoints)+" "+to_string(cal.startFreqHz)+" "+to_string(cal.stepFreqHz));
    tmp.append(" "+to_string(cal.attenuation1)+" "+to_string(cal.attenuation2));
    tmp += '\n';
    for(auto& calstd:cal.measurements) {
        tmp.append(calstd.first);
        tmp += '\n';
        for(const VNARawValue& val:calstd.second) {
            int sz=val.rows()*val.cols();
            for(int i=0;i<sz;i++) {
                tmp.append(to_string(val(i).real()) + " ");
                tmp.append(to_string(val(i).imag()));
                tmp += ' ';
            }
            tmp += '\n';
        }
    }
    return tmp;
}

CalibrationInfo MainWindow::deserializeCalibration(QTextStream &inp) {
    CalibrationInfo ret;
    string versionStr = inp.readLine().toStdString();
    if(versionStr != calFileVer) {
        throw runtime_error("Unsupported file version: "+versionStr+". Should be: "+calFileVer);
    }
    ret.calName = inp.readLine().toStdString();
    inp >> ret.nPoints;
    inp >> ret.startFreqHz;
    inp >> ret.stepFreqHz;
    inp >> ret.attenuation1;
    inp >> ret.attenuation2;
    inp.readLine();

    QString calstd;
    while((calstd = inp.readLine())!=nullptr) {
        vector<VNARawValue> values;
        for(int i=0;i<ret.nPoints;i++) {
            VNARawValue value;
            for(int j=0;j<4;j++) {
                double re,im;
                inp >> re;
                inp >> im;
                value(j) = complex<double>(re,im);
            }
            values.push_back(value);
        }
        ret.measurements[calstd.toStdString()] = values;
        fprintf(stderr, "found cal standard %s\n", calstd.toUtf8().data());
        fflush(stderr);
    }
    return ret;
}

void MainWindow::saveFile(QString path, const string &data) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::information(this, tr("Unable to open file"), file.errorString());
        return;
    }
    file.write(data.data(), data.size());
}

void MainWindow::saveCalibration(QString path) {
    CalibrationInfo cal = {vna->nPoints,vna->startFreqHz,vna->stepFreqHz,
                           vna->attenuation1,vna->attenuation2,
                           curCal->name(),curCalMeasurements,
                           cks};
    saveFile(path, serializeCalibration(cal));
    addRecentFile(path);
}

void MainWindow::loadCalibration(QString path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::information(this, tr("Unable to open file"), file.errorString());
        return;
    }
    QTextStream stream(&file);
    CalibrationInfo calInfo = deserializeCalibration(stream);

    const VNACalibration* cal=nullptr;
    int i=0;
    for(auto* c:calibrationTypes) {
        if(c->name() == calInfo.calName) {
            cal = c;
            ui->d_caltype->setCurrentIndex(i);
            break;
        }
        i++;
    }
    if(!cal) {
        string errmsg = "The calibration file uses calibration type \""+calInfo.calName+"\" which is not supported";
        QMessageBox::critical(this,"Error",qs(errmsg));
        return;
    }
    bool scan=vna->_threadRunning;
    vna->stopScan();

    vna->nPoints = calInfo.nPoints;
    vna->startFreqHz = calInfo.startFreqHz;
    vna->stepFreqHz = calInfo.stepFreqHz;
    vna->attenuation1 = calInfo.attenuation1;
    vna->attenuation2 = calInfo.attenuation2;

    updateSweepParams();
    calMeasurements = calInfo.measurements;
    this->on_d_caltype_currentIndexChanged(ui->d_caltype->currentIndex());
    this->on_b_apply_clicked();

    if(scan) vna->startScan();
    addRecentFile(path);
}

void MainWindow::addRecentFile(QString path) {
    recentFiles.insert(0,path);
    recentFiles.removeDuplicates();
    refreshRecentFiles();
    QSettings settings;
    settings.setValue("recentFiles", recentFiles);
}

void MainWindow::refreshRecentFiles() {
    for(auto* act:recentFileActions) {
        ui->menuCalibration->removeAction(act);
    }
    recentFileActions.clear();
    for(QString entry:recentFiles) {
        auto* act = ui->menuCalibration->addAction(entry, [entry,this](){
            loadCalibration(entry);
        });
        recentFileActions.push_back(act);
    }
}

void MainWindow::captureSParam(vector<VNACalibratedValue> *var) {
    enableUI(false);
    var->resize(vna->nPoints);
    vna->takeMeasurement([this,var](const vector<VNARawValue>& vals){
        assert(curCal != nullptr);
        for(int i=0;i<vna->nPoints;i++)
            var->at(i) = curCal->computeValue(curCalCoeffs.at(i),vals.at(i));
        QMetaObject::invokeMethod(this, "sMeasurementCompleted", Qt::QueuedConnection);
    });
}

QString MainWindow::fileDialogSave(QString title, QString filter, QString defaultSuffix) {
    QFileDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix(defaultSuffix);
    dialog.setNameFilter(filter);
    if(dialog.exec() == QDialog::Accepted)
        return dialog.selectedFiles().at(0);
    return nullptr;
}

string MainWindow::freqStr(double freqHz) {
    return ssprintf(32, "%.2f", freqHz*freqScale);
}

void MainWindow::hideEvent(QHideEvent *) {
    // hack required because qt seems to close all dock widgets when
    // the main window is minimized
    // TODO(xaxaxa): remove this when qt bug is fixed
    layoutState = this->saveState();
}

void MainWindow::showEvent(QShowEvent *) {
    this->restoreState(layoutState);
}


void MainWindow::on_d_caltype_currentIndexChanged(int index) {
    QLayoutItem *child;
    while ((child = ui->w_calstandards->layout()->takeAt(0)) != 0) {
        delete child->widget();
        delete child;
    }
    if(index<0 || index>=(int)calibrationTypes.size()) return;
    calButtons.clear();
    const VNACalibration* cal = calibrationTypes[index];
    for(auto calstd:cal->getRequiredStandards()) {
        string name = calstd[0];
        string desc = calstd[1];

        QPushButton* btn = new QPushButton(QString::fromStdString(desc));
        btn->setToolTip(QString::fromStdString(name));

        if(calMeasurements[name].size() != 0)
            btn->setStyleSheet(calButtonDoneStyle);
        ui->w_calstandards->layout()->addWidget(btn);
        calButtons[name] = btn;

        connect(btn,&QPushButton::clicked,[btn,this]() {
            btn_measure_click(btn);
        });
    }
}

void MainWindow::btn_measure_click(QPushButton *btn) {
    string name = btn->toolTip().toStdString();
    cout << name << endl;
    ui->dock_cal_contents->setEnabled(false);
    vna->takeMeasurement([this,name](const vector<VNARawValue>& vals){
        calMeasurements[name] = vals;
        QMetaObject::invokeMethod(this, "calMeasurementCompleted", Qt::QueuedConnection, Q_ARG(string, name));
    });
}

void MainWindow::on_actionOther_triggered() {
   auto tmp = QInputDialog::getText(this,"Specify device path...","Device (/dev/tty... on linux/mac, COM... on windows)").toStdString();
   if(tmp != "") openDevice(tmp);
}

void MainWindow::calMeasurementCompleted(string calName) {
    ui->dock_cal_contents->setEnabled(true);
    calButtons[calName]->setStyleSheet(calButtonDoneStyle);
}

void MainWindow::on_b_clear_m_clicked() {
    calMeasurements.clear();
    for(auto& tmp:calButtons) {
        tmp.second->setStyleSheet("");
    }
}

void MainWindow::on_b_apply_clicked() {
    int index = ui->d_caltype->currentIndex();
    auto* cal = calibrationTypes[index];

    auto calStds = cal->getRequiredStandards();
    vector<string> names;
    for(auto tmp:calStds) {
        names.push_back(tmp[0]);
    }
    // indexed by frequency, calStdType
    vector<vector<VNARawValue> > measurements(vna->nPoints);
    vector<vector<VNACalibratedValue> > calStdModels(vna->nPoints);

    for(int i=0;i<(int)names.size();i++) {
        if(int(calMeasurements[names[i]].size()) != vna->nPoints) {
            QMessageBox::critical(this,"Error",qs("measurement for \"" + names[i] + "\" does not exist"));
            return;
        }
    }

    // populate measurements and calStdModels
    for(int j=0;j<(int)measurements.size();j++) {
        measurements[j].resize(names.size());
        calStdModels[j].resize(names.size());
        for(int i=0;i<(int)names.size();i++) {
            measurements[j][i] = calMeasurements[names[i]].at(j);
        }
    }
    // populate calStdModels
    for(int i=0;i<(int)names.size();i++) {
        string name = names[i];
        // if the cal standard not set in settings, use ideal parameters
        if(cks.calKitModels.find(name) == cks.calKitModels.end()) {
            printf("using ideal parameters for %s\n", name.c_str());
            assert(idealCalStds.find(name) != idealCalStds.end());
            auto tmp = idealCalStds[name];
            for(int j=0;j<(int)measurements.size();j++)
                calStdModels[j][i] = tmp;
        } else {
            printf("using user parameters for %s\n", name.c_str());
            auto tmp = cks.calKitModels[name];
            for(int j=0;j<(int)measurements.size();j++) {
                auto sss = tmp.interpolate(vna->freqAt(j));
                printf("%lf dB, %lf deg\n", dB(norm(sss(0,0))), arg(sss(0,0))*180/M_PI);
                calStdModels[j][i] = sss;
            }
        }
    }
    curCal = cal;
    curCalMeasurements = calMeasurements;
    curCalCoeffs.resize(vna->nPoints);
    for(int i=0;i<(int)measurements.size();i++) {
        curCalCoeffs[i] = curCal->computeCoefficients(measurements[i], calStdModels[i]);
    }
    ui->l_cal->setText(qs(curCal->description()));
    uiState.calEnabled = true;
    updateUIState();
}

void MainWindow::on_b_clear_clicked() {
    curCal = NULL;
    curCalCoeffs.clear();
    tmp_sn1.clear();
    tmp_sn2.clear();
    ui->l_cal->setText("None");
    uiState.calEnabled = false;
    updateUIState();
}

void MainWindow::on_actionSweep_params_triggered() {
    FrequencyDialog dialog(this);
    dialog.fromVNA(*vna);
    if(dialog.exec() == QDialog::Accepted) {
        bool running = vna->_threadRunning;
        if(running) vna->stopScan();
        if(dialog.toVNA(*vna)) {
            emit on_b_clear_clicked();
            emit on_b_clear_m_clicked();
        }
        updateSweepParams();
        if(running) vna->startScan();
    }
}

void MainWindow::updateViews(int freqIndex) {
    if(freqIndex >= (int)nv.values.size()) return;
    if(curCal)
        nv.values.at(freqIndex) = curCal->computeValue(curCalCoeffs.at(freqIndex), this->rawValues.at(freqIndex));
    else nv.values.at(freqIndex) = (VNACalibratedValue) this->rawValues.at(freqIndex);
    nv.updateViews(freqIndex);

    time_t t = time(nullptr);
    if(abs(int64_t(t) - int64_t(lastDTFUpdate)) >= 1)
        dtf.updateValues(nv.values);
    lastDTFUpdate = t;
}

void MainWindow::on_actionLoad_triggered() {
    QString fileName = QFileDialog::getOpenFileName(this,
            tr("Open calibration"), "",
            tr("VNA calibration (*.cal);;All Files (*)"));
    if (fileName.isEmpty()) return;
    loadCalibration(fileName);
}

void MainWindow::on_actionSave_triggered() {
    if(curCal == NULL) {
        QMessageBox::critical(this, "Error", "No calibration is currently active");
        return;
    }
    QString fileName = QFileDialog::getSaveFileName(this,
            tr("Save calibration"), "",
            tr("VNA calibration (*.cal);;All Files (*)"));
    if (fileName.isEmpty()) return;
    saveCalibration(fileName);
}

void MainWindow::on_actionExport_s1p_triggered() {
    QString fileName = fileDialogSave(
            tr("Save S parameters"),
            tr("Touchstone .s1p (*.s1p);;All Files (*)"), "s1p");
    if (fileName.isEmpty()) return;

    tmp_s11.resize(vna->nPoints);
    enableUI(false);
    vna->takeMeasurement([this,fileName](const vector<VNARawValue>& vals){
        assert(curCal != nullptr);
        for(int i=0;i<vna->nPoints;i++)
            tmp_s11.at(i) = curCal->computeValue(curCalCoeffs.at(i),vals.at(i))(0,0);
        QMetaObject::invokeMethod(this, "s11MeasurementCompleted", Qt::QueuedConnection, Q_ARG(QString, fileName));
    });
}

void MainWindow::on_actionCapture_S_1_triggered() {
    captureSParam(&tmp_sn1);
}

void MainWindow::on_actionCapture_S_2_triggered() {
    captureSParam(&tmp_sn2);
}

void MainWindow::on_actionExport_s2p_triggered() {
    if(int(tmp_sn1.size())!=vna->nPoints) {
        QMessageBox::critical(this,"Error","S*1 has not been captured");
        return;
    }
    if(int(tmp_sn2.size())!=vna->nPoints) {
        QMessageBox::critical(this,"Error","S*2 has not been captured");
        return;
    }
    vector<VNACalibratedValue> res(vna->nPoints);
    for(int i=0;i<vna->nPoints;i++) {
        res[i] << tmp_sn1[i](0,0), tmp_sn2[i](1,0),
                tmp_sn1[i](1,0), tmp_sn2[i](0,0);
    }
    string data = serializeTouchstone(res,vna->startFreqHz,vna->stepFreqHz);
    QString fileName = fileDialogSave(
            tr("Save S parameters"),
            tr("Touchstone .s2p (*.s2p);;All Files (*)"), "s2p");
    if (fileName.isEmpty()) return;
    saveFile(fileName, data);
}

void MainWindow::on_actionImpedance_pane_toggled(bool arg1) {
    ui->dock_impedance->setVisible(arg1);
}
void MainWindow::on_actionCalibration_pane_toggled(bool arg1) {
    ui->dock_cal->setVisible(arg1);
}
void MainWindow::on_dock_cal_visibilityChanged(bool visible) {
    if(visible != ui->actionCalibration_pane->isChecked())
        ui->actionCalibration_pane->setChecked(visible);
}
void MainWindow::on_dock_impedance_visibilityChanged(bool visible) {
    if(visible != ui->actionImpedance_pane->isChecked())
        ui->actionImpedance_pane->setChecked(visible);
}

void MainWindow::on_actionRefresh_triggered() {
    populateDevicesMenu();
}

void MainWindow::on_actionKit_settings_triggered() {
    CalKitSettingsDialog dialog;
    dialog.fromSettings(cks);
    if(dialog.exec() == QDialog::Accepted) {
        dialog.toSettings(cks);
        QSettings settings;
        QVariant var;
        var.setValue(cks);
        settings.setValue("calkits", var);
    }
}

extern "C" int __init_xavna_mock();
extern map<string, xavna_constructor> xavna_virtual_devices;
void MainWindow::on_actionMock_device_triggered() {
    if(xavna_virtual_devices.find("mock") == xavna_virtual_devices.end())
        __init_xavna_mock();
    openDevice("mock");
}

void MainWindow::on_menuDevice_aboutToShow() {
    populateDevicesMenu();
}

void MainWindow::on_actionTime_to_fault_toggled(bool arg1) {
    if(arg1) dtf.show();
    else dtf.hide();
}
