/**
 * @file  baton_sim_main.c
 * @brief Command-line runner of the bus simulation: baton_sim [seed]
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>

#include "baton_sim.h"

int main(int argc, char** argv)
{
    uint32_t seed = BATON_SIM_DEFAULT_SEED;
    if (argc > 1)
    {
        seed = (uint32_t) strtoul(argv[1], NULL, 0);
    }
    return (baton_sim_run_all(seed) != 0) ? 1 : 0;
}
