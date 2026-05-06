/**
 * @file interface_motobuddy.h
 * Backend-specific setup for MoToBuddy.
 */

#ifndef INTERFACE_MOTOBUDDY_H
#define INTERFACE_MOTOBUDDY_H

#include "bdd.h"

#ifndef QBDD_TYPE_DEFINED
#define QBDD_TYPE_DEFINED
typedef BDD qBDD;
#endif

#include "mtbdd.h"

#include "interface_bdd_core.h"
#include "interface_leaf.h"
#include "interface_gate_ops.h"
#include "interface_prob.h"
#include "interface_norm.h"
#include "interface_symb.h"
#include "interface_lifecycle.h"

extern LEAF_TYPE clonePimpl(LEAF_TYPE src);

#endif /* INTERFACE_MOTOBUDDY_H */