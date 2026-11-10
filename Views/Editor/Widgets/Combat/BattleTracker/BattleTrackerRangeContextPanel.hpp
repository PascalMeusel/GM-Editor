    EncounterModel &encounter = _controller._draftEncounter;
    if (_controller._combatStep != 1)
        return;
    if (_controller._draftEncounter.combatPaused)
        return;
    if (encounter.combatPhase != CombatPhase::ActionResolution)
        return;
    if (encounter.participants.size() < 2)
        return;

    ImGui::Separator();
    drawHelpText("3. Resolve an Attack", "Choose a target and weapon/action, enter the dice results rolled at the table, then apply damage if the attack hits.");
    _controller._selectedParticipantIndex = encounter.activeParticipantIndex;

    std::vector<const char *> participantLabels;
    for (const auto &participant : encounter.participants)
        participantLabels.push_back(participant.displayName.c_str());
    if (_controller._rangeTargetIndex < 0 || _controller._rangeTargetIndex >= static_cast<int>(participantLabels.size()))
        _controller._rangeTargetIndex = encounter.activeParticipantIndex == 0 ? 1 : 0;
    if (_controller._rangeTargetIndex == encounter.activeParticipantIndex && encounter.participants.size() > 1)
        _controller._rangeTargetIndex = encounter.activeParticipantIndex == 0 ? 1 : 0;

    auto resetActionProgress = [&]() {
        _controller._actionAdjudicationReviewed = false;
        _controller._lastAttackResolved = false;
        _controller._lastAttackHit = false;
        _controller._lastAttackMessage.clear();
        _controller._healingDiceEntered = false;
    };

    EncounterParticipantModel &attacker = encounter.participants[_controller._selectedParticipantIndex];
    if (!attacker.actions.empty() && _controller._selectedActionIndex >= static_cast<int>(attacker.actions.size()))
        _controller._selectedActionIndex = 0;

    MonsterActionModel selectedAction;
    std::vector<int> selectedTargets;
    RuleSpellResolutionSummary spellSummary;
    bool selectedActionIsSpell = false;
    bool hasSpellSummary = false;
    bool enforceGridRange = !encounter.theaterOfMind;
    bool allTargetsInRange = false;
    bool targetInRange = false;
    bool targetBeyondNormalRange = false;
    int distance = 0;
    int actionRange = 0;
    int normalRange = 0;
    int adjudicationCount = 0;
    int activeResolutionStep = 0;

    auto refreshActionState = [&]() {
        selectedAction = _controller.getSelectedAction();
        selectedTargets = _controller.getSelectedTargetIndices();
        distance = _controller.distanceBetweenParticipants(_controller._selectedParticipantIndex, _controller._rangeTargetIndex);
        actionRange = std::max(selectedAction.reachFeet, selectedAction.rangeFeet);
        normalRange = std::max(selectedAction.reachFeet, selectedAction.normalRangeFeet);
        enforceGridRange = !encounter.theaterOfMind;
        targetInRange = !enforceGridRange || distance <= actionRange;
        allTargetsInRange = std::all_of(selectedTargets.begin(), selectedTargets.end(), [&](int targetIndex) {
            return !enforceGridRange || _controller.distanceBetweenParticipants(_controller._selectedParticipantIndex, targetIndex) <= actionRange;
        });
        targetBeyondNormalRange = enforceGridRange && targetInRange && distance > normalRange;
        selectedActionIsSpell = _controller.selectedActionIsSpell();
        spellSummary = RuleSpellResolutionSummary{};
        hasSpellSummary = selectedActionIsSpell && _controller.getSelectedSpellResolutionSummary(spellSummary);
        adjudicationCount = adjudicationModifierCount();
        activeResolutionStep = actionResolutionCurrentStep(
            selectedAction,
            selectedTargets,
            allTargetsInRange,
            _controller._actionAdjudicationReviewed,
            _controller._lastAttackResolved);
    };

    refreshActionState();
    drawActionResolutionGuide(
        selectedAction,
        selectedTargets,
        selectedActionIsSpell,
        hasSpellSummary,
        spellSummary,
        allTargetsInRange,
        adjudicationCount,
        _controller._actionAdjudicationReviewed,
        _controller._lastAttackResolved,
        _controller._lastAttackHit);
    const char *resolutionStepLabels[] = {
        localizedUiText("Choose Action"),
        localizedUiText("Choose Targets"),
        localizedUiText("Adjudicate"),
        localizedUiText("Roll / Save"),
        localizedUiText("Apply / Log")};
    UiStatusActionStrip actionFocusStrip;
    const std::string actionMode = battleFormat("Action Step {0}", {std::to_string(activeResolutionStep + 1)});
    const std::string actionSelection = resolutionStepLabels[std::clamp(activeResolutionStep, 0, 4)];
    std::string actionNext = battleText("Use the active section below; completed or optional sections stay available for review.");
    if (activeResolutionStep == 0)
        actionNext = battleText("Choose an action or define a manual action.");
    else if (activeResolutionStep == 1)
        actionNext = battleText("Select one or more valid targets.");
    else if (activeResolutionStep == 2)
        actionNext = battleText("Review GM adjudication and continue to the roll or save step.");
    else if (activeResolutionStep == 3)
        actionNext = battleText(selectedActionIsSpell ? "Enter spell roll or target saves, then resolve the cast." : "Enter the attack roll and check the hit.");
    else if (activeResolutionStep == 4)
        actionNext = battleText(selectedActionIsSpell ? "Apply or log the spell outcome." : "Apply damage if the checked attack hit.");
    actionFocusStrip.mode = actionMode.c_str();
    actionFocusStrip.selection = actionSelection.c_str();
    actionFocusStrip.nextAction = actionNext.c_str();
    actionFocusStrip.help = "Action Resolution is step-focused: the guide marks the current step and the controls below keep blocked actions disabled with reasons.";
    drawStatusActionStrip("BattleTrackerActionFocusStrip", actionFocusStrip);
