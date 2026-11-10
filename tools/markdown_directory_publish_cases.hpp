// Real same-volume publication, without replacing any existing destination.
void markdownDirectoryPublish(const std::filesystem::path &root)
{
    const auto parent=root/std::filesystem::u8path("Veröffentlichung-灯");
    const auto stage=parent/std::filesystem::u8path("Bühne-α");
    write(stage/"nested/note.md","STAGED_BYTES\r\n");
    const auto empty=parent/"existing-empty";
    check(std::filesystem::create_directory(empty),"Could not create owned empty destination");
    const auto refused=[&](const std::filesystem::path &destination) {
        bool failed=false;
        try {ProjectDirectoryPublish::renameExclusive(stage,destination);}
        catch(const std::filesystem::filesystem_error &){failed=true;}
        check(failed&&bytes(stage/"nested/note.md")=="STAGED_BYTES\r\n",
            "No-replace publication consumed staging or accepted existing destination");
    };
    refused(empty);
    check(std::filesystem::is_directory(empty)&&std::filesystem::is_empty(empty),
        "No-replace publication changed existing empty directory");
    const auto file=parent/"existing-file";write(file,"DESTINATION_CANARY");refused(file);
    check(bytes(file)=="DESTINATION_CANARY","No-replace publication overwrote existing file");
    const auto full=parent/"existing-content";write(full/"keep.txt","NESTED_CANARY");refused(full);
    check(bytes(full/"keep.txt")=="NESTED_CANARY","No-replace publication changed existing directory");
    const auto destination=parent/std::filesystem::u8path("Fertig-β");
    ProjectDirectoryPublish::renameExclusive(stage,destination);
    check(!std::filesystem::exists(stage)&&bytes(destination/"nested/note.md")=="STAGED_BYTES\r\n"&&
        bytes(file)=="DESTINATION_CANARY"&&std::filesystem::is_empty(empty),
        "Unicode same-volume publication changed bytes or unrelated paths");
}
