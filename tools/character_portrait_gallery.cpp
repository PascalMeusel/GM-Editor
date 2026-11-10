#include "Models/Character/CharacterDraftGenerator.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "Views/Editor/Widgets/UiPortrait.hpp"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

static void require(bool condition, const char *message) { if(!condition) throw std::runtime_error(message); }
static void bitmap(const std::filesystem::path &path, int width, int height)
{
    const int stride = (width*3+3)&~3;
    std::vector<unsigned char> pixels(stride*height);
    glPixelStorei(GL_PACK_ALIGNMENT,4);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    for(int y=0;y<height;++y) for(int x=0;x<width;++x)
        std::swap(pixels[y*stride+x*3],pixels[y*stride+x*3+2]);
    require(glGetError()==GL_NO_ERROR,"Framebuffer read failed");
    std::ofstream out(path,std::ios::binary);
    auto u16=[&](unsigned v) { out.put(char(v));out.put(char(v>>8)); };
    auto u32=[&](unsigned v) {u16(v);u16(v>>16);};
    out.write("BM",2);u32(54+unsigned(pixels.size()));u32(0);u32(54);u32(40);u32(width);u32(height);
    u16(1);u16(24);u32(0);u32(unsigned(pixels.size()));u32(2835);u32(2835);u32(0);u32(0);
    out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());
    require(bool(out),"Could not write gallery");
}
int main(int argc,char **argv)
{
    try {
        require(argc==2,"Expected a NEW output directory");
        const auto output=std::filesystem::absolute(argv[1]);
        require(!std::filesystem::exists(output),"Output directory already exists; refusing to overwrite");
        std::filesystem::create_directories(output);
#ifdef _WIN32
        _putenv_s("TTRPG_DATA_ROOT",output.string().c_str());
        _putenv_s("TTRPG_DB_PATH",(output/"TTRPG.db").string().c_str());
#endif
        Campaign campaign;
        campaign.name="Portrait and character creation - 100 examples";
        campaign.ruleSet=std::make_shared<DND5e>();
        std::vector<std::string> names;
        std::vector<EntityMetadata> portraits;
        std::set<std::pair<int,int>> portraitIdentities;
        std::ofstream report(output/"characters.md");
        report << "# 100 final character examples\n\n50 players and 50 NPCs. Creative drafts; player rules choices remain editable.\n\n";
        for(int i=0;i<100;++i)
        {
            gm::creation::Generator generator(7000u+i);
            // Both player and NPC examples cover every portrait species.
            const std::string race=gm::portraits::speciesNames[(i/2)%gm::portraits::speciesCount];
            std::string name;
            EntityMetadata metadata;
            if(i%2==0)
            {
                CharacterModel player;
                player.selectedRace=race;
                player.selectedClass="Fighter";
                generator.playerIdentity(player);
                name=generator.uniqueName(player.nameBuffer,names);
                std::snprintf(player.nameBuffer,sizeof(player.nameBuffer),"%s",name.c_str());
                for(const auto &attribute:campaign.ruleSet->getAttributes()) player.attributes[attribute]=10;
                normalizeCharacterHitPoints(player);
                metadata=player.metadata;
                campaign.characters.push_back(player);
                report << "## " << i+1 << ". " << name << " (player, " << race << ")\n\n" << player.background << ". " << player.backstory << "\n\n";
            }
            else
            {
                auto npc=generator.npc({race});
                name=generator.uniqueName(npc.name,names); npc.name=name;
                for(const auto &attribute:campaign.ruleSet->getAttributes()) npc.attributes[attribute]=8+generator.number(9);
                metadata=npc.metadata; campaign.npcs.push_back(npc);
                require(!npc.relationships.empty()&&!npc.dialogueNotes.empty(),"NPC has missing roleplay details");
                report << "## " << i+1 << ". " << name << " (NPC, " << race << ")\n\n" << npc.description << "\n\n" << npc.role << ": " << npc.goals << "\n\n" << npc.resources << "\n\n" << npc.threatLocation << "\n\nSecret: " << npc.secrets << "\n\n" << npc.relationships << "\n\n" << npc.dialogueNotes << "\n\n";
            }
            auto portrait=gm::portraits::read(metadata);
            portrait.expression=(i/(2*gm::portraits::speciesCount))*2+(i%2);
            gm::portraits::write(metadata,portrait);
            require(portraitIdentities.emplace(portrait.species,portrait.expression).second,"Duplicate portrait identity in final examples");
            if(i%2==0) campaign.characters.back().metadata=metadata;
            else campaign.npcs.back().metadata=metadata;
            require(gm::portraits::read(metadata).species==gm::portraits::speciesForRace(race),"Species mismatch");
            names.push_back(name); portraits.push_back(metadata);
        }
        require(std::set<std::string>(names.begin(),names.end()).size()==100,"Duplicate final names");
        require(portraitIdentities.size()==100,"Expected 100 distinct final portrait identities");
        CampaignRepository::saveCampaign(campaign);
        const auto loaded=CampaignRepository::loadAllCampaigns();
        require(loaded.size()==1 && loaded[0].characters.size()==50 && loaded[0].npcs.size()==50,"Persistence count mismatch");
        for(std::size_t i=0;i<50;++i)
        {
            const auto findMetadata=[&](const auto &items,const EntityMetadata &expected) {
                for(const auto &item:items) if(item.metadata.id==expected.id) return item.metadata.customProperties==expected.customProperties;
                return false;
            };
            require(findMetadata(loaded[0].characters,campaign.characters[i].metadata),"Player portrait changed after reload");
            require(findMetadata(loaded[0].npcs,campaign.npcs[i].metadata),"NPC portrait changed after reload");
        }
        require(glfwInit()!=0,"GLFW init failed");
        glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
        auto *window=glfwCreateWindow(1440,1280,"Portrait gallery",nullptr,nullptr);
        require(window!=nullptr,"Hidden gallery window failed"); glfwMakeContextCurrent(window);
        ImGui::CreateContext(); ImGui::GetIO().IniFilename=nullptr;
        ImGui_ImplGlfw_InitForOpenGL(window,true); ImGui_ImplOpenGL3_Init("#version 130");
        for(int species=0;species<gm::portraits::speciesCount;++species)
        {
            const auto &texture=getCachedImageTexture(PortraitUi::artworkPath(species));
            require(texture.loaded && texture.width>=1024 && texture.height>=1024,"Missing or undersized painted portrait sheet");
        }
        for(int page=0;page<5;++page)
        {
            const int canvasHeight=page==1?260:page==4?960:1280;
            glfwSetWindowSize(window,1440,canvasHeight);glfwPollEvents();
            ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImVec2(1440,float(canvasHeight)));
            ImGui::Begin("Portraits",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
            const int count=page==0?gm::portraits::speciesCount:page==1?8:page==4?10:50;
            for(int i=0;i<count;++i)
            {
                const int columns=page==1?8:page==4?5:10;
                const float pitchX=page==4?284.f:142.f;
                const float pitchY=page==4?465.f:245.f;
                ImGui::SetCursorPos(ImVec2(12.f+(i%columns)*pitchX,12.f+(i/columns)*pitchY));
                ImGui::BeginGroup();
                gm::portraits::Recipe r;
                if(page==0) {r.species=i;r.expression=(i*5)%8;}
                else if(page==1) {r.expression=i;r.species=1;}
                else if(page==4) {const int core[]={0,1,2,3,4,5,8,9,10,11};r.species=core[i];r.expression=i%8;}
                else r=gm::portraits::read(portraits[(page-2)*50+i]);
                const char *identities[] = {"Portrait 1", "Portrait 2", "Portrait 3", "Portrait 4", "Portrait 5", "Portrait 6", "Portrait 7", "Portrait 8"};
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+(page==4?265.f:130.f));
                ImGui::TextUnformatted(page==1?identities[i]:page==0||page==4?gm::portraits::speciesNames[r.species]:names[(page-2)*50+i].c_str());
                ImGui::PopTextWrapPos();
                PortraitUi::draw(r,page==4?256.f:128.f);
                ImGui::Text("S%d / P%d",r.species,r.expression+1);
                ImGui::EndGroup();
            }
            ImGui::End();ImGui::Render();
            int width,height;glfwGetFramebufferSize(window,&width,&height);
            glViewport(0,0,width,height);glClearColor(.08f,.09f,.12f,1);glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());glFinish();
            bitmap(output/(std::string("portraits-")+std::to_string(page)+".bmp"),width,height);
            glfwSwapBuffers(window);glfwPollEvents();
        }
        clearImageTextureCache();
        ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();
        std::cout << "100 unique characters saved and reloaded; 47 species and 376 distinct identities rendered.\n";
        return 0;
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
