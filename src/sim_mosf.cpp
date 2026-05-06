/**
 * @file sim_mosf.cpp
 * @brief Module performing quantum circuit simulation of MOSF circuits using tree traversal
 * 
 * @warning Requires the MoToBuDDy backend, especially the MOSF parser
 * @warning C++
 */

#include "sim_mosf.h"
#include "../lib/MoToBuddy/src/mosf_parser.h"
#include "interface.h"



extern "C" bool sim_mosf_file(FILE *in, qBDD *circ, const sim_flags_t *flags, sim_info_t *info) {
    if (!in || !circ || !flags || !info) return false;

    printf("Parsing MOSF file and simulating...\n");
    try {
        mosf::json j = mosf::json::parse(in);
        info->n_qubits = j.at("x_levels").get<int>();
        circuit_init_interface(circ, info->n_qubits);
        mosf::ExtensionRegistry reg;

        reg.node_ops["neg"] = [](const mosf::json& j) -> NodeOp {
            return [](BDD node) -> BDD {
                qBDD res = unary_apply(node, invertLeaf);
                return res;
            };
        };

        reg.node_ops["i_mul"] = [](const mosf::json& j) -> NodeOp {
            return [](BDD node) -> BDD {
                qBDD res = unary_apply(node, rotateCoef2);
                return res;
            };
        };

        reg.node_ops["neg_i_mul"] = [](const mosf::json& j) -> NodeOp {
            return [](BDD node) -> BDD {
                qBDD res = unary_apply(node, negI_mul);
                return res;
            };
        };

        reg.node_ops["phase_mul"] = [](const mosf::json& j) -> NodeOp {
            double theta = j.at("phase").get<double>();

            constexpr double PI = std::acos(-1.0);
            constexpr double EPS = 1e-12;

            // Fast paths -- avoid cos/sin entirely
            if (std::abs(theta) < EPS) {
                return [](BDD node) -> BDD { return node; };
            }
            if (std::abs(theta - PI/2) < EPS) {
                return [](BDD node) -> BDD {
                    qBDD res = unary_apply(node, rotateCoef2);
                    return res;
                };
            }

            if (std::abs(theta + PI/2) < EPS) {
                return [](BDD node) -> BDD {                 
                    qBDD res = unary_apply(node, negI_mul);
                    return res;
                };
            }

            if (std::abs(std::abs(theta) - PI) < EPS) {
                return [](BDD node) -> BDD {
                    qBDD res = unary_apply(node, invertLeaf);
                    return res;
                };
            }

            // General path -- compute once, capture by value
            double c = std::cos(theta);
            double s = std::sin(theta);

            return [c, s](BDD node) -> BDD {
                PhaseParam *p = (PhaseParam*)malloc(sizeof(PhaseParam));
                p->c = c;
                p->s = s;
                qBDD res = unary_apply_param(node, mulPhaseLeaf, (size_t)(p));
                free(p);
                return res;
            };
        };

        reg.node_ops["rx"] = [](const mosf::json& j) -> NodeOp {
            const std::string angle_expr = j.at("angle").get<std::string>();
            return [angle_expr](BDD node) -> BDD {
                // TODO:
                // Apply Rx(theta) at current node:
                // new_low  = cos(theta/2) * low  - i sin(theta/2) * high
                // new_high = -i sin(theta/2) * low + cos(theta/2) * high
                throw std::runtime_error("rx not implemented");
            };
        };

        reg.node_ops["ry"] = [](const mosf::json& j) -> NodeOp {
            const std::string angle_expr = j.at("angle").get<std::string>();
            return [angle_expr](BDD node) -> BDD {
                // TODO:
                // Apply Ry(theta) at current node:
                // new_low  = cos(theta/2) * low - sin(theta/2) * high
                // new_high = sin(theta/2) * low + cos(theta/2) * high
                throw std::runtime_error("ry not implemented");
            };
        };

        reg.node_ops["rz"] = [](const mosf::json& j) -> NodeOp {
            const std::string angle_expr = j.at("angle").get<std::string>();
            return [angle_expr](BDD node) -> BDD {
                // TODO:
                // Apply Rz(theta) at current node:
                // new_low  = exp(-i theta/2) * low
                // new_high = exp(+i theta/2) * high
                throw std::runtime_error("rz not implemented");
            };
        };

        reg.binary_ops["plus_mulsqrt2"] = [](const mosf::json& j) -> BinaryNodeOp {
            return [](BDD low, BDD high) -> BDDPair {
                BDD out = binary_apply(low, high, addLeafS);
                return BDDPair{out, 0};
            };
        };

        reg.binary_ops["minus_mulsqrt2"] = [](const mosf::json& j) -> BinaryNodeOp {
            return [](BDD low, BDD high) -> BDDPair {
                qBDD_protect(low);
                qBDD_protect(high);

                BDD out = binary_apply(low, high, subLeafS);  // (low - high) / sqrt(2)

                qBDD_unprotect(low);
                qBDD_unprotect(high);
                return BDDPair{out, 0};
            };
        };

        mosf::MOSFFile mf = mosf::parse(j, reg);

        while (mf.has_next()) {
            mosf::Event ev = mf.next(); // unrolled for now, add symbolic later
            qBDD_protect(*circ);
            qBDD res = ev.op(*circ);
            qBDD_unprotect(*circ);
            *circ = res;
        }

        return true;
    } catch (const std::exception& e) {
        fprintf(stderr, "Error simulating MOSF file: %s\n", e.what());
        return false;
    }
}