#ifndef CEDITLINKCOMMAND_H
#define CEDITLINKCOMMAND_H

#include "IUndoCommand.h"
#include "ObjReferences.h"
#include "Editor/WorldEditor/CWorldEditor.h"
#include <Core/Resource/Script/CLink.h>

class CEditLinkCommand : public IUndoCommand
{
    CWorldEditor *mpEditor;
    CLinkPtr mpEditLink;

    CLink mOldLink;
    CLink mNewLink;
    u32 mOldSenderIndex;
    u32 mOldReceiverIndex;

    CInstancePtrList mAffectedInstances;

public:
    CEditLinkCommand(CWorldEditor *pEditor, CLink *pLink, CLink NewLink);
    QList<CScriptObject*> AffectedInstances() const;
    void undo();
    void redo();
    bool AffectsCleanState() const { return true; }
};

#endif // CEDITLINKCOMMAND_H
