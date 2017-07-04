#ifndef IUIRELAY_H
#define IUIRELAY_H

#include <Common/TString.h>

class IUIRelay
{
public:
    virtual bool AskYesNoQuestion(const TString& rkInfoBoxTitle, const TString& rkQuestion) = 0;
};
extern IUIRelay *gpUIRelay;

#endif // IUIRELAY_H
