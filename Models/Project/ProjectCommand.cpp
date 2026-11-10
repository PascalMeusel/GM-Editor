#include "ProjectCommand.hpp"
#include "Models/Project/ProjectEventBus.hpp"
#include "Models/Project/ProjectLogger.hpp"

#include <utility>
#include <exception>

namespace
{
void notifyCommand(const std::string &message)
{
    try { ProjectEventBus::getInstance().publish(ProjectEvent{ProjectEventType::EntityUpdated, {}, {}, "command", message}); }
    catch (...) {} // Notifications cannot undo an already committed command.
}
}

// A campaign may be replaced by a synchronous save/event callback while a
// command is on its own call stack. Revoke labels immediately, but destroy the
// command only after that callback has returned; clearing in-place would delete
// the executing command, while ignoring clear would retain stale history.
struct ProjectCommandManager::Operation
{
    ProjectCommandManager &owner;
    explicit Operation(ProjectCommandManager &manager) : owner(manager) { owner._busy = true; }
    ~Operation()
    {
        owner._busy = false;
        if (owner._clearPending) owner.clear();
    }
};

LambdaProjectCommand::LambdaProjectCommand(std::string commandName, std::function<void()> executeCallback, std::function<void()> undoCallback)
    : _name(std::move(commandName)), _executeCallback(std::move(executeCallback)), _undoCallback(std::move(undoCallback))
{
}

void LambdaProjectCommand::execute()
{
    _executeCallback();
}

void LambdaProjectCommand::undo()
{
    _undoCallback();
}

ProjectCommandManager &ProjectCommandManager::getInstance()
{
    static ProjectCommandManager manager;
    return manager;
}

void ProjectCommandManager::execute(std::unique_ptr<IProjectCommand> command)
{
    if (_busy) return;
    _lastError.clear();
    if (!command) return;
    Operation scope(*this);
    try
    {
        _undoStack.reserve(_undoStack.size() + 1);
        const auto message = "Applied command: " + command->name();
        if (_clearPending) return;
        command->execute();
        if (_clearPending) return;
        _undoStack.push_back(std::move(command));
        if (_undoStack.size() > 100) _undoStack.erase(_undoStack.begin());
        _redoStack.clear();
        notifyCommand(message);
    }
    catch (const std::exception &error) { _lastError = error.what(); }
    catch (...) { _lastError = "The command could not be applied."; }
}

bool ProjectCommandManager::recordApplied(std::unique_ptr<IProjectCommand> command)
{
    if (_busy) return false;
    _lastError.clear();
    if (!command) return false;
    Operation scope(*this);
    try
    {
        const auto message = "Applied command: " + command->name();
        if (_clearPending) return false;
        _undoStack.push_back(std::move(command));
        if (_undoStack.size() > 100) _undoStack.erase(_undoStack.begin());
        _redoStack.clear();
        notifyCommand(message);
        return true;
    }
    catch (const std::exception &error) { _lastError = error.what(); }
    catch (...) { _lastError = "The applied change could not be added to history."; }
    return false;
}

std::string ProjectCommandManager::undoLabel() const
{
    return canUndo() ? _undoStack.back()->name() : std::string{};
}

std::string ProjectCommandManager::redoLabel() const
{
    return canRedo() ? _redoStack.back()->name() : std::string{};
}

bool ProjectCommandManager::undo()
{
    if (_busy) return false;
    _lastError.clear();
    if (!canUndo()) return false;
    Operation scope(*this);
    try
    {
        _redoStack.reserve(_redoStack.size() + 1);
        const auto message = "Undid command: " + _undoStack.back()->name();
        if (_clearPending) return false;
        _undoStack.back()->undo();
        if (_clearPending) return true;
        _redoStack.push_back(std::move(_undoStack.back()));
        _undoStack.pop_back();
        notifyCommand(message);
        return true;
    }
    catch (const std::exception &error) { _lastError = error.what(); }
    catch (...) { _lastError = "The change could not be undone."; }
    return false;
}

bool ProjectCommandManager::redo()
{
    if (_busy) return false;
    _lastError.clear();
    if (!canRedo()) return false;
    Operation scope(*this);
    try
    {
        _undoStack.reserve(_undoStack.size() + 1);
        const auto message = "Redid command: " + _redoStack.back()->name();
        if (_clearPending) return false;
        _redoStack.back()->execute();
        if (_clearPending) return true;
        _undoStack.push_back(std::move(_redoStack.back()));
        _redoStack.pop_back();
        notifyCommand(message);
        return true;
    }
    catch (const std::exception &error) { _lastError = error.what(); }
    catch (...) { _lastError = "The change could not be redone."; }
    return false;
}

void ProjectCommandManager::clear()
{
    if (_busy) { _clearPending = true; return; }
    _clearPending = false;
    _undoStack.clear();
    _redoStack.clear();
    _lastError.clear();
    try { ProjectLogger::getInstance().debug("ProjectCommandManager", "Command history cleared."); } catch (...) {}
}
