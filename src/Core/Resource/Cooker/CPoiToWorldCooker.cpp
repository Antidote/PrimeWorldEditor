#include "CPoiToWorldCooker.h"

bool CPoiToWorldCooker::CookEGMC(CPoiToWorld *pPoiToWorld, IOutputStream& rOut)
{
    // Create mappings list
    struct SPoiMapping
    {
        u32 MeshID;
        u32 PoiID;
    };
    std::vector<SPoiMapping> Mappings;

    for (u32 iPoi = 0; iPoi < pPoiToWorld->NumMappedPOIs(); iPoi++)
    {
        const CPoiToWorld::SPoiMap *pkMap = pPoiToWorld->MapByIndex(iPoi);

        for (auto it = pkMap->ModelIDs.begin(); it != pkMap->ModelIDs.end(); it++)
        {
            SPoiMapping Mapping;
            Mapping.MeshID = *it;
            Mapping.PoiID = pkMap->PoiID;
            Mappings.push_back(Mapping);
        }
    }

    // Write EGMC
    rOut.WriteLong(Mappings.size());

    for (u32 iMap = 0; iMap < Mappings.size(); iMap++)
    {
        rOut.WriteLong(Mappings[iMap].MeshID);
        rOut.WriteLong(Mappings[iMap].PoiID);
    }

    return true;
}
