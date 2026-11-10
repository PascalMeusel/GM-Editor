// Included inside the campaign history test namespace. Public manager API only.
// These callbacks model campaign replacement while a command/save is executing.
void managerDeferredClearCases()
{
    auto &history = ProjectCommandManager::getInstance();
    for (int mode = 0; mode < 4; ++mode)
    {
        history.clear();
        auto token = std::make_shared<int>(1);
        const std::weak_ptr<int> lifetime = token;
        bool called = false, aliveInCallback = false, immediatelyRevoked = false;
        bool nestedAccepted = true;
        int nestedMutations = 0;
        std::function<void()> clearCallback = [&, held = token] {
            called = true;
            history.clear();
            aliveInCallback = !lifetime.expired();
            immediatelyRevoked = !history.canUndo() && !history.canRedo() &&
                history.undoLabel().empty() && history.redoLabel().empty();
            nestedAccepted = history.recordApplied(std::make_unique<LambdaProjectCommand>(
                "Obsolete nested record", [] {}, [] {}));
            history.execute(std::make_unique<LambdaProjectCommand>("Obsolete nested edit",
                [&] { ++nestedMutations; }, [] {}));
            if (mode == 3) throw std::runtime_error("Context changed during undo");
        };
        auto command = std::make_unique<LambdaProjectCommand>("Reset fixture",
            (mode == 0 || mode == 2) ? clearCallback : std::function<void()>([] {}),
            (mode == 1 || mode == 3) ? clearCallback : std::function<void()>([] {}));
        clearCallback = {};
        token.reset();
        require(!lifetime.expired(), "Reset fixture did not bind command lifetime");
        if (mode == 0) history.execute(std::move(command));
        else
        {
            require(history.recordApplied(std::move(command)), "Could not seed reset fixture");
            if (mode == 2)
            {
                require(history.undo() && !lifetime.expired(), "Redo reset fixture lost its command early");
                require(history.redo(), "Completed redo was not reported after context reset");
            }
            else require(history.undo() == (mode == 1), "Reset callback returned wrong operation result");
        }
        require(called && aliveInCallback && immediatelyRevoked && !nestedAccepted && nestedMutations == 0,
            "Context reset destroyed its active callback or left stale actions available");
        require(lifetime.expired() && !history.canUndo() && !history.canRedo() &&
            !history.undo() && !history.redo(), "Context reset retained an obsolete command after unwinding");
        require(history.recordApplied(std::make_unique<LambdaProjectCommand>("Fresh context", [] {}, [] {})) &&
            history.undoLabel() == "Fresh context", "Deferred reset poisoned the next campaign history");
    }
    history.clear();

    // Post-operation notifications can also replace the campaign, after a
    // command has entered a stack. Revocation must survive each operation tail.
    for (int mode = 0; mode < 4; ++mode)
    {
        history.clear();
        if (mode >= 2)
        {
            require(history.recordApplied(std::make_unique<LambdaProjectCommand>("Event fixture", [] {}, [] {})),
                "Could not seed notification reset fixture");
            if (mode == 3) require(history.undo(), "Could not prepare redo notification reset");
        }
        int observed = 0;
        bool revokedInNotification = false;
        struct Subscription
        {
            int id;
            ~Subscription() { ProjectEventBus::getInstance().unsubscribe(id); }
        } subscription{ProjectEventBus::getInstance().subscribe([&](const ProjectEvent &event) {
            if (event.entityType != "command") return;
            ++observed;
            history.clear();
            revokedInNotification = !history.canUndo() && !history.canRedo() &&
                history.undoLabel().empty() && history.redoLabel().empty();
        })};
        if (mode == 0) history.execute(std::make_unique<LambdaProjectCommand>("Execute reset", [] {}, [] {}));
        else if (mode == 1)
            require(history.recordApplied(std::make_unique<LambdaProjectCommand>("Record reset", [] {}, [] {})),
                "Committed record notification reported mutation failure");
        else if (mode == 2) require(history.undo(), "Committed undo notification reported mutation failure");
        else require(history.redo(), "Committed redo notification reported mutation failure");
        require(observed == 1 && revokedInNotification && !history.canUndo() && !history.canRedo(),
            "An operation tail resurrected history after notification-time reset");
    }
    history.clear();
}
