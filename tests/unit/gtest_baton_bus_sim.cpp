/**
 * @file gtest_baton_bus_sim.cpp
 * @brief Bus simulation as unit tests: several nodes on one simulated single-wire bus.
 *
 * Every scenario checks exactly-once in-order delivery per (source, destination,
 * priority), final state delivery, no collisions and the slot budget. The same code
 * runs optimized for many seeds through the baton_sim executable.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "baton_sim.h"

TEST(BusSim, ApiChecks)
{
    EXPECT_EQ(baton_sim_api_tests(), 0);
}

TEST(BusSim, QueueFuzz)
{
    baton_sim_seed(BATON_SIM_DEFAULT_SEED);
    EXPECT_EQ(baton_sim_queue_fuzz(), 0);
}

class BusSimScenario : public ::testing::TestWithParam<int>
{
};

TEST_P(BusSimScenario, DeliversEverythingExactlyOnce)
{
    baton_sim_seed(BATON_SIM_DEFAULT_SEED + static_cast<uint32_t>(GetParam()));
    EXPECT_EQ(baton_sim_run_scenario(GetParam()), 0) << baton_sim_scenario_name(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Scenarios, BusSimScenario,
                         ::testing::Range(0, baton_sim_scenario_count()));
