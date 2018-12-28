#ifndef CTWEAKMANAGER_H
#define CTWEAKMANAGER_H

#include "CTweakData.h"

/** Class responsible for managing game tweak data, including saving/loading and providing access */
class CTweakManager
{
    /** Project */
    CGameProject* mpProject;

    /** All tweak resources in the current game */
    std::vector< TResPtr<CTweakData> > mTweakObjects;

public:
    CTweakManager(CGameProject* pInProject);
    void LoadTweaks();
    void SaveTweaks();

    // Accessors
    inline const std::vector< TResPtr<CTweakData> >& TweakObjects() const
    {
        return mTweakObjects;
    }
};

#endif // CTWEAKMANAGER_H
