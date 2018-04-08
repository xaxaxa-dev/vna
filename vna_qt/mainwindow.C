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
#include <iostream>

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
        updateMarkerViews();
        updateValueDisplays();
        updateBottomLabels();
    });
    timer->start(200);

    setAttribute(Qt::WA_DeleteOnClose);
    addMarker(false);

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

    graphLimits = {
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
    views.push_back(SParamView{
                        {0,0,
                        SParamViewSource::TYPE_COMPLEX},
                        this->polarView, nullptr,
                        {}, nullptr
                    });

    vector<string> graphTraces;
    vector<SParamViewSource> graphSources;
    for(int i=SParamViewSource::UNDEFINED+1;i<SParamViewSource::_LAST;i++) {
        string name;
        switch(i) {
        case SParamViewSource::TYPE_COMPLEX: continue;
        case SParamViewSource::TYPE_GRPDELAY: name = "GroupDelay"; break;
        case SParamViewSource::TYPE_MAG: name = "mag"; break;
        case SParamViewSource::TYPE_PHASE: name = "arg"; break;
        }
        for(int row=0;row<2;row++)
            for(int col=0;col<2;col++) {
                if(col==1) continue;
                string desc = name + "(S" + to_string(row+1) + to_string(col+1) + ")";
                graphTraces.push_back(desc);
                graphSources.push_back({row, col, SParamViewSource::Types(i)});
            }
    }


    GraphPanel* gp = new GraphPanel();
    gp->populateComboBox(0, graphTraces);
    gp->populateComboBox(1, graphTraces);
    xAxis.push_back(gp->axisX);

    ui->w_graph->layout()->addWidget(gp);

    vector<int> curViews;
    for(int i=0;i<2;i++) {
        // set up markers
        auto fn = [gp, i, this](SParamView& view){
            QScatterSeries* markerSeries = new QScatterSeries();
            gp->chart->addSeries(markerSeries);
            markerSeries->setPointsVisible(true);
            markerSeries->setPen(QPen(QColor(0,200,0),2.));
            markerSeries->setBrush(Qt::transparent);
            markerSeries->setMarkerSize(6.);
            markerSeries->attachAxis(gp->axisX);
            markerSeries->attachAxis(gp->axisY[i]);
            markerSeries->setPointLabelsVisible(true);
            view.markerViews.push_back(markerSeries);
        };
        views.push_back({
                            graphSources[0],
                            gp->series[i], gp->axisY[i],
                            {}, fn
                        });
        curViews.push_back(views.size()-1);
    }
    connect(gp,&GraphPanel::comboBoxSelectionChanged, [this, curViews, graphSources](int index, int sel) {
        views.at(curViews.at(index)).src = graphSources.at(sel);
        updateView(curViews.at(index));
        updateYAxis(curViews.at(index));
        updateMarkerViews();
    });
    gp->comboBox(0)->setCurrentIndex(1);
    gp->comboBox(1)->setCurrentIndex(0);
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
    vector<string> devices = vna->findDevices();
    for(string dev:devices) {
        QAction* action = new QAction(qs("   " + dev));
        connect(action, &QAction::triggered, [this,dev](){
            this->openDevice(dev);
        });
        ui->menuDevice->insertAction(ui->actionRefresh, action);
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

void MainWindow::updateViews(int freqIndex) {
    if(freqIndex >= (int)values.size()) return;
    if(curCal)
        this->values.at(freqIndex) = curCal->computeValue(curCalCoeffs.at(freqIndex), this->rawValues.at(freqIndex));
    else this->values.at(freqIndex) = (VNACalibratedValue) this->rawValues.at(freqIndex);
    for(int i=0;i<(int)this->views.size();i++) {
        updateView(i, freqIndex);
    }
}

void MainWindow::updateView(int viewIndex, int freqIndex) {
    double period = 1./vna->stepFreqHz;
    double grpDelayScale = period/(2*M_PI) * 1e9;

    if(freqIndex < 0) {
        for(int i=0;i<(int)values.size();i++) {
            updateView(viewIndex,i);
        }
        return;
    }
    SParamView tmp = this->views.at(viewIndex);

    VNACalibratedValue val = this->values.at(freqIndex);
    complex<double> entry = val(tmp.src.row,tmp.src.col);

    switch(tmp.src.type) {
    case SParamViewSource::TYPE_MAG:
    case SParamViewSource::TYPE_PHASE:
    case SParamViewSource::TYPE_GRPDELAY:
    {
        auto* series = dynamic_cast<QLineSeries*>(tmp.view);
        double y = 0;
        switch(tmp.src.type) {
        case SParamViewSource::TYPE_MAG:
            y = dB(norm(entry));
            break;
        case SParamViewSource::TYPE_PHASE:
            y = arg(entry)*180./M_PI;
            break;
        case SParamViewSource::TYPE_GRPDELAY:
        {
            if(freqIndex>0) {
                VNACalibratedValue prevVal = this->values.at(freqIndex-1);
                complex<double> prevEntry = prevVal(tmp.src.row,tmp.src.col);
                y = arg(prevEntry) - arg(entry);
                if(y>=M_PI) y-=2*M_PI;
                if(y<-M_PI) y+=2*M_PI;
            } else y=0;
            y *= grpDelayScale;
            break;
        }
        default: assert(false);
        }
        series->replace(freqIndex,series->at(freqIndex).x(), y);
        break;
    }
    case SParamViewSource::TYPE_COMPLEX:
    {
        auto* view = dynamic_cast<PolarView*>(tmp.view);
        view->points.at(freqIndex) = entry;
        break;
    }
    default: assert(false);
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
    graphLimits[SParamViewSource::TYPE_GRPDELAY] = {0., maxGrpDelayNs};
    updateYAxis();
    rawValues.resize(vna->nPoints);
    values.resize(vna->nPoints);
    for(int i=0;i<(int)this->views.size();i++) {
        SParamView tmp = this->views[i];
        switch(tmp.src.type) {
        case SParamViewSource::TYPE_MAG:
        case SParamViewSource::TYPE_PHASE:
        case SParamViewSource::TYPE_GRPDELAY:
        {
            auto* series = dynamic_cast<QLineSeries*>(tmp.view);
            series->clear();
            for(int j=0;j<vna->nPoints;j++) {
                series->append(vna->freqAt(j)*freqScale, 0);
            }
            break;
        }
        case SParamViewSource::TYPE_COMPLEX:
        {
            auto* view = dynamic_cast<PolarView*>(tmp.view);
            view->points.resize(vna->nPoints);
            break;
        }
        default: assert(false);
        }
    }
    for(Marker& marker:markers) {
        if(marker.ms != NULL)
            marker.ms->ui->slider->setRange(0, vna->nPoints-1);
    }
    for(QValueAxis* axisX:xAxis) {
        axisX->setMin(vna->startFreqHz*freqScale);
        axisX->setMax(vna->freqAt(vna->nPoints-1)*freqScale);
    }
}

void MainWindow::updateValueDisplays() {
    int freqIndex = markers.at(0).freqIndex;
    if(curCal) impdisp->setValue(values.at(freqIndex)(0,0), vna->freqAt(freqIndex));
    else impdisp->clearValue();
}

void MainWindow::updateMarkerViews(int marker) {
    for(SParamView view:views) {
        if(auto* series = dynamic_cast<QLineSeries*>(view.view)) {
            for(int i=0;i<(int)markers.size();i++) {
                if(marker>=0 && marker!=i) continue;
                auto* ss = dynamic_cast<QScatterSeries*>(view.markerViews.at(i));
                int freqIndex = markers[i].freqIndex;
                if(markers[i].ms == NULL || !markers[i].enabled) {
                    ss->replace(0, -100, 0);
                    continue;
                }
                ss->replace(0,series->at(freqIndex));
                //printf("sss %f %f\n",series->at(freqIndex).x(), series->at(freqIndex).y());
                fflush(stdout);
            }
        }
        if(auto* pv = dynamic_cast<PolarView*>(view.view)) {
            for(int i=0;i<(int)markers.size();i++) {
                if(marker>=0 && marker!=i) continue;
                if(markers[i].ms == NULL || !markers[i].enabled)
                    pv->markers.at(i).index = -1;
                else pv->markers.at(i).index = markers[i].freqIndex;
            }
            pv->repaint();
        }
    }
}

void MainWindow::updateBottomLabels(int marker) {
    SParamView* lineViews[4];
    int lineViewCount=0;
    for(auto& view:views) {
        if(dynamic_cast<QLineSeries*>(view.view)) {
            lineViews[lineViewCount] = &view;
            lineViewCount++;
            if(lineViewCount>=4) break;
        }
    }
    for(int i=0;i<(int)markers.size();i++) {
        if(marker>=0 && marker!=i) continue;
        auto& marker = markers[i];
        if(marker.ms == NULL) continue;
        for(int j=0;j<4;j++) {
            if(j>=lineViewCount) marker.ms->setLabelText(j, "");
            else {
                auto* series = dynamic_cast<QLineSeries*>(lineViews[j]->view);
                const char* unit = "";
                switch(lineViews[j]->src.type) {
                case SParamViewSource::TYPE_MAG: unit = "dB"; break;
                case SParamViewSource::TYPE_PHASE: unit = "°"; break;
                case SParamViewSource::TYPE_GRPDELAY: unit = "ns"; break;
                default: unit = "";
                }
                marker.ms->setLabelText(j, ssprintf(32, "%.2f %s", series->at(marker.freqIndex).y(), unit));
            }
        }
    }
}

void MainWindow::updateYAxis(int viewIndex) {
    if(viewIndex<0) {
        for(int i=0;i<(int)views.size();i++) updateYAxis(i);
        return;
    }
    auto& view = views.at(viewIndex);
    int typeIndex = view.src.type;
    if(view.yAxis == nullptr) return;
    view.yAxis->setRange(graphLimits.at(typeIndex)[0], graphLimits.at(typeIndex)[1]);
}

void MainWindow::addMarker(bool removable) {
    int newId=-1;
    for(int i=0;i<(int)markers.size();i++)
        if(markers[i].ms == NULL) {
            newId=i;
            break;
        }
    if(newId<0) {
        newId = (int)markers.size();
        markers.push_back({0, NULL, true});
        for(SParamView& view:views) {
            if(dynamic_cast<QLineSeries*>(view.view)) {
                view.addMarker(view);
                auto* series = dynamic_cast<QScatterSeries*>(view.markerViews.at(view.markerViews.size()-1));
                series->append({-100, 0});
                series->setPointLabelsFormat(qs(to_string(newId)));
            }
            if(auto* pv = dynamic_cast<PolarView*>(view.view)) {
                pv->markers.push_back({0xff0000,0});
            }
        }
    }
    MarkerSlider* ms = new MarkerSlider();
    ms->id = newId;
    ms->ui->l_id->setText(qs(to_string(newId)));
    ms->ui->slider->setRange(0, vna->nPoints-1);
    markers[newId].ms = ms;
    markers[newId].freqIndex = 0;
    updateMarkerViews(newId);

    connect(ms->ui->slider,&QSlider::valueChanged, [this,ms,newId](int value) {
        this->markers[newId].freqIndex = value;
        ms->ui->b_freq->setText(qs(freqStr(vna->freqAt(value)) + " MHz"));
        updateMarkerViews(newId);
        if(newId==0) updateValueDisplays();
        updateBottomLabels(newId);
    });
    connect(ms->ui->b_freq,&QPushButton::toggled, [this,newId](bool checked){
        markers.at(newId).enabled = checked;
        updateMarkerViews(newId);
    });
    ms->ui->slider->valueChanged(0);
    if(removable) {
        ms->ui->b_add_del->setText("⨯");
        connect(ms->ui->b_add_del, &QPushButton::clicked, [this,ms,newId](){
            this->markers[newId].ms = NULL;
            updateMarkerViews(newId);
            ms->deleteLater();
        });
    } else {
        ms->ui->b_add_del->setText("+");
        connect(ms->ui->b_add_del, &QPushButton::clicked, [this](){
            addMarker(true);
        });
    }
    auto* layout = dynamic_cast<QBoxLayout*>(ui->w_bottom->layout());
    layout->insertWidget(0, ms);
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
                           curCal->name(),curCalMeasurements};
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


void MainWindow::on_slider_valueChanged(int value) {
    markers[0].freqIndex = value;
    //ui->b_freq->setText(qs(ssprintf(32,"%.2f", vna->freqAt(value)/1e6)));
    updateMarkerViews();
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
            assert(idealCalStds.find(name) != idealCalStds.end());
            auto tmp = idealCalStds[name];
            for(int j=0;j<(int)measurements.size();j++)
                calStdModels[j][i] = tmp;
        } else {
            auto tmp = cks.calKitModels[name];
            for(int j=0;j<(int)measurements.size();j++)
                calStdModels[j][i] = tmp.interpolate(vna->freqAt(j));
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
    if(tmp_sn1.size()!=vna->nPoints) {
        QMessageBox::critical(this,"Error","S*1 has not been captured");
        return;
    }
    if(tmp_sn2.size()!=vna->nPoints) {
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
