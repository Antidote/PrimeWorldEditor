#ifndef CCHANGELAYERCOMMAND_H
#define CCHANGELAYERCOMMAND_H

#include "IUndoCommand.h"
#include "ObjReferences.h"
#include "Editor/WorldEditor/CWorldEditor.h"
#include <Core/Scene/CScriptNode.h>

class CChangeLayerCommand : public IUndoCommand
{
    CNodePtrList mNodes;
    QMap<u32, CScriptLayer*> mOldLayers;
    CScriptLayer *mpNewLayer;
    CWorldEditor *mpEditor;

public:
    CChangeLayerCommand(CWorldEditor *pEditor, const QList<CScriptNode*>& rkNodeList, CScriptLayer *pNewLayer);
    void undo();
    void redo();
    bool AffectsCleanState() const { return true; }
};

#endif // CCHANGELAYERCOMMAND_H
