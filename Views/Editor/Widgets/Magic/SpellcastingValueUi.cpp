#include "SpellcastingValueUi.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"

namespace SpellcastingValueUi
{
namespace
{
const char *tr(const char *en,const char *de) { return uiTextForLocale(en,de); }
const char *modeLabel(SpellValueMode mode)
{
    if(mode==SpellValueMode::Calculated)return tr("Calculated","Berechnet");
    if(mode==SpellValueMode::Manual)return tr("Manual","Manuell");
    return tr("Legacy","Bisherige Auswertung");
}
std::string number(int value,bool signedValue)
{ return (signedValue&&value>=0?"+":"")+std::to_string(value); }
}
const char *reasonText(SpellcastingValues::Reason reason)
{
    using R=SpellcastingValues::Reason;
    switch(reason) {
    case R::None:return "";
    case R::NoProvider:return tr("Calculation unavailable: no rules provider is loaded. Choose rules or a manual value.","Berechnung nicht verfügbar: Kein Regelanbieter ist geladen. Wähle Regeln oder einen manuellen Wert.");
    case R::MissingBase:return tr("Calculation unavailable: the pinned base rules are missing. Restore that version or choose a manual value.","Berechnung nicht verfügbar: Die festgelegten Basisregeln fehlen. Stelle diese Version wieder her oder wähle einen manuellen Wert.");
    case R::MissingOverlay:return tr("Calculation unavailable: the pinned overlay is missing. Restore that version or choose a manual value.","Berechnung nicht verfügbar: Die festgelegte Regelergänzung fehlt. Stelle diese Version wieder her oder wähle einen manuellen Wert.");
    case R::AmbiguousBase:return tr("Calculation unavailable: the pinned base rules are ambiguous. Review the rules library.","Berechnung nicht verfügbar: Die festgelegten Basisregeln sind nicht eindeutig. Prüfe die Regelbibliothek.");
    case R::AmbiguousOverlay:return tr("Calculation unavailable: the pinned overlay is ambiguous. Review the rules library.","Berechnung nicht verfügbar: Die festgelegte Regelergänzung ist nicht eindeutig. Prüfe die Regelbibliothek.");
    case R::PinsChanged:return tr("Calculation unavailable: the rules pins changed. Reload the campaign to resolve them; its pins are retained.","Berechnung nicht verfügbar: Die Regelversionen wurden geändert. Lade die Kampagne erneut; ihre Festlegungen bleiben erhalten.");
    case R::ProviderChanged:return tr("Calculation unavailable: the loaded rules provider changed. Reload the campaign to resolve its pinned rules.","Berechnung nicht verfügbar: Der geladene Regelanbieter wurde geändert. Lade die Kampagne erneut, um ihre festgelegten Regeln aufzulösen.");
    case R::InvalidDefinition:return tr("Calculation unavailable: the pinned rules definition is invalid. Review it or choose a manual value.","Berechnung nicht verfügbar: Die festgelegte Regeldefinition ist ungültig. Prüfe sie oder wähle einen manuellen Wert.");
    case R::InvalidFormula:return tr("Calculation unavailable: a rules formula could not be evaluated. Review the formula or choose a manual value.","Berechnung nicht verfügbar: Eine Regelformel konnte nicht ausgewertet werden. Prüfe die Formel oder wähle einen manuellen Wert.");
    }
    return "";
}
std::optional<Edit> drawField(const Campaign &campaign,const std::map<std::string,int> &attributes,
    const CharacterSpellcastingModel &casting,int level,SpellcastingValues::Field field)
{
    const bool attack=field==SpellcastingValues::Field::Attack;
    const auto resolved=SpellcastingValues::resolve(campaign,attributes,casting,level,field);
    const int stored=attack?casting.spellAttackBonus:casting.spellSaveDc;
    ImGui::PushID(attack?"SpellAttackValue":"SpellSaveValue");
    std::optional<Edit> edit;
    ImGui::TextUnformatted(attack?tr("Spell attack","Zauberangriff"):tr("Spell save DC","Zauberrettungs-SG"));
    ImGui::SetNextItemWidth(-1);
    if(FormUi::BeginCombo(tr("Value source","Wertquelle"),modeLabel(resolved.mode))) {
        for(const auto mode:{SpellValueMode::Legacy,SpellValueMode::Calculated,SpellValueMode::Manual}) {
            const bool enabled=mode!=SpellValueMode::Calculated||resolved.calculated.has_value();
            ImGui::BeginDisabled(!enabled);
            if(ImGui::Selectable(modeLabel(mode),mode==resolved.mode))edit=Edit{mode,stored,false};
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    int value=resolved.effective.value_or(stored);
    ImGui::SetNextItemWidth(-1);
    if(FormUi::InputInt(attack?tr("Spell attack override","Zauberangriff überschreiben"):
        tr("Spell save DC override","Zauberrettungs-SG überschreiben"),&value,0,0))
        edit=Edit{SpellValueMode::Manual,attack?std::max(-20,value):std::max(1,value),true};
    // Changing source while typing must not move the active numeric widget.
    if(resolved.mode==SpellValueMode::Legacy)
        ImGui::TextWrapped("%s",attack?tr("Older files: stored 0 means calculated; any other number overrides it.",
            "Ältere Dateien: Gespeicherte 0 bedeutet berechnet; jede andere Zahl hat Vorrang."):
            tr("Older files: stored DC 10 or lower means calculated; above 10 overrides it.",
            "Ältere Dateien: Gespeicherter SG bis 10 bedeutet berechnet; über 10 hat die Zahl Vorrang."));
    if(!resolved.effective)ImGui::TextWrapped("%s",reasonText(resolved.reason));
    if(!resolved.effective)ImGui::TextWrapped("%s",tr("The number above is retained for manual use; it is not an available calculation.",
        "Die Zahl oben bleibt für manuelle Verwendung erhalten; sie ist kein verfügbares Berechnungsergebnis."));
    ImGui::TextWrapped("%s",tr("Editing the number selects Manual.","Eine Zahleneingabe wählt Manuell."));
    if(resolved.mode!=SpellValueMode::Manual)
        ImGui::TextWrapped(tr("Choosing Manual restores the stored number: %d.","Manuell verwendet wieder die gespeicherte Zahl: %d."),stored);
    // Keep this keyboard target present after activation. Re-selecting the
    // current source is a no-op in the controller, never a second save.
    if(drawUiButton(attack?tr("Use calculated attack","Berechneten Angriff verwenden"):
        tr("Use calculated DC","Berechneten SG verwenden"),resolved.calculated.has_value(),reasonText(resolved.reason),UiButtonRole::Secondary,ImVec2(-1,0)))
        edit=Edit{SpellValueMode::Calculated,stored,false};
    ImGui::PopID();
    return edit;
}
void drawDraftFields(const Campaign &campaign,const std::map<std::string,int> &attributes,CharacterSpellcastingModel &casting,int level)
{
    if(const auto edit=drawField(campaign,attributes,casting,level,SpellcastingValues::Field::Attack)) {
        casting.spellAttackMode=edit->mode;if(edit->valueEdited)casting.spellAttackBonus=edit->value;
    }
    if(const auto edit=drawField(campaign,attributes,casting,level,SpellcastingValues::Field::SaveDc)) {
        casting.spellSaveDcMode=edit->mode;if(edit->valueEdited)casting.spellSaveDc=edit->value;
    }
    drawSourceDetails(campaign,attributes,casting,level);
}
void drawSourceDetails(const Campaign &campaign,const std::map<std::string,int> &attributes,const CharacterSpellcastingModel &casting,int level)
{
    if(!ImGui::CollapsingHeader(tr("Spell calculation and source","Zauberberechnung und Quelle")))return;
    const auto source=SpellcastingValues::provenance(campaign);
    const auto pin=[&](const char *label,const std::string &id,const std::string &version) {
        ImGui::TextWrapped("%s: %s%s%s",label,id.empty()?tr("Not pinned","Nicht festgelegt"):id.c_str(),version.empty()?"":" / ",version.c_str());
    };
    pin(tr("Campaign base rules","Basisregeln der Kampagne"),source.requested.rulesetId,source.requested.version);
    pin(tr("Campaign overlay","Regelergänzung der Kampagne"),source.requested.overlayId,source.requested.overlayVersion);
    if(source.providerName.empty())ImGui::TextWrapped("%s: %s",tr("Loaded provider","Geladener Regelanbieter"),tr("None","Keiner"));
    else ImGui::TextWrapped("%s: %s (%s / %s)",tr("Loaded provider","Geladener Regelanbieter"),source.providerName.c_str(),source.providerId.c_str(),source.providerVersion.c_str());
    if(source.reason!=SpellcastingValues::Reason::None)ImGui::TextWrapped("%s",reasonText(source.reason));
    for(const auto field:{SpellcastingValues::Field::Attack,SpellcastingValues::Field::SaveDc}) {
        const auto value=SpellcastingValues::resolve(campaign,attributes,casting,level,field);
        ImGui::SeparatorText(field==SpellcastingValues::Field::Attack?tr("Calculated attack","Berechneter Angriff"):tr("Calculated DC","Berechneter SG"));
        if(value.calculated&&value.abilityModifier&&value.proficiency)
            ImGui::TextWrapped("%d + %s (%+d) + %s (%+d) = %d",value.base,casting.ability.empty()?tr("Ability","Attribut"):casting.ability.c_str(),*value.abilityModifier,tr("proficiency","Übung"),*value.proficiency,*value.calculated);
        else ImGui::TextWrapped("%s",reasonText(value.reason));
        if(!value.abilityFormula.empty())ImGui::TextWrapped("%s: %s",tr("Configured ability formula","Hinterlegte Attributsformel"),value.abilityFormula.c_str());
        if(!value.proficiencyFormula.empty())ImGui::TextWrapped("%s: %s",tr("Configured proficiency formula","Hinterlegte Übungsformel"),value.proficiencyFormula.c_str());
        if(value.calculated&&value.abilityFormula.empty()&&value.proficiencyFormula.empty())
            ImGui::TextWrapped("%s",tr("Values supplied by the loaded rules provider.","Werte des geladenen Regelanbieters."));
    }
    ImGui::TextWrapped("%s",tr("A spell's own attack bonus takes precedence for that spell. These controls do not change campaign rules pins.",
        "Ein eigener Angriffsbonus des Zaubers hat für diesen Zauber Vorrang. Diese Eingaben ändern keine festgelegten Regelversionen der Kampagne."));
}
std::string spellValueText(const Campaign &campaign,const std::map<std::string,int> &attributes,
    const CharacterSpellcastingModel &casting,int level,const SpellModel &spell)
{
    if(spell.resolution==SpellResolutionType::SpellAttack&&!spell.attackBonus.empty())
        return std::string(tr("Attack ","Angriff "))+spell.attackBonus+tr(" (spell override)"," (Zaubervorgabe)");
    if(spell.resolution!=SpellResolutionType::SpellAttack&&spell.resolution!=SpellResolutionType::SavingThrow)return {};
    const bool attack=spell.resolution==SpellResolutionType::SpellAttack;
    const auto value=SpellcastingValues::resolve(campaign,attributes,casting,level,attack?SpellcastingValues::Field::Attack:SpellcastingValues::Field::SaveDc);
    if(!value.effective)return reasonText(value.reason);
    return std::string(attack?tr("Attack ","Angriff "):tr("DC ","SG "))+number(*value.effective,attack);
}
}
