#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <vector>
#include <exception>

namespace Ui {
class MainWindow;
}
class QPushButton;
class PolarView;

namespace QtCharts {
class QChartView;
class QChart;
}
namespace xaxaxa {
class VNADevice;
}
class QTimer;

using namespace QtCharts;
using namespace xaxaxa;
using namespace std;

// a UI view of one S parameter vs frequency
struct SParamView {
    int row,col;    // which S parameter are we viewing
    enum {
        TYPE_MAG=1,
        TYPE_PHASE,
        TYPE_COMPLEX
    } type;
    QObject* view;
};
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    VNADevice* vna=NULL;
    PolarView* polarView=NULL;
    QChartView* chartView=NULL;
    QChart* chart=NULL;
    QTimer* timer=NULL;
    vector<SParamView> views;


    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void setupViews();
    void setCallbacks();
    void updateSweepParams();
    void updateValueDisplays();

protected:

private slots:
    void on_d_caltype_currentIndexChanged(int index);
    void btn_measure_click(QPushButton* btn);

    void on_actionOther_triggered();


private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
