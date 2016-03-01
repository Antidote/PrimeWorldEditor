#ifndef CSTATEMESSAGEMODEL_H
#define CSTATEMESSAGEMODEL_H

#include "Editor/UICommon.h"
#include <Core/Resource/Script/CMasterTemplate.h>
#include <Core/Resource/Script/CScriptTemplate.h>
#include <QAbstractListModel>

// todo: support pulling states/messages from script templates instead of master
class CStateMessageModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum EType
    {
        eStates,
        eMessages
    };

private:
    struct SEntry
    {
        u32 ID;
        QString Name;

        SEntry() {}
        SEntry(u32 _ID, const QString& rkName)
            : ID(_ID), Name(rkName) {}

        bool operator<(const SEntry& rkOther) const
        {
            return Name < rkOther.Name;
        }
    };
    QList<SEntry> mEntries;

    CMasterTemplate *mpMaster;
    CScriptTemplate *mpScript;
    EType mType;

public:
    explicit CStateMessageModel(EType Type, QObject *pParent = 0)
        : QAbstractListModel(pParent)
        , mType(Type)
        , mpMaster(nullptr)
        , mpScript(nullptr)
    {}

    int rowCount(const QModelIndex& /*rkParent*/) const
    {
        return mEntries.size();
    }

    QVariant data(const QModelIndex& rkIndex, int Role) const
    {
        if (Role == Qt::DisplayRole)
        {
            return mEntries[rkIndex.row()].Name;
        }

        else return QVariant::Invalid;
    }

    void SetMasterTemplate(CMasterTemplate *pMaster)
    {
        beginResetModel();

        mpMaster = pMaster;
        mEntries.clear();

        if (mType == eStates)
        {
            for (u32 iState = 0; iState < pMaster->NumStates(); iState++)
            {
                SState State = pMaster->StateByIndex(iState);
                mEntries << SEntry(State.ID, TO_QSTRING(State.Name));
            }
        }

        else
        {
            for (u32 iMsg = 0; iMsg < pMaster->NumMessages(); iMsg++)
            {
                SMessage Message = pMaster->MessageByIndex(iMsg);
                mEntries << SEntry(Message.ID, TO_QSTRING(Message.Name));
            }
        }

        qSort(mEntries);
        endResetModel();
    }

    void SetScriptTemplate(CScriptTemplate *pScript)
    {
        mpScript = pScript;
    }

    inline u32 State(u32 Index) const
    {
        return (mType == eStates ? mEntries[Index].ID : 0);
    }

    inline u32 Message(u32 Index) const
    {
        return (mType == eMessages ? mEntries[Index].ID : 0);
    }
};

#endif // CSTATEMESSAGEMODEL_H
