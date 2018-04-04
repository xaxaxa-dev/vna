#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <vector>
#include <exception>
#include <functional>
#include <map>
#include <string>
#include <xavna/common.H>
#include "calkitsettings.H"
namespace Ui {
class MainWindow;
}
class QPushButton;
class PolarView;
class ImpedanceDisplay;

namespace QtCharts {
class QChartView;
class QChart;
class QValueAxis;
}
namespace xaxaxa {
class VNADevice;
class VNACalibration;
}
class QTimer;
class MarkerSlider;
class QTextStream;

using namespace QtCharts;
using namespace xaxaxa;
using namespace std;

struct SParamViewSource {
    int row,col;    // which S parameter are we viewing
    enum Types {
        UNDEFINED=0,
        TYPE_MAG=1,     // view must be QLineSeries
        TYPE_PHASE,     // view must be QLineSeries
        TYPE_GRPDELAY,  // view must be QLineSeries
        TYPE_COMPLEX,    // view must be PolarView
        _LAST
    } type;
};

// a UI view of one S parameter vs frequency
struct SParamView {
    SParamViewSource src;
    QObject* view;      // either PolarView or QLineSeries depending on src.type
    QValueAxis* yAxis;  // NULL if view is not QLineSeries

    // these are not applicable for PolarView
    vector<QObject*> markerViews; // a QScatterSeries for each marker if view is a QLineSeries
    function<void(SParamView&)> addMarker;   // this function must add a new object to markerViews
};
struct Marker {
    int freqIndex;
    MarkerSlider* ms;
    bool enabled;
};

struct CalibrationInfo {
    int nPoints;
    double startFreqHz,stepFreqHz;
    int attenuation1,attenuation2;
    string calName;
    map<string, vector<VNARawValue> > measurements;
};
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    const char* calButtonDoneStyle = "background-color: #9999ff";

    VNADevice* vna=NULL;
    ImpedanceDisplay* impdisp=NULL;
    QTimer* timer=NULL;
    PolarView* polarView=NULL;

    // min and max of the y axis of the graph, indexed by SParamViewSource::Types
    vector<array<double,2> > graphLimits;
    // array of values directly from the vna, updated every time a frequency point arrives
    vector<VNARawValue> rawValues;
    // same as rawValues, but with calibration applied (if any)
    vector<VNACalibratedValue> values;
    // time domain values; usually the fourier transform of this->values
    vector<VNACalibratedValue> values_td;
    // array of UI views of the data; each view has a source (describing what data to display) and a widget
    // (where to display the data on
    vector<SParamView> views;
    // array of axis to update when frequency sweep params are changed
    vector<QValueAxis*> xAxis;
    // user-added markers
    vector<Marker> markers;
    // for each reference standard, map from name to the measure button
    map<string, QWidget*> calButtons;
    // for each reference standard, map from name to measured values
    map<string, vector<VNARawValue> > calMeasurements;
    // current active calibration
    const VNACalibration* curCal = NULL;
    // current calibration coefficients
    vector<MatrixXcd> curCalCoeffs;
    // a copy of calMeasurements from when curCal was updated
    map<string, vector<VNARawValue> > curCalMeasurements;
    // the current global cal kit settings
    CalKitSettings cks;

    vector<complex<double> > tmp_s11;
    vector<VNACalibratedValue> tmp_sn1,tmp_sn2;
    QString tmp_fileName;

    QStringList recentFiles;
    int maxRecentFiles=10;
    vector<QAction*> recentFileActions;

    struct {
        bool enabled;
        bool calEnabled;
    } uiState {true, false};

    double freqScale = 1e-6;

    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void loadSettings();

    void populateCalTypes();
    // setup the line graph and smith chart widgets, and populate this->views
    void setupViews();
    // set the callbacks of this->vna
    void setCallbacks();
    // find devices and populate the devices menu
    void populateDevicesMenu();

    // try to use the device dev
    void openDevice(string dev);

    // call after changing frequency sweep parameters of vna
    void updateSweepParams();
    // update the impedance displays
    void updateValueDisplays();
    void updateMarkerViews(int marker=-1);
    void updateBottomLabels(int marker=-1);
    void updateYAxis(int viewIndex=-1);
    void addMarker(bool removable);
    void updateUIState();
    void enableUI(bool enable);

    string serializeCalibration(const CalibrationInfo& cal);
    CalibrationInfo deserializeCalibration(QTextStream &inp);

    void saveFile(QString path, const string& data);
    void saveCalibration(QString path);
    void loadCalibration(QString path);

    void addRecentFile(QString path);
    void refreshRecentFiles();

    void captureSParam(vector<VNACalibratedValue>* var);

    QString fileDialogSave(QString title, QString filter, QString defaultSuffix);


    string freqStr(double freqHz);

protected:

private slots:
    void on_d_caltype_currentIndexChanged(int index);
    void btn_measure_click(QPushButton* btn);
    void on_actionOther_triggered();
    void on_slider_valueChanged(int value);
    void calMeasurementCompleted(string calName);
    void on_b_clear_m_clicked();
    void on_b_apply_clicked();
    void on_b_clear_clicked();
    void on_actionSweep_params_triggered();

    // update a single point on all views in this->views
    void updateViews(int freqIndex);

    // update a single point on a view, or all points on a view (if freqIndex is -1)
    void updateView(int viewIndex, int freqIndex=-1);

    // called when the vna background thread encounters an error
    void handleBackgroundError(QString msg);

    // called after measurement complete when the user clicks "export s1p"
    void s11MeasurementCompleted(QString fileName);
    void sMeasurementCompleted();

    void on_actionLoad_triggered();
    void on_actionSave_triggered();
    void on_actionExport_s1p_triggered();
    void on_actionCapture_S_1_triggered();
    void on_actionCapture_S_2_triggered();
    void on_actionExport_s2p_triggered();
    void on_actionImpedance_pane_toggled(bool arg1);
    void on_actionCalibration_pane_toggled(bool arg1);
    void on_dock_cal_visibilityChanged(bool visible);
    void on_dock_impedance_visibilityChanged(bool visible);
    void on_actionRefresh_triggered();
    void on_actionKit_settings_triggered();

    void on_actionMock_device_triggered();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
