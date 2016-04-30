#include "CSplinePathExtra.h"
#include "CWaypointExtra.h"
#include "Core/Resource/Script/CLink.h"
#include "Core/Scene/CScene.h"

CSplinePathExtra::CSplinePathExtra(CScriptObject *pInstance, CScene *pScene, CScriptNode *pParent)
    : CScriptExtra(pInstance, pScene, pParent)
{
    mpPathColor = TPropCast<TColorProperty>(pInstance->Properties()->PropertyByID(0x00DD86E2));
}

void CSplinePathExtra::PropertyModified(IProperty *pProperty)
{
    if (pProperty == mpPathColor)
    {
        for (auto it = mWaypoints.begin(); it != mWaypoints.end(); it++)
            (*it)->CheckColor();
    }
}

void CSplinePathExtra::PostLoad()
{
    AddWaypoints();
}

void CSplinePathExtra::FindAttachedWaypoints(std::set<CWaypointExtra*>& rChecked, CWaypointExtra *pWaypoint)
{
    if (rChecked.find(pWaypoint) != rChecked.end())
        return;

    rChecked.insert(pWaypoint);
    pWaypoint->AddToSplinePath(this);
    mWaypoints.push_back(pWaypoint);

    std::list<CWaypointExtra*> Attached;
    pWaypoint->GetLinkedWaypoints(Attached);

    for (auto it = Attached.begin(); it != Attached.end(); it++)
        FindAttachedWaypoints(rChecked, *it);
}

void CSplinePathExtra::AddWaypoints()
{
    if (mGame != eReturns)
        return;

    std::set<CWaypointExtra*> CheckedWaypoints;

    for (u32 iLink = 0; iLink < mpInstance->NumLinks(eOutgoing); iLink++)
    {
        CLink *pLink = mpInstance->Link(eOutgoing, iLink);

        if ( (pLink->State() == 0x49533030 && pLink->Message() == 0x41544348) || // InternalState00/Attach
             (pLink->State() == 0x4D4F5450 && pLink->Message() == 0x41544348) )  // MotionPath/Attach
        {
            CScriptNode *pNode = mpScene->NodeForInstanceID(pLink->ReceiverID());

            if (pNode && pNode->Instance()->ObjectTypeID() == 0x57415950) // Waypoint
            {
                CWaypointExtra *pWaypoint = static_cast<CWaypointExtra*>(pNode->Extra());
                FindAttachedWaypoints(CheckedWaypoints, pWaypoint);
            }
        }
    }
}

void CSplinePathExtra::RemoveWaypoint(CWaypointExtra *pWaypoint)
{
    for (auto it = mWaypoints.begin(); it != mWaypoints.end(); it++)
    {
        if (*it == pWaypoint)
        {
            mWaypoints.erase(it);
            break;
        }
    }
}

void CSplinePathExtra::ClearWaypoints()
{
    for (auto it = mWaypoints.begin(); it != mWaypoints.end(); it++)
        (*it)->RemoveFromSplinePath(this);

    mWaypoints.clear();
}
