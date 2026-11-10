#include "Models/Rulesets/DND/DND5e.hpp"
#include "Models/Rulesets/DefinitionBackedRuleSet.hpp"
#include "Models/Rulesets/RulesetLibrary.hpp"
#include "Models/Rulesets/RulesetStudioLibrary.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
bool require(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "FAILED: " << message << '\n';
    return condition;
}

RulesetDefinitionModel definition(std::string stableId,
                                  std::string rulesetId,
                                  std::string name,
                                  std::string version,
                                  bool published,
                                  bool installed)
{
    RulesetDefinitionModel result;
    result.metadata.id = std::move(stableId);
    result.rulesetId = std::move(rulesetId);
    result.name = std::move(name);
    result.version = std::move(version);
    result.published = published;
    result.installedGlobally = installed;
    return result;
}
}

int main()
{
    std::vector<RulesetDefinitionModel> installed{
        definition("ember@2.0.0", "ember", "Ember", "2.0.0", true, true),
        definition("draft@draft", "draft", "Draft", "0.1.0", false, true),
        definition("local@1", "local", "Local", "1.0.0", true, false),
        definition("ember@1.0.0", "ember", "Ember", "1.0.0", true, true),
        definition("ember-duplicate", "ember", "Ember duplicate", "1.0.0", true, true)};

    const std::vector<RulesetLibraryEntry> library = buildRulesetLibrary(installed);
    bool ok = true;
    ok &= require(library.size() == 3, "library must contain the fallback and two unique published pins");
    ok &= require(library.front().builtIn, "compiled D&D must remain the first fallback");
    ok &= require(std::string(library.front().ruleSet->getId()) == "dnd-2024", "unexpected fallback ruleset");
    ok &= require(library[1].reference.rulesetId == "ember" && library[1].reference.version == "1.0.0",
                  "first published version did not keep its exact pin");
    ok &= require(library[2].reference.rulesetId == "ember" && library[2].reference.version == "2.0.0",
                  "second published version did not keep its exact pin");

    Campaign customCampaign;
    customCampaign.ruleSet = library[2].ruleSet;
    pinCampaignRuleset(customCampaign);
    ok &= require(customCampaign.rulesetReference.rulesetId == "ember",
                  "definition-backed campaign did not pin the logical ruleset ID");
    ok &= require(customCampaign.rulesetReference.version == "2.0.0",
                  "definition-backed campaign must pin the plain immutable version");

    Campaign fallbackCampaign;
    fallbackCampaign.ruleSet = std::make_shared<DND5e>();
    pinCampaignRuleset(fallbackCampaign);
    ok &= require(fallbackCampaign.rulesetReference.rulesetId == "dnd-2024",
                  "compiled fallback did not keep its stable ID");
    ok &= require(fallbackCampaign.rulesetReference.version == "dnd-2024-5.5e",
                  "compiled fallback did not keep its version ID");

    Campaign emberOne;
    emberOne.rulesetReference = {"ember", "1.0.0", {}, {}};
    Campaign emberTwo;
    emberTwo.rulesetReference = {"ember", "2.0.0", {}, {}};
    Campaign emberTwoAgain;
    emberTwoAgain.rulesetReference = {"ember", "2.0.0", {}, {}};
    const std::vector<RulesetStudioFamily> studio = buildRulesetStudioLibrary(
        installed, {emberOne, emberTwo, emberTwoAgain});
    ok &= require(studio.size() == 2, "studio should group installed drafts and versions into two families");
    const auto emberFamily = std::find_if(studio.begin(), studio.end(), [](const RulesetStudioFamily &family) {
        return family.rulesetId == "ember";
    });
    ok &= require(emberFamily != studio.end(), "studio lost the Ember ruleset family");
    if (emberFamily != studio.end())
    {
        ok &= require(emberFamily->publishedVersions.size() == 2,
                      "studio history should collapse duplicate exact pins");
        ok &= require(emberFamily->publishedVersions.front().definition.version == "2.0.0",
                      "studio history should show the newest numeric version first");
        ok &= require(emberFamily->publishedVersions.front().campaignUseCount == 2,
                      "studio history should show exact campaign pin usage");
        ok &= require(emberFamily->campaignUseCount() == 3,
                      "family usage should sum every exact campaign pin once");
    }

    ok &= require(compareRulesetVersions("1.10.0", "1.2.0") > 0,
                  "numeric ruleset version ordering must not be lexical");
    ok &= require(compareRulesetVersions("v2", "1.99.99") > 0,
                  "numeric version ordering should accept a v prefix");
    ok &= require(suggestNextRulesetVersion("ember", installed) == "2.0.1",
                  "next ruleset version should increment the newest published patch");

    const RulesetDefinitionModel nextDraft = createNextRulesetVersionDraft(installed.front(), installed);
    ok &= require(!nextDraft.published && nextDraft.publishedAtUtc.empty(),
                  "next-version draft must remain editable and unpublished");
    ok &= require(nextDraft.rulesetId == "ember" && nextDraft.version == "2.0.1",
                  "next-version draft should preserve identity and use the suggested version");
    ok &= require(nextDraft.metadata.id == "ember@draft",
                  "next-version draft should reuse the family's editable draft slot");
    ok &= require(installed.front().published && installed.front().version == "2.0.0",
                  "creating a draft must not mutate its immutable source version");

    return ok ? 0 : 1;
}
