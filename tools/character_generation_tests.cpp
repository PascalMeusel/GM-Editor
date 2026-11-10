#include "Models/Character/CharacterDraftGenerator.hpp"
#include "Models/Character/PortraitArtwork.hpp"
#include <iostream>
#include <stdexcept>

void require(bool ok,const char *message) { if(!ok) throw std::runtime_error(message); }
int main()
{
    try {
        for(int identity=0;identity<8;++identity)
        {
            const auto cell=gm::portraits::artworkRegion(identity);
            require(cell.u0>=0 && cell.v0>=0 && cell.u1<=1 && cell.v1<=1 && cell.u0<cell.u1 && cell.v0<cell.v1,"Painted atlas crop is invalid");
            require(cell.u0 >= (identity%4)*.25f && cell.u1 <= (identity%4+1)*.25f && cell.v0 >= (identity/4)*.5f && cell.v1 <= (identity/4+1)*.5f,"Identity crop crossed into another cell");
        }
        EntityMetadata legacy;
        legacy.customProperties["portrait.recipe"]="1";
        legacy.customProperties["portrait.species"]="2";
        legacy.customProperties["portrait.expression"]="6";
        require(gm::portraits::read(legacy).collection==0 && gm::portraits::read(legacy).expression==6,
                "A saved portrait without a collection was silently restyled");
        legacy.customProperties["portrait.collection"]="invalid";
        require(gm::portraits::read(legacy).collection==0,"Malformed saved collection lost legacy fallback");
        require(gm::portraits::read(EntityMetadata{}).collection==gm::portraits::currentCollection,
                "New portraits did not select the current collection");
        for(int collection=0;collection<=gm::portraits::currentCollection;++collection)
        for(int species=0;species<gm::portraits::speciesCount;++species)
            for(int expression=0;expression<8;++expression)
            {
                EntityMetadata metadata;
                metadata.customProperties["unrelated"]="keep";
                gm::portraits::Recipe recipe;
                recipe.species=species;recipe.expression=expression;recipe.collection=collection;
                gm::portraits::vary(recipe);
                gm::portraits::write(metadata,recipe);
                const auto loaded=gm::portraits::read(metadata);
                require(loaded.species==species && loaded.expression==expression && loaded.collection==collection && loaded.hairStyle==recipe.hairStyle && loaded.beard==recipe.beard,"Portrait round-trip failed");
                require(metadata.customProperties.at("unrelated")=="keep","Portrait overwrote unrelated metadata");
                require(gm::portraits::speciesForRace(gm::portraits::speciesNames[species])==species,"Catalog mapping failed");
            }
        gm::creation::Generator a(42),b(42);
        auto first=a.npc({"Gnome"}),second=b.npc({"Gnome"});
        require(first.name==second.name && first.goals==second.goals && first.metadata.customProperties==second.metadata.customProperties,"Seed did not reproduce creative content");
        require(first.metadata.id!=second.metadata.id,"Reproducing content reused an entity ID");
        require(a.uniqueName("Mira",{"Mira","Mira 2"})=="Mira 3","Collision handling failed");
        CharacterModel player;
        player.selectedRace="Goliath";player.selectedClass="Wizard";player.level=7;
        player.maxHitPoints=33;player.currentHitPoints=11;player.attributes["Intelligence"]=18;
        player.gmNotes="Private note";
        const auto id=player.metadata.id;
        a.playerIdentity(player);
        require(player.metadata.id==id && player.selectedClass=="Wizard" && player.level==7 && player.maxHitPoints==33 && player.currentHitPoints==11 && player.attributes.at("Intelligence")==18 && player.gmNotes=="Private note","Identity suggestion overwrote gameplay or private fields");
        require(gm::portraits::read(player.metadata).species==10,"Player race did not select matching portrait");
        EntityMetadata malformed;
        malformed.customProperties["portrait.species"]="999999999999999999";
        malformed.customProperties["portrait.expression"]="oops";
        require(gm::portraits::read(malformed).species==0 && gm::portraits::read(malformed).expression==0,"Malformed recipe failed to recover");
        std::cout<<"Two collections / 752 saved portrait slots, legacy compatibility, generation determinism, collisions and player field preservation passed\n";
        return 0;
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
