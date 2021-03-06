/*
 *  yosys -- Yosys Open SYnthesis Suite
 *
 *  Copyright (C) 2012  Clifford Wolf <clifford@clifford.at>
 *  
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "opt_status.h"
#include "kernel/register.h"
#include "kernel/sigtools.h"
#include "kernel/celltypes.h"
#include "kernel/log.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <set>

static bool did_something;

void replace_undriven(RTLIL::Design *design, RTLIL::Module *module)
{
	CellTypes ct(design);
	SigMap sigmap(module);
	SigPool driven_signals;
	SigPool used_signals;
	SigPool all_signals;

	for (auto &it : module->cells)
	for (auto &conn : it.second->connections) {
		if (!ct.cell_known(it.second->type) || ct.cell_output(it.second->type, conn.first))
			driven_signals.add(sigmap(conn.second));
		if (!ct.cell_known(it.second->type) || ct.cell_input(it.second->type, conn.first))
			used_signals.add(sigmap(conn.second));
	}

	for (auto &it : module->wires) {
		if (it.second->port_input)
			driven_signals.add(sigmap(it.second));
		if (it.second->port_output)
			used_signals.add(sigmap(it.second));
		all_signals.add(sigmap(it.second));
	}

	all_signals.del(driven_signals);
	RTLIL::SigSpec undriven_signals = all_signals.export_all();

	for (auto &c : undriven_signals.chunks)
	{
		RTLIL::SigSpec sig = c;

		if (c.wire->name[0] == '$')
			sig = used_signals.extract(sig);
		if (sig.width == 0)
			continue;

		log("Setting undriven signal in %s to undef: %s\n", RTLIL::id2cstr(module->name), log_signal(c));
		module->connections.push_back(RTLIL::SigSig(c, RTLIL::SigSpec(RTLIL::State::Sx, c.width)));
		OPT_DID_SOMETHING = true;
	}
}

void replace_cell(RTLIL::Module *module, RTLIL::Cell *cell, std::string info, std::string out_port, RTLIL::SigSpec out_val)
{
	RTLIL::SigSpec Y = cell->connections[out_port];
	log("Replacing %s cell `%s' (%s) in module `%s' with constant driver `%s = %s'.\n",
			cell->type.c_str(), cell->name.c_str(), info.c_str(),
			module->name.c_str(), log_signal(Y), log_signal(out_val));
	// ILANG_BACKEND::dump_cell(stderr, "--> ", cell);
	module->connections.push_back(RTLIL::SigSig(Y, out_val));
	module->cells.erase(cell->name);
	delete cell;
	OPT_DID_SOMETHING = true;
	did_something = true;
}

void replace_const_cells(RTLIL::Design *design, RTLIL::Module *module, bool consume_x, bool mux_undef, bool mux_bool)
{
	if (!design->selected(module))
		return;

	SigMap assign_map(module);
	std::map<RTLIL::SigSpec, RTLIL::SigSpec> invert_map;

	std::vector<RTLIL::Cell*> cells;
	cells.reserve(module->cells.size());
	for (auto &cell_it : module->cells)
		if (design->selected(module, cell_it.second)) {
			if ((cell_it.second->type == "$_INV_" || cell_it.second->type == "$not" || cell_it.second->type == "$logic_not") &&
					cell_it.second->connections["\\A"].width == 1 && cell_it.second->connections["\\Y"].width == 1)
				invert_map[assign_map(cell_it.second->connections["\\Y"])] = assign_map(cell_it.second->connections["\\A"]);
			cells.push_back(cell_it.second);
		}

	for (auto cell : cells)
	{
#define ACTION_DO(_p_, _s_) do { replace_cell(module, cell, input.as_string(), _p_, _s_); goto next_cell; } while (0)
#define ACTION_DO_Y(_v_) ACTION_DO("\\Y", RTLIL::SigSpec(RTLIL::State::S ## _v_))

		if ((cell->type == "$_INV_" || cell->type == "$not" || cell->type == "$logic_not") && cell->connections["\\Y"].width == 1 &&
				invert_map.count(assign_map(cell->connections["\\A"])) != 0) {
			replace_cell(module, cell, "double_invert", "\\Y", invert_map.at(assign_map(cell->connections["\\A"])));
			goto next_cell;
		}

		if ((cell->type == "$_MUX_" || cell->type == "$mux") && invert_map.count(assign_map(cell->connections["\\S"])) != 0) {
			RTLIL::SigSpec tmp = cell->connections["\\A"];
			cell->connections["\\A"] = cell->connections["\\B"];
			cell->connections["\\B"] = tmp;
			cell->connections["\\S"] = invert_map.at(assign_map(cell->connections["\\S"]));
			OPT_DID_SOMETHING = true;
			did_something = true;
			goto next_cell;
		}

		if (cell->type == "$_INV_") {
			RTLIL::SigSpec input = cell->connections["\\A"];
			assign_map.apply(input);
			if (input.match("1")) ACTION_DO_Y(0);
			if (input.match("0")) ACTION_DO_Y(1);
			if (input.match("*")) ACTION_DO_Y(x);
		}

		if (cell->type == "$_AND_") {
			RTLIL::SigSpec input;
			input.append(cell->connections["\\B"]);
			input.append(cell->connections["\\A"]);
			assign_map.apply(input);
			if (input.match(" 0")) ACTION_DO_Y(0);
			if (input.match("0 ")) ACTION_DO_Y(0);
			if (input.match("11")) ACTION_DO_Y(1);
			if (input.match("**")) ACTION_DO_Y(x);
			if (input.match("1*")) ACTION_DO_Y(x);
			if (input.match("*1")) ACTION_DO_Y(x);
			if (consume_x) {
				if (input.match(" *")) ACTION_DO_Y(0);
				if (input.match("* ")) ACTION_DO_Y(0);
			}
			if (input.match(" 1")) ACTION_DO("\\Y", input.extract(1, 1));
			if (input.match("1 ")) ACTION_DO("\\Y", input.extract(0, 1));
		}

		if (cell->type == "$_OR_") {
			RTLIL::SigSpec input;
			input.append(cell->connections["\\B"]);
			input.append(cell->connections["\\A"]);
			assign_map.apply(input);
			if (input.match(" 1")) ACTION_DO_Y(1);
			if (input.match("1 ")) ACTION_DO_Y(1);
			if (input.match("00")) ACTION_DO_Y(0);
			if (input.match("**")) ACTION_DO_Y(x);
			if (input.match("0*")) ACTION_DO_Y(x);
			if (input.match("*0")) ACTION_DO_Y(x);
			if (consume_x) {
				if (input.match(" *")) ACTION_DO_Y(1);
				if (input.match("* ")) ACTION_DO_Y(1);
			}
			if (input.match(" 0")) ACTION_DO("\\Y", input.extract(1, 1));
			if (input.match("0 ")) ACTION_DO("\\Y", input.extract(0, 1));
		}

		if (cell->type == "$_XOR_") {
			RTLIL::SigSpec input;
			input.append(cell->connections["\\B"]);
			input.append(cell->connections["\\A"]);
			assign_map.apply(input);
			if (input.match("00")) ACTION_DO_Y(0);
			if (input.match("01")) ACTION_DO_Y(1);
			if (input.match("10")) ACTION_DO_Y(1);
			if (input.match("11")) ACTION_DO_Y(0);
			if (input.match(" *")) ACTION_DO_Y(x);
			if (input.match("* ")) ACTION_DO_Y(x);
			if (input.match(" 0")) ACTION_DO("\\Y", input.extract(1, 1));
			if (input.match("0 ")) ACTION_DO("\\Y", input.extract(0, 1));
		}

		if (cell->type == "$_MUX_") {
			RTLIL::SigSpec input;
			input.append(cell->connections["\\S"]);
			input.append(cell->connections["\\B"]);
			input.append(cell->connections["\\A"]);
			assign_map.apply(input);
			if (input.extract(2, 1) == input.extract(1, 1))
				ACTION_DO("\\Y", input.extract(2, 1));
			if (input.match("  0")) ACTION_DO("\\Y", input.extract(2, 1));
			if (input.match("  1")) ACTION_DO("\\Y", input.extract(1, 1));
			if (input.match("01 ")) ACTION_DO("\\Y", input.extract(0, 1));
			if (input.match("10 ")) {
				cell->type = "$_INV_";
				cell->connections["\\A"] = input.extract(0, 1);
				cell->connections.erase("\\B");
				cell->connections.erase("\\S");
				goto next_cell;
			}
			if (input.match("11 ")) ACTION_DO_Y(1);
			if (input.match("00 ")) ACTION_DO_Y(0);
			if (input.match("** ")) ACTION_DO_Y(x);
			if (input.match("01*")) ACTION_DO_Y(x);
			if (input.match("10*")) ACTION_DO_Y(x);
			if (mux_undef) {
				if (input.match("*  ")) ACTION_DO("\\Y", input.extract(1, 1));
				if (input.match(" * ")) ACTION_DO("\\Y", input.extract(2, 1));
				if (input.match("  *")) ACTION_DO("\\Y", input.extract(2, 1));
			}
		}

		if (cell->type == "$eq" || cell->type == "$ne" || cell->type == "$eqx" || cell->type == "$nex")
		{
			RTLIL::SigSpec a = cell->connections["\\A"];
			RTLIL::SigSpec b = cell->connections["\\B"];

			if (cell->parameters["\\A_WIDTH"].as_int() != cell->parameters["\\B_WIDTH"].as_int()) {
				int width = std::max(cell->parameters["\\A_WIDTH"].as_int(), cell->parameters["\\B_WIDTH"].as_int());
				a.extend_u0(width, cell->parameters["\\A_SIGNED"].as_bool() && cell->parameters["\\B_SIGNED"].as_bool());
				b.extend_u0(width, cell->parameters["\\A_SIGNED"].as_bool() && cell->parameters["\\B_SIGNED"].as_bool());
			}

			RTLIL::SigSpec new_a, new_b;
			a.expand(), b.expand();

			assert(a.chunks.size() == b.chunks.size());
			for (size_t i = 0; i < a.chunks.size(); i++) {
				if (a.chunks[i].wire == NULL && b.chunks[i].wire == NULL && a.chunks[i].data.bits[0] != b.chunks[i].data.bits[0] &&
						a.chunks[i].data.bits[0] <= RTLIL::State::S1 && b.chunks[i].data.bits[0] <= RTLIL::State::S1) {
					RTLIL::SigSpec new_y = RTLIL::SigSpec((cell->type == "$eq" || cell->type == "$eqx") ?  RTLIL::State::S0 : RTLIL::State::S1);
					new_y.extend(cell->parameters["\\Y_WIDTH"].as_int(), false);
					replace_cell(module, cell, "empty", "\\Y", new_y);
					goto next_cell;
				}
				if (a.chunks[i] == b.chunks[i])
					continue;
				new_a.append(a.chunks[i]);
				new_b.append(b.chunks[i]);
			}

			if (new_a.width == 0) {
				RTLIL::SigSpec new_y = RTLIL::SigSpec((cell->type == "$eq" || cell->type == "$eqx") ?  RTLIL::State::S1 : RTLIL::State::S0);
				new_y.extend(cell->parameters["\\Y_WIDTH"].as_int(), false);
				replace_cell(module, cell, "empty", "\\Y", new_y);
				goto next_cell;
			}

			if (new_a.width < a.width || new_b.width < b.width) {
				new_a.optimize();
				new_b.optimize();
				cell->connections["\\A"] = new_a;
				cell->connections["\\B"] = new_b;
				cell->parameters["\\A_WIDTH"] = new_a.width;
				cell->parameters["\\B_WIDTH"] = new_b.width;
			}
		}

		if ((cell->type == "$eq" || cell->type == "$ne") && cell->parameters["\\Y_WIDTH"].as_int() == 1 &&
				cell->parameters["\\A_WIDTH"].as_int() == 1 && cell->parameters["\\B_WIDTH"].as_int() == 1)
		{
			RTLIL::SigSpec a = assign_map(cell->connections["\\A"]);
			RTLIL::SigSpec b = assign_map(cell->connections["\\B"]);

			if (a.is_fully_const()) {
				RTLIL::SigSpec tmp;
				tmp = a, a = b, b = tmp;
				cell->connections["\\A"] = a;
				cell->connections["\\B"] = b;
			}

			if (b.is_fully_const()) {
				if (b.as_bool() == (cell->type == "$eq")) {
					RTLIL::SigSpec input = b;
					ACTION_DO("\\Y", cell->connections["\\A"]);
				} else {
					cell->type = "$not";
					cell->parameters.erase("\\B_WIDTH");
					cell->parameters.erase("\\B_SIGNED");
					cell->connections.erase("\\B");
				}
				goto next_cell;
			}
		}

		if (mux_bool && (cell->type == "$mux" || cell->type == "$_MUX_") &&
				cell->connections["\\A"] == RTLIL::SigSpec(0, 1) && cell->connections["\\B"] == RTLIL::SigSpec(1, 1)) {
			replace_cell(module, cell, "mux_bool", "\\Y", cell->connections["\\S"]);
			goto next_cell;
		}

		if (mux_bool && (cell->type == "$mux" || cell->type == "$_MUX_") &&
				cell->connections["\\A"] == RTLIL::SigSpec(1, 1) && cell->connections["\\B"] == RTLIL::SigSpec(0, 1)) {
			cell->connections["\\A"] = cell->connections["\\S"];
			cell->connections.erase("\\B");
			cell->connections.erase("\\S");
			if (cell->type == "$mux") {
				cell->parameters["\\A_WIDTH"] = cell->parameters["\\WIDTH"];
				cell->parameters["\\Y_WIDTH"] = cell->parameters["\\WIDTH"];
				cell->parameters["\\A_SIGNED"] = 0;
				cell->parameters.erase("\\WIDTH");
				cell->type = "$not";
			} else
				cell->type = "$_INV_";
			OPT_DID_SOMETHING = true;
			did_something = true;
			goto next_cell;
		}

		if (consume_x && mux_bool && (cell->type == "$mux" || cell->type == "$_MUX_") && cell->connections["\\A"] == RTLIL::SigSpec(0, 1)) {
			cell->connections["\\A"] = cell->connections["\\S"];
			cell->connections.erase("\\S");
			if (cell->type == "$mux") {
				cell->parameters["\\A_WIDTH"] = cell->parameters["\\WIDTH"];
				cell->parameters["\\B_WIDTH"] = cell->parameters["\\WIDTH"];
				cell->parameters["\\Y_WIDTH"] = cell->parameters["\\WIDTH"];
				cell->parameters["\\A_SIGNED"] = 0;
				cell->parameters["\\B_SIGNED"] = 0;
				cell->parameters.erase("\\WIDTH");
				cell->type = "$and";
			} else
				cell->type = "$_AND_";
			OPT_DID_SOMETHING = true;
			did_something = true;
			goto next_cell;
		}

		if (consume_x && mux_bool && (cell->type == "$mux" || cell->type == "$_MUX_") && cell->connections["\\B"] == RTLIL::SigSpec(1, 1)) {
			cell->connections["\\B"] = cell->connections["\\S"];
			cell->connections.erase("\\S");
			if (cell->type == "$mux") {
				cell->parameters["\\A_WIDTH"] = cell->parameters["\\WIDTH"];
				cell->parameters["\\B_WIDTH"] = cell->parameters["\\WIDTH"];
				cell->parameters["\\Y_WIDTH"] = cell->parameters["\\WIDTH"];
				cell->parameters["\\A_SIGNED"] = 0;
				cell->parameters["\\B_SIGNED"] = 0;
				cell->parameters.erase("\\WIDTH");
				cell->type = "$or";
			} else
				cell->type = "$_OR_";
			OPT_DID_SOMETHING = true;
			did_something = true;
			goto next_cell;
		}

		if (mux_undef && (cell->type == "$mux" || cell->type == "$pmux")) {
			RTLIL::SigSpec new_a, new_b, new_s;
			int width = cell->connections.at("\\A").width;
			if ((cell->connections.at("\\A").is_fully_undef() && cell->connections.at("\\B").is_fully_undef()) ||
					cell->connections.at("\\S").is_fully_undef()) {
				replace_cell(module, cell, "mux undef", "\\Y", cell->connections.at("\\A"));
				goto next_cell;
			}
			for (int i = 0; i < cell->connections.at("\\S").width; i++) {
				RTLIL::SigSpec old_b = cell->connections.at("\\B").extract(i*width, width);
				RTLIL::SigSpec old_s = cell->connections.at("\\S").extract(i, 1);
				if (old_b.is_fully_undef() || old_s.is_fully_undef())
					continue;
				new_b.append(old_b);
				new_s.append(old_s);
			}
			new_a = cell->connections.at("\\A");
			if (new_a.is_fully_undef() && new_s.width > 0) {
				new_a = new_b.extract((new_s.width-1)*width, width);
				new_b = new_b.extract(0, (new_s.width-1)*width);
				new_s = new_s.extract(0, new_s.width-1);
			}
			if (new_s.width == 0) {
				replace_cell(module, cell, "mux undef", "\\Y", new_a);
				goto next_cell;
			}
			if (new_a == RTLIL::SigSpec(RTLIL::State::S0) && new_b == RTLIL::SigSpec(RTLIL::State::S1)) {
				replace_cell(module, cell, "mux undef", "\\Y", new_s);
				goto next_cell;
			}
			if (cell->connections.at("\\S").width != new_s.width) {
				cell->connections.at("\\A") = new_a;
				cell->connections.at("\\B") = new_b;
				cell->connections.at("\\S") = new_s;
				if (new_s.width > 1) {
					cell->type = "$pmux";
					cell->parameters["\\S_WIDTH"] = new_s.width;
				} else {
					cell->type = "$mux";
					cell->parameters.erase("\\S_WIDTH");
				}
				OPT_DID_SOMETHING = true;
				did_something = true;
			}
		}

#define FOLD_1ARG_CELL(_t) \
		if (cell->type == "$" #_t) { \
			RTLIL::SigSpec a = cell->connections["\\A"]; \
			assign_map.apply(a); \
			if (a.is_fully_const()) { \
				a.optimize(); \
				if (a.chunks.empty()) a.chunks.push_back(RTLIL::SigChunk()); \
				RTLIL::Const dummy_arg(RTLIL::State::S0, 1); \
				RTLIL::SigSpec y(RTLIL::const_ ## _t(a.chunks[0].data, dummy_arg, \
						cell->parameters["\\A_SIGNED"].as_bool(), false, \
						cell->parameters["\\Y_WIDTH"].as_int())); \
				replace_cell(module, cell, stringf("%s", log_signal(a)), "\\Y", y); \
				goto next_cell; \
			} \
		}
#define FOLD_2ARG_CELL(_t) \
		if (cell->type == "$" #_t) { \
			RTLIL::SigSpec a = cell->connections["\\A"]; \
			RTLIL::SigSpec b = cell->connections["\\B"]; \
			assign_map.apply(a), assign_map.apply(b); \
			if (a.is_fully_const() && b.is_fully_const()) { \
				a.optimize(), b.optimize(); \
				if (a.chunks.empty()) a.chunks.push_back(RTLIL::SigChunk()); \
				if (b.chunks.empty()) b.chunks.push_back(RTLIL::SigChunk()); \
				RTLIL::SigSpec y(RTLIL::const_ ## _t(a.chunks[0].data, b.chunks[0].data, \
						cell->parameters["\\A_SIGNED"].as_bool(), \
						cell->parameters["\\B_SIGNED"].as_bool(), \
						cell->parameters["\\Y_WIDTH"].as_int())); \
				replace_cell(module, cell, stringf("%s, %s", log_signal(a), log_signal(b)), "\\Y", y); \
				goto next_cell; \
			} \
		}

		FOLD_1ARG_CELL(not)
		FOLD_2ARG_CELL(and)
		FOLD_2ARG_CELL(or)
		FOLD_2ARG_CELL(xor)
		FOLD_2ARG_CELL(xnor)

		FOLD_1ARG_CELL(reduce_and)
		FOLD_1ARG_CELL(reduce_or)
		FOLD_1ARG_CELL(reduce_xor)
		FOLD_1ARG_CELL(reduce_xnor)
		FOLD_1ARG_CELL(reduce_bool)

		FOLD_1ARG_CELL(logic_not)
		FOLD_2ARG_CELL(logic_and)
		FOLD_2ARG_CELL(logic_or)

		FOLD_2ARG_CELL(shl)
		FOLD_2ARG_CELL(shr)
		FOLD_2ARG_CELL(sshl)
		FOLD_2ARG_CELL(sshr)

		FOLD_2ARG_CELL(lt)
		FOLD_2ARG_CELL(le)
		FOLD_2ARG_CELL(eq)
		FOLD_2ARG_CELL(ne)
		FOLD_2ARG_CELL(gt)
		FOLD_2ARG_CELL(ge)

		FOLD_2ARG_CELL(add)
		FOLD_2ARG_CELL(sub)
		FOLD_2ARG_CELL(mul)
		FOLD_2ARG_CELL(div)
		FOLD_2ARG_CELL(mod)
		FOLD_2ARG_CELL(pow)

		FOLD_1ARG_CELL(pos)
		FOLD_1ARG_CELL(bu0)
		FOLD_1ARG_CELL(neg)

		// be very conservative with optimizing $mux cells as we do not want to break mux trees
		if (cell->type == "$mux") {
			RTLIL::SigSpec input = assign_map(cell->connections["\\S"]);
			RTLIL::SigSpec inA = assign_map(cell->connections["\\A"]);
			RTLIL::SigSpec inB = assign_map(cell->connections["\\B"]);
			if (input.is_fully_const())
				ACTION_DO("\\Y", input.as_bool() ? cell->connections["\\B"] : cell->connections["\\A"]);
			else if (inA == inB)
				ACTION_DO("\\Y", cell->connections["\\A"]);
		}

	next_cell:;
#undef ACTION_DO
#undef ACTION_DO_Y
#undef FOLD_1ARG_CELL
#undef FOLD_2ARG_CELL
	}
}

struct OptConstPass : public Pass {
	OptConstPass() : Pass("opt_const", "perform const folding") { }
	virtual void help()
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    opt_const [options] [selection]\n");
		log("\n");
		log("This pass performs const folding on internal cell types with constant inputs.\n");
		log("\n");
		log("    -mux_undef\n");
		log("        remove 'undef' inputs from $mux, $pmux and $_MUX_ cells\n");
		log("\n");
		log("    -mux_bool\n");
		log("        replace $mux cells with inverters or buffers when possible\n");
		log("\n");
		log("    -undriven\n");
		log("        replace undriven nets with undef (x) constants\n");
		log("\n");
	}
	virtual void execute(std::vector<std::string> args, RTLIL::Design *design)
	{
		bool mux_undef = false;
		bool mux_bool = false;
		bool undriven = false;

		log_header("Executing OPT_CONST pass (perform const folding).\n");
		log_push();

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			if (args[argidx] == "-mux_undef") {
				mux_undef = true;
				continue;
			}
			if (args[argidx] == "-mux_bool") {
				mux_bool = true;
				continue;
			}
			if (args[argidx] == "-undriven") {
				undriven = true;
				continue;
			}
			break;
		}
		extra_args(args, argidx, design);

		for (auto &mod_it : design->modules)
		{
			if (undriven)
				replace_undriven(design, mod_it.second);

			do {
				do {
					did_something = false;
					replace_const_cells(design, mod_it.second, false, mux_undef, mux_bool);
				} while (did_something);
				replace_const_cells(design, mod_it.second, true, mux_undef, mux_bool);
			} while (did_something);
		}

		log_pop();
	}
} OptConstPass;

