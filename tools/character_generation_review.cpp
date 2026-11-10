#include "Models/Character/CharacterDraftGenerator.hpp"
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    const int batch = std::stoi(argv[1]);
    std::ofstream out(argv[2]);
    if (!out || batch < 1 || batch > 10) return 2;
    const std::vector<std::string> races = {"Aasimar", "Dragonborn", "Dwarf", "Elf", "Gnome", "Goliath", "Halfling", "Human", "Orc", "Tiefling"};
    out << "# Character review batch " << batch << "\n\n";
    std::set<std::string> names;
    for (int i = 0; i < 10; ++i)
    {
        const auto seed = 1000u + batch * 100u + i;
        gm::creation::Generator g(seed);
        auto n = g.npc({races[i]});
        const bool player = batch >= 5 && i % 2 == 0;
        if(player)
        {
            CharacterModel p;
            p.selectedRace = races[i];
            g.playerIdentity(p);
            n.metadata = p.metadata;
            n.name = p.nameBuffer;
            n.role = "Player character";
            n.description = p.background;
            n.goals = p.backstory;
            n.resources = "Player chooses equipment using campaign rules.";
            n.secrets = "Player chooses what to reveal.";
            n.relationships.clear(); n.dialogueNotes.clear(); n.threatLocation.clear();
        }
        n.name = g.uniqueName(n.name, std::vector<std::string>(names.begin(),names.end()));
        const auto portrait = gm::portraits::read(n.metadata);
        if(n.goals.empty() || n.resources.empty() || n.secrets.empty() || !gm::portraits::exists(n.metadata)) throw std::runtime_error("Incomplete draft");
        names.insert(n.name);
        out << "## " << ((batch-1)*10+i+1) << ". " << n.name << "\n\n"
            << "Seed: " << seed << "; species: " << races[i] << "; portrait type: " << portrait.species << "; expression: " << portrait.expression << "\n\n"
            << n.description << "\n\nRole: " << n.role << "\n\nGoal: " << n.goals << "\n\nResources: " << n.resources
            << "\n\nLocation: " << n.threatLocation << "\n\nSecret: " << n.secrets << "\n\nRelationships: " << n.relationships << "\n\nVoice: " << n.dialogueNotes << "\n\n";
    }
    std::cout << "Batch " << batch << ": 10 complete drafts, " << names.size() << " distinct names\n";
    return names.size() == 10 ? 0 : 1;
}
