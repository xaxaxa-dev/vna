#ifndef CALKITSETTINGSDIALOG_H
#define CALKITSETTINGSDIALOG_H

#include <QDialog>
#include "calkitsettings.H"
namespace Ui {
class CalKitSettingsDialog;
}

class CalKitSettingsDialog : public QDialog
{
    Q_OBJECT

    struct calKitInfo {
        SParamSeries data;
        bool useIdeal;
    };
public:
    explicit CalKitSettingsDialog(QWidget *parent = 0);
    ~CalKitSettingsDialog();

    void fromSettings(const CalKitSettings& settings);
    void toSettings(CalKitSettings& settings);

    map<string, calKitInfo> info;
private:
    Ui::CalKitSettingsDialog *ui;
};

#endif // CALKITSETTINGSDIALOG_H
