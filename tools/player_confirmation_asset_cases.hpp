void compactConfirmationAssets(const fs::path &root)
{
    using PlayerDisplayReview::ownConfirmation;
    const auto directory=root/"Resources/Campaigns/compact-confirmation/Assets";
    const std::array<std::string,3> contents{{"HANDOUT_ORIGINAL","INTERACTIVE_MAP","EXPORT_ORIGINAL"}};
    std::array<fs::path,3> paths;std::array<std::string,3> relative,hashes;
    for(std::size_t i=0;i<paths.size();++i){paths[i]=directory/(std::to_string(i)+".png");write(paths[i],contents[i]);
        relative[i]=fs::relative(paths[i],root).generic_u8string();hashes[i]=PlayerDisplayReview::handoutContentHash(paths[i]);}
    PlayerDisplayReviewSnapshot review;review.eligible=review.hasMapPreview=review.playerMapPlan.backgroundVisible=true;
    PlayerSafeHandout handout;handout.projectRelativePath=relative[0];handout.contentHash=hashes[0];review.playerHandouts.push_back(handout);
    review.playerMapPlan.backgroundInteractiveProjectRelativePath=relative[1];review.playerMapPlan.backgroundInteractiveContentHash=hashes[1];
    review.playerMapPlan.backgroundExportProjectRelativePath=relative[2];review.playerMapPlan.backgroundExportContentHash=hashes[2];
    const auto compact=ownConfirmation(review);require(compact.images.size()==3,"Compact confirmation omitted one reviewed image channel");
    auto destroyed=review;const auto independent=ownConfirmation(destroyed);destroyed={};
    PlayerDisplayPublicationAssets publication;require(publication.capture(independent),"Owned compact asset capture depended on destroyed snapshot");
    for(std::size_t i=0;i<paths.size();++i)require(bytes(publication.path(relative[i],hashes[i]))==contents[i],
        "Compact capture changed handout or interactive/export background bytes");
    auto duplicate=review;duplicate.playerHandouts.push_back(handout);
    require(publication.capture(duplicate)&&publication.capture(ownConfirmation(duplicate)),"Identical duplicate asset binding was rejected");
    for(int change=0;change<7;++change){auto bad=review;
        if(change==0)bad.eligible=false;
        if(change==1){auto conflict=handout;conflict.contentHash="fnv1a64:0000000000000001";bad.playerHandouts.push_back(conflict);}
        if(change==2)bad.playerHandouts[0].projectRelativePath="../outside.png";
        if(change==3)bad.playerHandouts[0].contentHash.clear();
        if(change==4)bad.playerHandouts[0].projectRelativePath=relative[0]+".missing";
        if(change==5)bad.playerMapPlan.backgroundInteractiveContentHash="fnv1a64:0000000000000001";
        if(change==6)bad.playerMapPlan.backgroundExportContentHash="fnv1a64:0000000000000001";
        require(publication.capture(compact),"Cannot restore positive capture before refusal");
        const auto old=publication.image(relative[0],hashes[0]);
        require(!publication.capture(bad)&&!old.valid()&&!publication.capture(ownConfirmation(bad))&&
            publication.path(relative[0],hashes[0]).empty(),"Snapshot/compact capture accepted unsafe, missing or conflicting reviewed bytes");
    }
    const auto modified=fs::last_write_time(paths[0]);const auto size=fs::file_size(paths[0]);
    write(paths[0],"HANDOUT_REPLACED");fs::last_write_time(paths[0],modified);
    require(fs::file_size(paths[0])==size&&!publication.capture(review)&&!publication.capture(compact),
        "Compact capture accepted same-size/mtime changed source bytes");
    write(paths[0],contents[0]);
    require(publication.capture(compact)&&bytes(publication.path(relative[0],hashes[0]))==contents[0],
        "Exact source restoration could not be reviewed again through compact capture");
}
