#pragma once
#include "Models/Character/PortraitRecipe.hpp"
#include "Models/NPC/NPCModel.hpp"
#include "Models/Character/CharacterModel.hpp"
#include <cstdio>
#include <random>

namespace gm::creation
{
// A seed describes creative content only; entity IDs remain fresh on every creation.
class Generator
{
    std::mt19937 engine;
public:
    explicit Generator(std::uint32_t seed) : engine(seed) {}
    int number(int count) { return int(engine() % std::uint32_t(count)); }
    template<std::size_t N> std::string pick(const char *const (&items)[N]) { return items[number(int(N))]; }
    std::string uniqueName(const std::string &candidate, const std::vector<std::string> &existing)
    {
        std::string result = candidate;
        for(int suffix=2; std::find(existing.begin(),existing.end(),result)!=existing.end(); ++suffix)
            result = candidate + " " + std::to_string(suffix);
        return result;
    }
    std::string name()
    {
        static const char *const first[] = {"Arel", "Mira", "Thorne", "Seren", "Lio", "Nessa", "Bram", "Ivo", "Zara", "Orin", "Tavi", "Kesh", "Dara", "Rook", "Vela", "Suri", "Fen", "Yara", "Niko", "Ember"};
        static const char *const last[] = {"Voss", "Quill", "Ash", "Vale", "Reed", "Copperleaf", "Stonebridge", "Moonwake", "Farrow", "Brightwater", "Thistledown", "Greyhaven", "Moss", "Flint", "Dawn", "Riverglass", "Wren", "Cinder", "Hollow", "Storm"};
        return pick(first) + " " + pick(last);
    }
    void portrait(EntityMetadata &metadata, const std::string &race)
    {
        portraits::Recipe r;
        r.species = portraits::speciesForRace(race);
        r.seed = engine() & 0x7fffffffu;
        portraits::vary(r);
        r.expression = number(8);
        portraits::write(metadata, r);
        metadata.customProperties["character.species"] = race;
    }
    void playerIdentity(CharacterModel &player)
    {
        const auto generated = name();
        std::snprintf(player.nameBuffer, sizeof(player.nameBuffer), "%s", generated.c_str());
        portrait(player.metadata, player.selectedRace);
        static const char *const backgrounds[] = {"Former courier", "Village artisan", "Travelling scholar", "Retired sailor", "Temple attendant", "Wilderness scout", "Market performer", "Displaced farmer", "Guild apprentice", "Caravan guard"};
        static const char *const motives[] = {"Searching for a missing mentor.", "Repaying the stranger who once saved their life.", "Seeking a home for their scattered community.", "Trying to learn who forged a letter in their name.", "Recovering a family heirloom sold in hard times.", "Hoping to earn the trust of an estranged sibling.", "Determined to chart a route others call impossible.", "Looking for evidence to clear a friend's name."};
        player.background = pick(backgrounds);
        player.backstory = pick(motives) + " " + name() + " knows why they left home.";
    }
    NPCModel npc(const std::vector<std::string> &races, NPCCategory category = NPCCategory::Person)
    {
        NPCModel result;
        result.category = category;
        result.name = name();
        const auto race = races.empty() ? std::string("Human") : races[number(int(races.size()))];
        portrait(result.metadata, race);
        hooks(result);
        static const char *const details[] = {"Wears a carefully mended travel cloak.", "Keeps a pressed flower in a brass locket.", "Carries a notebook filled with tiny sketches.", "Has ink-stained fingertips and polished boots.", "Wears mismatched earrings from two old friends.", "Ties their sleeves with faded blue ribbon.", "Carries a walking stick carved with place names.", "Keeps checking a repaired pocket watch.", "Wears a bright scarf over practical work clothes.", "Has a weathered satchel covered in travel patches."};
        result.description = race + ". " + pick(details);
        static const char *const voices[] = {
            "Speaks softly and waits for an answer. 'Tell me what you need from me.'",
            "Clips sentences when nervous. 'We have until dawn. No longer.'",
            "Uses precise words and corrects dates. 'Start at the beginning. Which day?'",
            "Laughs at their own bad jokes, then turns serious. 'I wish this were a better story.'",
            "Speaks slowly while weighing a coin. 'What will this cost us both?'",
            "Asks practical questions. 'Who is coming, and what should I pack?'",
            "Repeats names to remember them. 'I will remember that promise.'",
            "Uses vivid comparisons. 'That plan leaks worse than my old roof.'",
            "Lowers their voice for names. 'You did not hear that from me.'",
            "Pauses to listen before speaking. 'Someone else is out there.'"
        };
        result.dialogueNotes = pick(voices);
        const auto ally = uniqueName(name(), {result.name});
        const auto rival = uniqueName(name(), {result.name, ally});
        result.relationships = ally + " sheltered them during a crisis; " + rival + " wants repayment of an old debt.";
        static const char *const attitudes[] = {"Offers help in exchange for a small favor.", "Shares facts freely but avoids personal questions.", "Wary until the party offers concrete help.", "Welcomes newcomers and wants to hear their story.", "Distrusts promises; asks for evidence first.", "Afraid of attracting attention; asks to meet privately."};
        result.attitudeTowardParty = pick(attitudes);
        if(category == NPCCategory::FactionEntity)
        {
            result.name = "The " + name() + " Fellowship";
            result.role = "Mutual-aid society";
            result.description = "A network of neighbours who share supplies and shelter.";
            result.goals = "Keep a threatened refuge open through the coming winter.";
            result.resources = "A communal storehouse, volunteer couriers and safe rooms.";
            result.threatLocation = "The old meeting hall beside the refuge.";
            result.secrets = "One benefactor demands control of the refuge in return for funding.";
        }
        else if(category == NPCCategory::Deity)
        {
            result.name += ", Keeper of Promises";
            result.role = "Patron of oaths and reconciliation";
            result.description = "Appears in a " + race + " aspect. " + pick(details);
            result.goals = "Restore a broken covenant before its last witness dies.";
            result.resources = "A shrine offering shelter and a vision of the original oath.";
            result.threatLocation = "The ruined shrine where the covenant was sworn.";
            result.secrets = "The covenant was founded on a promise the patron also broke.";
        }
        return result;
    }
    void hooks(NPCModel &result)
    {
        struct Hook { const char *role, *goal, *resource, *place, *secret; };
        static const Hook hooks[] = {
            {"caravan merchant", "Recover a shipment before the guild audit tomorrow.", "A spare wagon and a ledger of toll payments.", "The east gate caravan yard.", "The missing shipment conceals medicine stolen from a noble's storehouse."},
            {"river guide", "Find the missing ferry crew before the river rises tonight.", "A shallow boat and a map of safe channels.", "The flooded ferry landing.", "Took payment to guide smugglers through the same channel last week."},
            {"watch captain", "Find a missing patrol without alarming the market crowd.", "Two loyal scouts and a sealed warrant.", "The watchhouse beside the market.", "A superior ordered the patrol into an ambush."},
            {"temple healer", "Deliver an antidote before the next bell.", "Three doses of antidote and sanctuary in the infirmary.", "The temple infirmary.", "The poison came from a donor's supposedly harmless gift."},
            {"archive keeper", "Retrieve a stolen page before the auction begins.", "A restricted reading pass and a rubbing of the page.", "The archive's locked reading room.", "Copied the page for the thief to pay a family debt."},
            {"innkeeper", "Get a threatened witness safely out of town at dawn.", "A cellar exit and a room under a false name.", "The inn's back kitchen.", "Recognized the pursuer as an old travelling companion."},
            {"relic hunter", "Reach a sealed tomb before a rival expedition.", "A broken key and a sketch of the entrance.", "A camp below the old burial ridge.", "The key was taken from the rival's dying mentor."},
            {"guild artisan", "Prove a machine was sabotaged before the public demonstration.", "Workshop access and a sample of the damaged mechanism.", "The guild workshop.", "An apprentice was blamed for the artisan's earlier mistake."},
            {"exiled envoy", "Arrange a safe meeting before the truce expires.", "A diplomatic seal and a neutral meeting room.", "The abandoned customs house.", "The envoy's own patron benefits if the truce fails."},
            {"forest warden", "Move the woodcutters away from a waking ruin tonight.", "Hidden trails and an old boundary map.", "The logging camp at the forest edge.", "Disturbed the ruin while searching for a lost sibling."}
        };
        const auto &h = hooks[number(int(std::size(hooks)))];
        result.role = h.role; result.goals = h.goal; result.resources = h.resource;
        result.threatLocation = h.place; result.secrets = h.secret;
    }
};
}
