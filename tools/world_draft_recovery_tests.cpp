#include "world_draft_recovery_tests_codec.hpp"
#include "world_draft_recovery_tests_storage.hpp"

int main(int argc,char **argv)
{
    using namespace world_recovery_test;
    try {
        if(argc==3){processMode(argv[1],fs::u8path(argv[2]));return 0;}
        require(argc==1,"Usage: world_draft_recovery_tests [--produce <new-root> | --consume <owned-root>]");
        const auto root=fs::temp_directory_path()/("gm-world-recovery-test-"+AtomicProjectFile::uniqueSuffix());
        require(fs::create_directory(root),"Cannot reserve isolated fixture root");isolate(root);
        std::cout<<"World recovery isolated fixture: "<<root.u8string()<<'\n';
        codecCases();limitCases();mergeCases();storageCases();
        // This is the unique directory reserved above; process modes preserve receipts.
        fs::remove_all(root);
        std::cout<<"World recovery: codec/limits, typed merge conflicts, explicit disk read/copy/remove, failure/retry and privacy scope passed.\n";return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
