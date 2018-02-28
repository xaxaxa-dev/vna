#define _USE_MATH_DEFINES
#include "mainwindow.H"
#include "ui_mainwindow.h"
#include "polarview.H"
#include "markerslider.H"
#include "ui_markerslider.h"
#include "impedancedisplay.H"
#include "frequencydialog.H"
#include "graphpanel.H"
#include "utility.H"
#include <xavna/calibration.H>
#include <xavna/xavna_cpp.H>
#include <iostream>

#include <QString>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>
#include <QGraphicsLayout>

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

    setCallbacks();
    setupViews();
    updateSweepParams();
    populateCalTypes();


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

void MainWindow::populateCalTypes() {
    ui->d_caltype->clear();
    for(const VNACalibration* cal:calibrationTypes) {
        ui->d_caltype->addItem(QString::fromStdString(cal->name()));
    }
}

void MainWindow::setupViews() {
    views.push_back(SParamView{
                        {0,0,
                        SParamViewSource::TYPE_COMPLEX},
                        this->polarView,
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
                            gp->series[i], {},
                            fn
                        });
        curViews.push_back(views.size()-1);
    }
    connect(gp,&GraphPanel::comboBoxSelectionChanged, [this, curViews, graphSources](int index, int sel) {
        views.at(curViews.at(index)).src = graphSources.at(sel);
        updateView(curViews.at(index));
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
    };
}

void MainWindow::updateViews(int freqIndex) {
    if(curCal)
        this->values[freqIndex] = curCal->computeValue(curCalCoeffs[freqIndex], this->rawValues[freqIndex]);
    else this->values[freqIndex] = (VNACalibratedValue) this->rawValues[freqIndex];
    for(int i=0;i<(int)this->views.size();i++) {
        updateView(i, freqIndex);
    }
}

void MainWindow::updateView(int viewIndex, int freqIndex) {
    if(freqIndex < 0) {
        for(int i=0;i<(int)values.size();i++) {
            updateView(viewIndex,i);
        }
        return;
    }
    VNACalibratedValue val = this->values.at(freqIndex);
    SParamView tmp = this->views.at(viewIndex);
    complex<double> entry = val(tmp.src.row,tmp.src.col);
    switch(tmp.src.type) {
    case SParamViewSource::TYPE_MAG:
    case SParamViewSource::TYPE_PHASE:
    case SParamViewSource::TYPE_GRPDELAY:
    {
        auto* series = dynamic_cast<QLineSeries*>(tmp.view);
        double y = tmp.src.type==SParamViewSource::TYPE_MAG?dB(norm(entry)):arg(entry);
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

void MainWindow::updateSweepParams() {
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
                printf("sss %f %f\n",series->at(freqIndex).x(), series->at(freqIndex).y());
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

void MainWindow::enableUI(bool enable) {
    ui->dock_bottom->setEnabled(enable);
    ui->dock_cal->setEnabled(enable);
    ui->dock_impedance->setEnabled(enable);
    ui->centralWidget->setEnabled(enable);
    ui->menuCalibration->setEnabled(enable);
    ui->menuS_parameters->setEnabled(false);
}

string MainWindow::freqStr(double freqHz) {
    return ssprintf(32, "%.2f", freqHz*freqScale);
}

template<typename Out>
void split(const std::string &s, char delim, Out result) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        *(result++) = item;
    }
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, std::back_inserter(elems));
    return elems;
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
   if(tmp!="") {
       try {
           vna->open(tmp);
           vna->startScan();
           enableUI(true);
       } catch(exception& ex) {
           QMessageBox::critical(this,"Exception",ex.what());
       }
   }
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

    for(int i=0;i<(int)names.size();i++) {
        if(int(calMeasurements[names[i]].size()) != vna->nPoints) {
            QMessageBox::critical(this,"Error",qs("measurement for \"" + names[i] + "\" does not exist"));
            return;
        }
    }

    for(int j=0;j<(int)measurements.size();j++) {
        measurements[j].resize(names.size());
        for(int i=0;i<(int)names.size();i++)
            measurements[j][i] = calMeasurements[names[i]].at(j);
    }
    curCal = cal;
    curCalCoeffs.resize(vna->nPoints);
    for(int i=0;i<(int)measurements.size();i++) {
        curCalCoeffs[i] = curCal->computeCoefficients(measurements[i]);
    }
    ui->l_cal->setText(qs(curCal->name()));
    ui->menuS_parameters->setEnabled(true);
}

void MainWindow::on_b_clear_clicked() {
    curCal = NULL;
    curCalCoeffs.clear();
    ui->l_cal->setText("None");
    ui->menuS_parameters->setEnabled(false);
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
