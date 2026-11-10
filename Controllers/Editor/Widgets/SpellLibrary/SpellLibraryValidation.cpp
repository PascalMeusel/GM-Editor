#include "SpellLibraryControllerSupport.hpp"

std::string SpellLibraryController::importPreviewRollbackPlan() const
{
    if (!_importPreviewReady || _importPreviewRows.empty())
        return "";

    int added = 0;
    int updated = 0;
    std::vector<std::string> updatedNames;
    for (const ImportPreviewRow &row : _importPreviewRows)
    {
        if (row.updatesExisting)
        {
            ++updated;
            updatedNames.push_back(row.spell.name.empty() ? uiText("<Unnamed Spell>", "<Unbenannter Zauber>") : row.spell.name);
        }
        else
        {
            ++added;
        }
    }

    std::string plan = uiText("Rollback plan: applying this preview will add ", "Rücksetzplan: Beim Anwenden dieser Vorschau werden ") +
                       std::to_string(added) + uiText(" spell(s) and update ", " Zauber hinzugefügt und ") +
                       std::to_string(updated) + uiText(".", " aktualisiert.");
    if (!updatedNames.empty())
        plan += uiText(" Updates overwrite campaign spells: ", "Diese Kampagnenzauber werden überschrieben: ") + joinLimitedNames(updatedNames) + ".";
    plan += uiText(" The save receipt will show the backup and recovery snapshot paths.",
                   " Der Speicherbeleg zeigt die Pfade der Sicherung und des Wiederherstellungspunkts.");
    return plan;
}

std::vector<std::string> SpellLibraryController::validateSpell(const SpellModel &spell) const
{
    std::vector<std::string> warnings;
    if (trimCopy(spell.name).empty())
        warnings.push_back(uiText("Name is required before this spell can be saved or imported.",
                                  "Vor dem Speichern oder Importieren ist ein Name erforderlich."));
    if (spell.level < 0 || spell.level > 9)
        warnings.push_back(uiText("Spell level must be 0 for a cantrip or 1-9 for a leveled spell.",
                                  "Der Zaubergrad muss für einen Zaubertrick 0 oder für einen höhergradigen Zauber 1–9 betragen."));

    switch (spell.resolution)
    {
    case SpellResolutionType::SpellAttack:
        if (trimCopy(spell.damage).empty() && trimCopy(spell.effectSummary).empty())
            warnings.push_back(uiText("Spell attack spells usually need damage or a clear effect summary.",
                                      "Zauberangriffe benötigen normalerweise Schaden oder eine klare Effektzusammenfassung."));
        break;
    case SpellResolutionType::SavingThrow:
        if (trimCopy(spell.savingThrowAbility).empty())
            warnings.push_back(uiText("Saving throw spells need a save ability such as Dexterity, Constitution, or Wisdom.",
                                      "Zauber mit Rettungswurf benötigen ein Attribut wie Geschicklichkeit, Konstitution oder Weisheit."));
        if (trimCopy(spell.damage).empty() && trimCopy(spell.effectSummary).empty())
            warnings.push_back(uiText("Saving throw spells usually need damage or an effect summary for the failed save.",
                                      "Zauber mit Rettungswurf benötigen normalerweise Schaden oder eine Effektzusammenfassung für einen misslungenen Rettungswurf."));
        break;
    case SpellResolutionType::Healing:
        if (trimCopy(spell.healing).empty() && trimCopy(spell.effectSummary).empty())
            warnings.push_back(uiText("Healing spells need a healing amount or a clear effect summary.",
                                      "Heilzauber benötigen eine Heilmenge oder eine klare Effektzusammenfassung."));
        break;
    case SpellResolutionType::SummonOrZone:
        if (trimCopy(spell.area).empty())
            warnings.push_back(uiText("Summon/zone spells should state the area, target, or placed zone.",
                                      "Beschwörungs- und Zonen-Zauber sollten Bereich, Ziel oder platzierte Zone angeben."));
        if (trimCopy(spell.effectSummary).empty() && trimCopy(spell.damage).empty())
            warnings.push_back(uiText("Summon/zone spells need an effect summary so the table knows what persists.",
                                      "Beschwörungs- und Zonen-Zauber benötigen eine Effektzusammenfassung, damit am Spieltisch klar ist, was bestehen bleibt."));
        break;
    case SpellResolutionType::Utility:
    default:
        if (trimCopy(spell.effectSummary).empty() && trimCopy(spell.description).empty())
            warnings.push_back(uiText("Utility spells benefit from an effect summary or notes for table use.",
                                      "Unterstützungszauber sollten eine Effektzusammenfassung oder Hinweise für den Spieltisch enthalten."));
        break;
    }

    const std::string duration = toLowerCopy(trimCopy(spell.duration));
    if (spell.concentration && (duration.empty() || duration == "instantaneous"))
        warnings.push_back(uiText("Concentration spells need a duration longer than Instantaneous.",
                                  "Konzentrationszauber benötigen eine längere Wirkungsdauer als ‚Sofort‘."));
    if (looksLikeMalformedDiceText(spell.damage))
        warnings.push_back(uiText("Damage has numbers but no dice expression like 3d6. Move narrative text to Effect Summary if this is intentional.",
                                  "Der Schaden enthält Zahlen, aber keinen Würfelausdruck wie 3d6. Verschiebe erzählerischen Text gegebenenfalls in die Effektzusammenfassung."));
    if (looksLikeMalformedDiceText(spell.healing))
        warnings.push_back(uiText("Healing has numbers but no dice expression like 1d8. Move narrative text to Effect Summary if this is intentional.",
                                  "Die Heilung enthält Zahlen, aber keinen Würfelausdruck wie 1d8. Verschiebe erzählerischen Text gegebenenfalls in die Effektzusammenfassung."));

    return warnings;
}

std::vector<std::string> SpellLibraryController::validateDraftSpell() const
{
    return validateSpell(_draftSpell);
}

bool SpellLibraryController::draftHasBlockingValidationErrors() const
{
    return trimCopy(_draftSpell.name).empty() || _draftSpell.level < 0 || _draftSpell.level > 9;
}

std::string SpellLibraryController::resolutionGuidance(SpellResolutionType resolution) const
{
    switch (resolution)
    {
    case SpellResolutionType::SpellAttack:
        return uiText("Use for spells that roll against AC. The caster's normal spell attack bonus is used unless an override is filled in.",
                      "Für Zauber verwenden, die gegen die RK gewürfelt werden. Ohne eingetragene Abweichung gilt der normale Zauberangriffsbonus des Wirkenden.");
    case SpellResolutionType::SavingThrow:
        return uiText("Use when targets roll a save. Fill the save ability and the failed-save damage or effect summary.",
                      "Verwenden, wenn Ziele einen Rettungswurf ablegen. Trage das Attribut sowie Schaden oder Effekt bei Misserfolg ein.");
    case SpellResolutionType::Healing:
        return uiText("Use for spells that restore hit points or grant restorative effects. Damage and save fields are hidden.",
                      "Für Zauber verwenden, die Trefferpunkte oder andere regenerative Effekte gewähren. Schaden und Rettungswurf werden ausgeblendet.");
    case SpellResolutionType::SummonOrZone:
        return uiText("Use for summons, areas, auras, walls, and persistent zones. State the area and what remains in play.",
                      "Für Beschwörungen, Bereiche, Auren, Wände und anhaltende Zonen verwenden. Gib den Bereich und den bestehen bleibenden Effekt an.");
    case SpellResolutionType::Utility:
    default:
        return uiText("Use for narrative, exploration, ritual, movement, information, and other spells without attack/save/healing resolution.",
                      "Für erzählerische, Erkundungs-, Ritual-, Bewegungs- und Informationszauber ohne Angriffs-, Rettungswurf- oder Heilungsauflösung verwenden.");
    }
}

std::string SpellLibraryController::resolutionSummary(const SpellModel &spell) const
{
    switch (spell.resolution)
    {
    case SpellResolutionType::SpellAttack:
        return uiText("Combat preview: roll spell attack", "Kampfvorschau: Zauberangriff würfeln") +
               (spell.damage.empty() ? std::string{} : uiText(" for ", " für ") + spell.damage) + ".";
    case SpellResolutionType::SavingThrow:
        return uiText("Combat preview: target rolls ", "Kampfvorschau: Ziel würfelt ") +
               (spell.savingThrowAbility.empty() ? uiText("a saving throw", "einen Rettungswurf")
                                                 : spell.savingThrowAbility + uiText(" save", "-Rettungswurf")) +
               (spell.damage.empty() ? std::string{} : uiText(" against ", " gegen ") + spell.damage) + ".";
    case SpellResolutionType::Healing:
        return uiText("Table preview: apply healing", "Spieltischvorschau: Heilung anwenden") +
               (spell.healing.empty() ? std::string{} : uiText(" of ", " in Höhe von ") + spell.healing) + ".";
    case SpellResolutionType::SummonOrZone:
        return uiText("Table preview: place or track ", "Spieltischvorschau: Platzieren oder nachhalten: ") +
               (spell.area.empty() ? uiText("a summon/zone", "Beschwörung oder Zone") : spell.area) + ".";
    case SpellResolutionType::Utility:
    default:
        return uiText("Table preview: read the effect summary and adjudicate without attack/save automation.",
                      "Spieltischvorschau: Effektzusammenfassung lesen und ohne automatische Angriffs- oder Rettungswurfauflösung entscheiden.");
    }
}
