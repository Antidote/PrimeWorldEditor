#include "CPropertyNameGenerator.h"
#include "IUIRelay.h"
#include "Core/Resource/Factory/CTemplateLoader.h"
#include "Core/Resource/Script/CGameTemplate.h"
#include <Common/Hash/CCRC32.h>

/** Default constructor */
CPropertyNameGenerator::CPropertyNameGenerator()
    : mWordListLoadStarted(false)
    , mWordListLoadFinished(false)
    , mIsRunning(false)
    , mFinishedRunning(false)
{
}

void CPropertyNameGenerator::Warmup()
{
    // Clear output from previous runs
    ASSERT(!mWordListLoadStarted || mWordListLoadFinished);
    mWordListLoadFinished = false;
    mWordListLoadStarted = true;
    mWords.clear();

    // Load the word list from the file
    FILE* pListFile = fopen("../resources/WordList.txt", "r");
    ASSERT(pListFile);

    while (!feof(pListFile))
    {
        char WordBuffer[256];
        fgets(&WordBuffer[0], 256, pListFile);

        // Capitalize first letter
        if (WordBuffer[0] >= 'a' && WordBuffer[0] <= 'z')
        {
            WordBuffer[0] -= 0x20;
        }

        SWord Word;
        Word.Word = TString(WordBuffer).Trimmed();
        Word.Usages = 0;
        mWords.push_back(Word);
    }

    fclose(pListFile);
    mWordListLoadFinished = true;
}

void CPropertyNameGenerator::Generate(const SPropertyNameGenerationParameters& rkParams, IProgressNotifier* pProgress)
{
    // Make sure all prerequisite data is loaded!
    ASSERT(!mIsRunning);
    ASSERT(rkParams.TypeNames.size() > 0);
    mGeneratedNames.clear();
    mIsRunning = true;
    mFinishedRunning = false;

    // If we haven't loaded the word list yet, load it.
    // If we are still loading the word list, wait until we're finished.
    if (!mWordListLoadFinished)
    {
        if (mWordListLoadStarted)
            while (!mWordListLoadFinished) {}
        else
            Warmup();
    }

    // Calculate the number of steps involved in this task.
    const int kNumWords = mWords.size();
    const int kMaxWords = rkParams.MaxWords;
    int TestsDone = 0;
    int TotalTests = 1;

    for (int i = 0; i < kMaxWords; i++)
        TotalTests *= kNumWords;

    pProgress->SetOneShotTask("Generating property names");
    pProgress->Report(TestsDone, TotalTests);

    // Configure params needed to run the name generation!
    bool WriteToLog = rkParams.PrintToLog;
    bool SaveResults = true;

    // The prefix only needs to be hashed this one time
    CCRC32 PrefixHash;
    PrefixHash.Hash( *rkParams.Prefix );

    // Use a stack to keep track of the current word we are on. We can use this
    // to cache the hash of a word and then re-use it later instead of recaculating
    // the same hashes over and over. Init the stack with the first word.
    struct SWordCache
    {
        int WordIndex;
        CCRC32 Hash;
    };
    std::vector<SWordCache> WordCache;

    SWordCache FirstWord { -1, CCRC32() };
    WordCache.push_back(FirstWord);

    while ( true )
    {
        // Increment the current word, handle wrapping back to 0, and update cached hashes as needed.
        int RecalcIndex = WordCache.size() - 1;
        WordCache.back().WordIndex++;

        while (WordCache[RecalcIndex].WordIndex >= kNumWords)
        {
            WordCache[RecalcIndex].WordIndex = 0;

            if (RecalcIndex > 0)
            {
                RecalcIndex--;
                WordCache[RecalcIndex].WordIndex++;
            }
            else
            {
                SWordCache NewWord { 0, CCRC32() };
                WordCache.push_back(NewWord);
            }
        }

        // If we've hit the word limit, break out and end the name generation system.
        if (WordCache.size() > kMaxWords)
            break;

        // Now that all words are updated, calculate the new hashes.
        CCRC32 LastValidHash = (RecalcIndex > 0 ? WordCache[RecalcIndex-1].Hash : PrefixHash);

        for (; RecalcIndex < WordCache.size(); RecalcIndex++)
        {
            int Index = WordCache[RecalcIndex].WordIndex;

            // Add an underscore if needed
            if (RecalcIndex > 0 && rkParams.UseUnderscores)
                LastValidHash.Hash("_");

            LastValidHash.Hash( *mWords[Index].Word );
            WordCache[RecalcIndex].Hash = LastValidHash;
        }

        // We got our hash yay! Now hash the suffix and then we can test with each type name
        CCRC32 BaseHash = LastValidHash;
        BaseHash.Hash( *rkParams.Suffix );

        for (int TypeIdx = 0; TypeIdx < rkParams.TypeNames.size(); TypeIdx++)
        {
            CCRC32 FullHash = BaseHash;
            FullHash.Hash( *rkParams.TypeNames[TypeIdx] );
            u32 PropertyID = FullHash.Digest();

            //@FIXME
#if 0
            // Check if this hash is a property ID - it's valid if there are any XMLs using this ID
            SGeneratedPropertyName PropertyName;
            CGameTemplate::XMLsUsingID(PropertyID, PropertyName.XmlList);

            if (PropertyName.XmlList.size() > 0)
            {
                // Generate a string with the complete name. (We wait to do this until now to avoid needless string allocation)
                PropertyName.Name = rkParams.Prefix;

                for (int WordIdx = 0; WordIdx < WordCache.size(); WordIdx++)
                {
                    int Index = WordCache[WordIdx].WordIndex;

                    if (WordIdx > 0 && rkParams.UseUnderscores)
                    {
                        PropertyName.Name += "_";
                    }

                    PropertyName.Name += mWords[Index].Word;
                }

                PropertyName.Name += rkParams.Suffix;
                PropertyName.Type = rkParams.TypeNames[TypeIdx];
                PropertyName.ID = PropertyID;

                if (SaveResults)
                {
                    mGeneratedNames.push_back(PropertyName);

                    // Check if we have too many saved results. This can cause memory issues and crashing.
                    // If we have too many saved results, then to avoid crashing we will force enable log output.
                    if (mGeneratedNames.size() > 9999)
                    {
                        gpUIRelay->AsyncMessageBox("Warning", "There are over 10,000 results. To avoid memory issues, results will no longer print to the screen. Check the log for the rest of the output.");
                        WriteToLog = true;
                        SaveResults = false;
                    }
                }

                // Log this out
                if ( WriteToLog )
                {
                    TString DelimitedXmlList;

                    for (int XmlIdx = 0; XmlIdx < PropertyName.XmlList.size(); XmlIdx++)
                    {
                        DelimitedXmlList += PropertyName.XmlList[XmlIdx] + "\n";
                    }

                    TString LogMsg = TString::Format("%s [%s] : 0x%08X\n", *PropertyName.Name, *PropertyName.Type, PropertyName.ID) + DelimitedXmlList;
                    Log::Write(LogMsg);
                }
            }
#endif
        }

        // Every 250 tests, check with the progress notifier. Update the progress
        // bar and check whether the user has requested to cancel the operation.
        TestsDone++;

        if ( (TestsDone % 250) == 0 )
        {
            if (pProgress->ShouldCancel())
                break;

            pProgress->Report(TestsDone, TotalTests);
        }
    }

    mIsRunning = false;
    mFinishedRunning = true;
}
