#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

class IProjectCommand
{
public:
    virtual ~IProjectCommand() = default;

    virtual std::string name() const = 0;
    virtual void execute() = 0;
    virtual void undo() = 0;
};

class LambdaProjectCommand : public IProjectCommand
{
public:
    LambdaProjectCommand(std::string commandName, std::function<void()> executeCallback, std::function<void()> undoCallback);

    std::string name() const override { return _name; }
    void execute() override;
    void undo() override;

private:
    std::string _name;
    std::function<void()> _executeCallback;
    std::function<void()> _undoCallback;
};

class ProjectCommandManager
{
public:
    static ProjectCommandManager &getInstance();

    void execute(std::unique_ptr<IProjectCommand> command);
    // Register a mutation which already committed through its production service.
    bool recordApplied(std::unique_ptr<IProjectCommand> command);
    bool canUndo() const { return !_clearPending && !_undoStack.empty(); }
    bool canRedo() const { return !_clearPending && !_redoStack.empty(); }
    std::string undoLabel() const;
    std::string redoLabel() const;
    bool undo();
    bool redo();
    void clear();
    const std::string &lastError() const { return _lastError; }

private:
    struct Operation;
    std::vector<std::unique_ptr<IProjectCommand>> _undoStack;
    std::vector<std::unique_ptr<IProjectCommand>> _redoStack;
    std::string _lastError;
    bool _busy = false;
    bool _clearPending = false;
};
