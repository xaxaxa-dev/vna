#include "mainwindow.H"
#include "ui_mainwindow.h"
#include "polarview.H"
#include <xavna/calibration.H>
#include <xavna/xavna_cpp.H>
#include <iostream>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QString>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>

using namespace std;
using namespace QtCharts;

inline double dB(double power) {
    return log10(power)*10.;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    vna = new VNADevice();
    polarView = new PolarView();
    chart = new QChart();
    chartView = new QChartView();
    timer = new QTimer();

    setCallbacks();
    setupViews();
    updateSweepParams();


    ui->d_caltype->clear();
    for(const VNACalibration* cal:calibrationTypes) {
        ui->d_caltype->addItem(QString::fromStdString(cal->name()));
    }


    ui->w_polar->layout()->addWidget(polarView);
    ui->w_graph->layout()->addWidget(chartView);
    chartView->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    chartView->setMinimumWidth(100);
    chartView->setBaseSize(100,100);
    chartView->setChart(chart);

    ui->dock_bottom->setTitleBarWidget(new QWidget());

    connect(timer, &QTimer::timeout, [this](){
        this->polarView->repaint();
        this->chartView->update();
    });
    timer->start(200);

    setAttribute(Qt::WA_DeleteOnClose);
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

void MainWindow::setupViews() {
    views.push_back({
                        0,0,
                        SParamView::TYPE_COMPLEX,
                        this->polarView
                    });

    QLineSeries* series = new QLineSeries();

    views.push_back({
                        1,0,
                        SParamView::TYPE_MAG,
                        series
                    });
    chart->addSeries(series);
    chart->legend()->hide();

    QValueAxis *axisX = new QValueAxis;
    axisX->setTickCount(10);
    axisX->setMin(vna->startFreqHz);
    axisX->setMax(vna->freqAt(vna->nPoints));
    chart->addAxis(axisX, Qt::AlignBottom);

    QValueAxis *axisY = new QValueAxis;
    axisY->setLinePenColor(series->pen().color());
    axisY->setMin(-50);
    axisY->setMax(30);
    chart->addAxis(axisY, Qt::AlignLeft);

    series->attachAxis(axisX);
    series->attachAxis(axisY);
}

void MainWindow::setCallbacks() {
    auto updateViews = [this](int freqIndex, VNACalibratedValue val) {

        for(int i=0;i<(int)this->views.size();i++) {
            SParamView tmp = this->views[i];
            complex<double> entry = val(tmp.row,tmp.col);
            switch(tmp.type) {
            case SParamView::TYPE_MAG:
            case SParamView::TYPE_PHASE:
            {
                auto* series = dynamic_cast<QLineSeries*>(tmp.view);
                double y = tmp.type==SParamView::TYPE_MAG?dB(norm(entry)):arg(entry);
                series->replace(freqIndex,vna->freqAt(freqIndex),y);
                break;
            }
            case SParamView::TYPE_COMPLEX:
            {
                auto* view = dynamic_cast<PolarView*>(tmp.view);
                view->points[freqIndex] = entry;
                break;
            }
            }
        }
    };
    vna->frequencyCompletedCallback = [this,updateViews](int freqIndex, VNARawValue val) {
        //printf("frequencyCompletedCallback: %d\n",freqIndex);
        //fflush(stdout);
        updateViews(freqIndex, (VNACalibratedValue)val);
    };
    vna->sweepCompletedCallback = [this](const vector<VNARawValue>& vals) {

    };
    vna->backgroundErrorCallback = [this](const exception& exc) {
        fprintf(stderr,"background thread error: %s\n",exc.what());
    };
}

void MainWindow::updateSweepParams() {
    for(int i=0;i<(int)this->views.size();i++) {
        SParamView tmp = this->views[i];
        switch(tmp.type) {
        case SParamView::TYPE_MAG:
        case SParamView::TYPE_PHASE:
        {
            auto* series = dynamic_cast<QLineSeries*>(tmp.view);
            series->clear();
            for(int j=0;j<vna->nPoints;j++) {
                series->append(vna->freqAt(j), 0);
            }
            break;
        }
        case SParamView::TYPE_COMPLEX:
        {
            auto* view = dynamic_cast<PolarView*>(tmp.view);
            view->points.resize(vna->nPoints);
            break;
        }
        }
    }
}

void MainWindow::updateValueDisplays() {

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
    const VNACalibration* cal = calibrationTypes[index];
    for(string calstd:cal->getRequiredStandards()) {
        vector<string> tmp = split(calstd,',');
        QPushButton* btn = new QPushButton(QString::fromStdString(tmp[1]));
        btn->setToolTip(QString::fromStdString(tmp[0]));
        ui->w_calstandards->layout()->addWidget(btn);

        connect(btn,&QPushButton::clicked,[btn,this](bool b) {
            btn_measure_click(btn);
        });
    }
}

void MainWindow::btn_measure_click(QPushButton *btn) {
    cout << btn->toolTip().toStdString() << endl;
}

void MainWindow::on_actionOther_triggered() {
   auto tmp = QInputDialog::getText(this,"Specify device path...","Device").toStdString();
   if(tmp!="") {
       try {
           vna->open(tmp);
           vna->startScan();
       } catch(exception& ex) {
           QMessageBox::critical(this,"Exception",ex.what());
       }
   }
}

