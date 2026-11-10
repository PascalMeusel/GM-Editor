#pragma once
#include "Controllers/Editor/Widgets/CharacterSheet/CharacterSheetController.hpp"
#include "Models/Rulesets/SpellcastingValues.hpp"
#include "Models/Rulesets/RulesetResolution.hpp"
struct SpellOverrideUiProbe
{
    static void draw(CharacterSheetController &c){c.view->draw();}
    static void cast(CharacterSheetController &c,int index,bool ritual=false){c.castSpellFromSelectedCharacter(index,1,ritual,false);}
    static const std::string &error(const CharacterSheetController &c){return c._errorMessage;}
};
