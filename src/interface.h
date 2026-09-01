/**
 * @file interface.h
 * @brief Umbrella for the Medusa qBDD API.
 *
 * qBDD and leaf_primitive_t must already be defined. The Makefile does that
 * with -include of a leaf primitive header, then the backend header
 * (interface_motobuddy.h by default / preferred, or interface_sylvan.h).
 * Translation units still '#include "interface.h"' so the API is visible
 * without relying on compiler flags alone.
 */

#ifndef INTERFACE_H
#define INTERFACE_H

#include "interface_bdd_core.h"
#include "interface_leaf.h"
#include "interface_gate_ops.h"
#include "interface_prob.h"
#include "interface_norm.h"
#include "interface_symb.h"
#include "interface_lifecycle.h"

#endif /* INTERFACE_H */
