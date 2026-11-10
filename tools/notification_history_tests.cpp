#include "workflow_test_database.hpp"
#include "Controllers/Editor/EditorWindowController.hpp"
#include "Views/Editor/EditorWindowView.hpp"
#include "Models/Project/ProjectLogger.hpp"
#include "Models/Project/ProjectEventBus.hpp"
#include <iostream>
#include <stdexcept>

struct NotificationHistoryProbe {
    static void collect(EditorWindowView &view) { view.collectToastMessages(); }
    static bool body(const EditorWindowView &view, const std::string &text) {
        for (const auto &toast : view._toasts) if (toast.body == text) return true;
        return false;
    }
    static void clear(EditorWindowView &view) { view._toasts.clear(); }
};
void require(bool ok,const char *message) { if (!ok) throw std::runtime_error(message); }
int main() {
    auto *context=ImGui::CreateContext();
    try {
        isolateWorkflowDatabase();
        {
            EditorWindowController editor;EditorWindowView view(editor);
            auto &logger=ProjectLogger::getInstance();auto &bus=ProjectEventBus::getInstance();
            for (int i=0;i<650;++i) {
                logger.info("generated",std::to_string(i));
                ProjectEvent event;event.type=ProjectEventType::EntityUpdated;bus.publish(event);
                NotificationHistoryProbe::collect(view);
            }
            require(logger.history().size()==300 && bus.history().size()==200,"History is no longer bounded");
            NotificationHistoryProbe::clear(view);
            logger.warning("generated","Latest warning after history saturation");
            ProjectEvent event;event.type=ProjectEventType::MapChanged;event.message="Latest map event after history saturation";bus.publish(event);
            NotificationHistoryProbe::collect(view);
            require(NotificationHistoryProbe::body(view,"Latest warning after history saturation"),"Warning lost after history saturation");
            require(NotificationHistoryProbe::body(view,event.message),"Event lost after history saturation");
            NotificationHistoryProbe::clear(view);NotificationHistoryProbe::collect(view);
            require(!NotificationHistoryProbe::body(view,event.message),"Previously consumed event repeated");
            ProjectEvent saved;saved.type=ProjectEventType::CampaignSaved;bus.publish(saved);
            NotificationHistoryProbe::collect(view);
            require(NotificationHistoryProbe::body(view,"Campaign data saved.") &&
                !NotificationHistoryProbe::body(view,"All changes saved"),"Save toast falsely promises all editor drafts saved");
            // A hidden consumer can also lag behind the whole retained window.
            for (int i=0;i<350;++i) logger.info("generated","silent");
            logger.error("generated","Failure after hidden history overflow");
            NotificationHistoryProbe::collect(view);
            require(NotificationHistoryProbe::body(view,"Failure after hidden history overflow"),"Lagging consumer missed newest failure");
        }
        ImGui::DestroyContext(context);std::cout<<"Bounded notification history tests passed\n";return 0;
    } catch(const std::exception &error) { ImGui::DestroyContext(context);std::cerr<<error.what()<<'\n';return 1; }
}
