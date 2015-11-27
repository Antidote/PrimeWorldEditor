#include "CScriptExtra.h"

#include "CWaypointExtra.h"
#include "CSpacePirateExtra.h"
#include "CPointOfInterestExtra.h"

CScriptExtra* CScriptExtra::CreateExtra(CScriptNode *pNode)
{
    CScriptExtra *pExtra = nullptr;
    CScriptObject *pObj = pNode->Object();

    if (pObj)
    {
        switch (pObj->ObjectTypeID())
        {
        case 0x02:       // Waypoint (MP1)
        case 0x0D:       // CameraWaypoint (MP1)
        case 0x2C:       // SpiderBallWaypoint (MP1)
        case 0x32:       // DebugCameraWaypoint(MP1)
        case 0x41495750: // "AIWP" AIWaypoint (MP2/MP3/DKCR)
        case 0x42414C57: // "BALW" SpiderBallWaypoint (MP2/MP3)
        case 0x43414D57: // "CAMW" CameraWaypoint (MP2)
        case 0x57415950: // "WAYP" Waypoint (MP2/MP3/DKCR)
            pExtra = new CWaypointExtra(pObj, pNode->Scene(), pNode);
            break;

        case 0x24: // SpacePirate (MP1)
            pExtra = new CSpacePirateExtra(pObj, pNode->Scene(), pNode);
            break;

        case 0x42:       // PointOfInterest (MP1)
        case 0x504F494E: // "POIN" PointOfInterest (MP2/MP3)
            pExtra = new CPointOfInterestExtra(pObj, pNode->Scene(), pNode);
            break;
        }
    }

    return pExtra;
}
