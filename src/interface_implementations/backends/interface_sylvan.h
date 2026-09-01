/**
 * @file interface_sylvan.h
 * Optional Sylvan backend (original MEDUSA MTBDD package).
 *
 * MoToBuddy is the preferred product (`make` / interface_motobuddy.h). This
 * header is used only for `make sylvan_doubles` / `sylvan_gmp` (C path, no
 * MOSF). qBDD is Sylvan's MTBDD. Leaf files still include Buddy names (bdd.h,
 * mtbdd_maketerminal, ...); those shims live here so sim/gates/leaves stay
 * unchanged.
 */

#ifndef INTERFACE_SYLVAN_H
#define INTERFACE_SYLVAN_H

#include <sylvan.h>

#ifndef QBDD_TYPE_DEFINED
#define QBDD_TYPE_DEFINED
typedef MTBDD qBDD;
typedef MTBDD BDD;
#endif

typedef uint32_t mtbdd_terminal_type;

#include "interface_bdd_core.h"
#include "interface_leaf.h"
#include "interface_gate_ops.h"
#include "interface_prob.h"
#include "interface_norm.h"
#include "interface_symb.h"
#include "interface_lifecycle.h"

extern LEAF_TYPE clonePimpl(LEAF_TYPE src);

extern mtbdd_terminal_type lt_classic, lt_symb_map, lt_symb_val;

enum { BDD_MEMORY = 1 };

int bdd_varnum(void);
int bdd_setvarnum(int n);
qBDD bdd_ithvar(int i);
int bdd_error(int e);

qBDD mtbdd_maketerminal(void *valuep, mtbdd_terminal_type type);
qBDD mtbdd_cube2(int assignment, int width, qBDD *variables, qBDD leaf1, qBDD leaf0);
void *mtbdd_getTerminalValue(qBDD terminal);

#ifndef bdd_false
#define bdd_false() mtbdd_false
#endif
#ifndef bdd_true
#define bdd_true() mtbdd_true
#endif

#endif /* INTERFACE_SYLVAN_H */
