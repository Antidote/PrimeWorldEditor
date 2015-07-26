#ifndef CLAYEREDITOR_H
#define CLAYEREDITOR_H

#include <QDialog>
#include "CLayerModel.h"

namespace Ui {
class CLayerEditor;
}

class CLayerEditor : public QDialog
{
    Q_OBJECT
    CGameArea *mpArea;
    CLayerModel *mpModel;
    CScriptLayer *mpCurrentLayer;

public:
    explicit CLayerEditor(QWidget *parent = 0);
    ~CLayerEditor();
    void SetArea(CGameArea *pArea);

public slots:
    void SetCurrentIndex(int index);
    void EditLayerName(const QString& name);
    void EditLayerActive(bool active);

private:
    Ui::CLayerEditor *ui;
};

#endif // CLAYEREDITOR_H
