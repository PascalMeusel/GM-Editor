#pragma once
#include "Controllers/Editor/EditorMarkdownExchange.hpp"
#include <set>

class ExchangePanel
{
public:
    void drawLauncher(EditorMarkdownExchange &);
    void reset();
private:
    using Kind=EditorMarkdownExchange::Kind;
    using Snapshot=EditorMarkdownExchange::Snapshot;
    Kind _tab=Kind::Export;
    std::uint64_t _seenToken=0;
    std::string _destination,_source,_updateSource;
    std::set<std::string> _attachments;
    std::set<EntityId> _selectedNotes;
    EntityId _detailNote;
    int _detailVersion=2;
    bool _editable=false,_reviewed=false,_wasOpen=false,_closeRequested=false,_focusReturn=false;
    void revokeConsent();
    void keyboardReturn();
    void drawStatus(const EditorMarkdownExchange &,const Snapshot *);
    void drawExport(EditorMarkdownExchange &,const Snapshot *);
    void drawImport(EditorMarkdownExchange &,const Snapshot *);
    void drawUpdates(EditorMarkdownExchange &,const Snapshot *);
};
