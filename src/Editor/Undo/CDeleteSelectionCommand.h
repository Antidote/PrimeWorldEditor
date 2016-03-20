#ifndef CDELETESELECTIONCOMMAND_H
#define CDELETESELECTIONCOMMAND_H

#include "CDeleteLinksCommand.h"
#include "IUndoCommand.h"
#include "ObjReferences.h"
#include "Editor/WorldEditor/CWorldEditor.h"
#include <Core/Scene/CSceneNode.h>

// todo: currently only supports deleting script nodes; needs support for light nodes as well
// plus maybe it should be extensible enough to support other possible types
class CDeleteSelectionCommand : public IUndoCommand
{
    CWorldEditor *mpEditor;
    CNodePtrList mOldSelection;
    CNodePtrList mNewSelection;
    CInstancePtrList mLinkedInstances;

    struct SDeletedNode
    {
        // Node Info
        CNodePtr NodePtr;
        u32 NodeID;
        CVector3f Position;
        CQuaternion Rotation;
        CVector3f Scale;

        // Instance Info
        CGameArea *pArea;
        CScriptLayer *pLayer;
        u32 LayerIndex;
        std::vector<char> InstanceData;
    };
    QVector<SDeletedNode> mDeletedNodes;

    struct SDeletedLink
    {
        u32 State;
        u32 Message;
        u32 SenderID;
        u32 SenderIndex;
        u32 ReceiverID;
        u32 ReceiverIndex;
        CInstancePtr pSender;
        CInstancePtr pReceiver;
    };
    QVector<SDeletedLink> mDeletedLinks;

public:
    CDeleteSelectionCommand(CWorldEditor *pEditor, const QString& rkCommandName = "Delete");
    void undo();
    void redo();
    bool AffectsCleanState() const { return true; }
};

#endif // CDELETESELECTIONCOMMAND_H
