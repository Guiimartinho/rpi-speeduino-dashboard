/**
 * @file test_scope_guard.cpp
 * @brief Unit tests for scope guard utilities
 */

#include <gtest/gtest.h>
#include "common/scope_guard.hpp"
#include <string>

using namespace speeduino;

TEST(ScopeGuardTest, ExecutesOnScopeExit) {
    bool executed = false;

    {
        ScopeGuard guard([&executed]() { executed = true; });
        EXPECT_FALSE(executed);
    }

    EXPECT_TRUE(executed);
}

TEST(ScopeGuardTest, DismissPreventsExecution) {
    bool executed = false;

    {
        ScopeGuard guard([&executed]() { executed = true; });
        guard.dismiss();
    }

    EXPECT_FALSE(executed);
}

TEST(ScopeGuardTest, MoveConstructor) {
    bool executed = false;

    {
        ScopeGuard guard1([&executed]() { executed = true; });
        ScopeGuard guard2(std::move(guard1));
        // guard1 should be dismissed after move
    }

    // Only one execution should happen
    EXPECT_TRUE(executed);
}

// Note: Move assignment is intentionally deleted in ScopeGuard
// to prevent accidental resource leaks. Only move construction is allowed.

TEST(ScopeExitTest, ExecutesOnNormalExit) {
    bool executed = false;

    {
        ScopeExit onExit([&executed]() { executed = true; });
        EXPECT_FALSE(executed);
    }

    EXPECT_TRUE(executed);
}

TEST(ScopeExitTest, ExecutesOnException) {
    bool executed = false;

    try {
        ScopeExit onExit([&executed]() { executed = true; });
        throw std::runtime_error("test");
    } catch (...) {
        // Expected
    }

    EXPECT_TRUE(executed);
}

TEST(ScopeFailTest, ExecutesOnException) {
    bool executed = false;

    try {
        ScopeFail onFail([&executed]() { executed = true; });
        throw std::runtime_error("test");
    } catch (...) {
        // Expected
    }

    EXPECT_TRUE(executed);
}

TEST(ScopeFailTest, DoesNotExecuteOnNormalExit) {
    bool executed = false;

    {
        ScopeFail onFail([&executed]() { executed = true; });
        // No exception thrown
    }

    EXPECT_FALSE(executed);
}

TEST(ScopeSuccessTest, ExecutesOnNormalExit) {
    bool executed = false;

    {
        ScopeSuccess onSuccess([&executed]() { executed = true; });
        // No exception thrown
    }

    EXPECT_TRUE(executed);
}

TEST(ScopeSuccessTest, DoesNotExecuteOnException) {
    bool executed = false;

    try {
        ScopeSuccess onSuccess([&executed]() { executed = true; });
        throw std::runtime_error("test");
    } catch (...) {
        // Expected
    }

    EXPECT_FALSE(executed);
}

TEST(ScopeExitMacroTest, BasicUsage) {
    int counter = 0;

    {
        SCOPE_EXIT { counter++; };
        EXPECT_EQ(counter, 0);
    }

    EXPECT_EQ(counter, 1);
}

TEST(ScopeExitMacroTest, MultipleGuards) {
    std::string order;

    {
        SCOPE_EXIT { order += "1"; };
        SCOPE_EXIT { order += "2"; };
        SCOPE_EXIT { order += "3"; };
    }

    // Should execute in reverse order (LIFO)
    EXPECT_EQ(order, "321");
}

TEST(ScopeFailMacroTest, BasicUsage) {
    bool executed = false;

    try {
        SCOPE_FAIL { executed = true; };
        throw std::runtime_error("test");
    } catch (...) {
        // Expected
    }

    EXPECT_TRUE(executed);
}

TEST(ScopeSuccessMacroTest, BasicUsage) {
    bool executed = false;

    {
        SCOPE_SUCCESS { executed = true; };
    }

    EXPECT_TRUE(executed);
}

TEST(ScopeGuardTest, ResourceCleanupPattern) {
    struct Resource {
        bool& released;
        explicit Resource(bool& r) : released(r) { released = false; }
    };

    bool released = false;

    {
        Resource* res = new Resource(released);
        SCOPE_EXIT { delete res; res->released = true; };
        // Use resource...
        EXPECT_FALSE(released);
    }

    EXPECT_TRUE(released);
}

TEST(ScopeGuardTest, TransactionPattern) {
    int value = 0;
    bool committed = false;

    // Simulates a transaction that can be committed or rolled back
    auto transaction = [&value, &committed]() {
        int oldValue = value;
        value = 42;  // Tentative change

        ScopeGuard rollback([&]() {
            if (!committed) {
                value = oldValue;  // Rollback
            }
        });

        // Simulate success
        committed = true;
        rollback.dismiss();  // No rollback needed
    };

    transaction();
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(committed);
}
