#pragma once
#include "Controllers/Editor/Widgets/LiveSessionDashboard/LiveSessionDashboardController.hpp"
#include "Controllers/Editor/Widgets/GMScreen/GMScreenController.hpp"
#include "Models/Project/ProjectEventBus.hpp"

// Calls real text-only production surfaces; no Application/native window.
struct SessionTargetProbe
{
    static void lifecycle(LiveSessionDashboardController &c){c.configure();c._view->drawSessionLifecycleControls();}
    static void legacy(GMScreenController &c){c._view->drawSessionFlow();}
    static void recap(LiveSessionDashboardController &c){c.configure();c._view->drawGmPrepWrapTools();}
    static void notes(LiveSessionDashboardController &c){c.configure();c._view->drawGmSessionNotes();}
    static void phases(LiveSessionDashboardController &c){c.configure();c._view->drawPhaseSelector();}
    static void configure(LiveSessionDashboardController &c){c.configure();}
    static void capture(LiveSessionDashboardController &c){c.reviewSessionStart();}
    static void capture(GMScreenController &c){c.reviewSessionStart();}
    static bool confirm(LiveSessionDashboardController &c){return c.startSession();}
    static bool confirm(GMScreenController &c){return c.startSession();}
    static const SessionPlan::StartReview &start(const LiveSessionDashboardController &c){return c._sessionReview.start;}
    static const SessionPlan::StartReview &start(const GMScreenController &c){return c._sessionStartReview;}
    static SessionReport::Owner &owner(LiveSessionDashboardController &c){return c._sessionReview;}
    static const std::string &output(const LiveSessionDashboardController &c){return c._postSessionReportDraft;}
    static bool generate(LiveSessionDashboardController &c){return c.generatePostSessionReport();}
    static bool completed(LiveSessionDashboardController &c,const EntityId &id={}){return c.viewCompletedRecap(id);}
    static void current(LiveSessionDashboardController &c){c.useCurrentRecap();}
    static void seedGm(GMScreenController &c){c._sessionTitleDraft="REVOKED_TITLE";c._sessionOperationError="REVOKED_ERROR";}
    static bool gmReset(const GMScreenController &c){return !c._sessionStartReview.valid&&c._sessionTitleDraft.empty()&&c._sessionOperationError.empty();}
    static const std::string &recapDraft(const LiveSessionDashboardController &c){return c._recapDraft;}
    static const std::string &pendingNote(const LiveSessionDashboardController &c){return c._sessionNoteDraft;}
};

struct SessionEventCounter
{
    EntityId campaign; int saves=0,started=0,ended=0,subscription;
    explicit SessionEventCounter(const EntityId &id):campaign(id)
    {
        subscription=ProjectEventBus::getInstance().subscribe([this](const ProjectEvent &event){
            if(event.campaignId!=campaign)return;
            if(event.type==ProjectEventType::CampaignSaved)++saves;
            if(event.type==ProjectEventType::SessionStarted)++started;
            if(event.type==ProjectEventType::SessionEnded)++ended;
        });
    }
    ~SessionEventCounter(){ProjectEventBus::getInstance().unsubscribe(subscription);}
};
