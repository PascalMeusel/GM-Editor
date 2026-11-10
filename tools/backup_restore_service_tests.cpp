#include "backup_restore_state_cases.hpp"
#include "backup_restore_failure_cases.hpp"

int main()
{
    try {
        using namespace backup_restore_test;
        initialize();
        presenceReversals();
        staleSelections();
        corruptAndMismatchedReceipts();
        conservativeLegacy();
        sqlFailureAfterDisplacement();
        copySwapAndRollbackFailures();
        cleanupWarningAndLinkedSource();
        destructiveOwnershipGuards();
        // Reserved IDs intentionally poison the identity namespace: keep this last.
        reservedAndAliasedTargets();
        std::cout<<"PASS backup restore: 9 groups; real SQLite/files, discoverable reversals, failure retention; no UI/audio\n";
        return 0;
    } catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
