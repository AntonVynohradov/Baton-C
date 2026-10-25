/**
 * @file  baton_sim.h
 * @brief Bus simulation used by the unit tests and by the multi-seed runner.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef BATON_SIM_H
#define BATON_SIM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Seed of the pseudo-random generator when none is given. */
#define BATON_SIM_DEFAULT_SEED 0x12345678u

void baton_sim_seed(uint32_t seed);
int baton_sim_api_tests(void);
int baton_sim_queue_fuzz(void);
int baton_sim_scenario_count(void);
const char* baton_sim_scenario_name(int index);
int baton_sim_run_scenario(int index);
/** Run everything with @p seed; returns the number of problems (0 = pass). */
int baton_sim_run_all(uint32_t seed);

#ifdef __cplusplus
}
#endif

#endif /* BATON_SIM_H */
