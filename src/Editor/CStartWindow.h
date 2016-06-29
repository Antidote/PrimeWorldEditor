#ifndef PWESTARTWINDOW_H
#define PWESTARTWINDOW_H

#include "WorldEditor/CWorldEditor.h"
#include "ModelEditor/CModelEditorWindow.h"
#include "CharacterEditor/CCharacterEditor.h"
#include <Core/GameProject/CResourceStore.h>
#include <Core/Resource/CWorld.h>

#include <QMainWindow>

namespace Ui {
class CStartWindow;
}

class CStartWindow : public QMainWindow
{
    Q_OBJECT
    Ui::CStartWindow *ui;

    TResPtr<CWorld> mpWorld;
    u32 mSelectedAreaIndex;

    CWorldEditor *mpWorldEditor;
    CModelEditorWindow *mpModelEditor;
    CCharacterEditor *mpCharEditor;

public:
    explicit CStartWindow(QWidget *pParent = 0);
    ~CStartWindow();
    void closeEvent(QCloseEvent *pEvent);

private slots:
    void on_actionOpen_MLVL_triggered();
    void on_AreaSelectComboBox_currentIndexChanged(int Index);
    void on_AttachedAreasList_doubleClicked(const QModelIndex& rkIndex);
    void on_LaunchWorldEditorButton_clicked();
    void on_actionLaunch_model_viewer_triggered();
    void on_actionExtract_PAK_triggered();

    void LaunchCharacterEditor();
    void About();
    void ExportGame();

private:
    void FillWorldUI();
    void FillAreaUI();
};

#endif // PWESTARTWINDOW_H
