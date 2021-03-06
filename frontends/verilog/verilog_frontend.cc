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
 *  ---
 *
 *  The Verilog frontend.
 *
 *  This frontend is using the AST frontend library (see frontends/ast/).
 *  Thus this frontend does not generate RTLIL code directly but creates an
 *  AST directly from the Verilog parse tree and then passes this AST to
 *  the AST frontend library.
 *
 */

#include "verilog_frontend.h"
#include "kernel/compatibility.h"
#include "kernel/register.h"
#include "kernel/log.h"
#include "libs/sha1/sha1.h"
#include <sstream>
#include <stdarg.h>
#include <assert.h>

using namespace VERILOG_FRONTEND;

// use the Verilog bison/flex parser to generate an AST and use AST::process() to convert it to RTLIL

static std::vector<std::string> verilog_defaults;
static std::list<std::vector<std::string>> verilog_defaults_stack;

struct VerilogFrontend : public Frontend {
	VerilogFrontend() : Frontend("verilog", "read modules from verilog file") { }
	virtual void help()
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    read_verilog [filename]\n");
		log("\n");
		log("Load modules from a verilog file to the current design. A large subset of\n");
		log("Verilog-2005 is supported.\n");
		log("\n");
		log("    -dump_ast1\n");
		log("        dump abstract syntax tree (before simplification)\n");
		log("\n");
		log("    -dump_ast2\n");
		log("        dump abstract syntax tree (after simplification)\n");
		log("\n");
		log("    -dump_vlog\n");
		log("        dump ast as verilog code (after simplification)\n");
		log("\n");
		log("    -yydebug\n");
		log("        enable parser debug output\n");
		log("\n");
		log("    -nolatches\n");
		log("        usually latches are synthesized into logic loops\n");
		log("        this option prohibits this and sets the output to 'x'\n");
		log("        in what would be the latches hold condition\n");
		log("\n");
		log("        this behavior can also be achieved by setting the\n");
		log("        'nolatches' attribute on the respective module or\n");
		log("        always block.\n");
		log("\n");
		log("    -nomem2reg\n");
		log("        under certain conditions memories are converted to registers\n");
		log("        early during simplification to ensure correct handling of\n");
		log("        complex corner cases. this option disables this behavior.\n");
		log("\n");
		log("        this can also be achieved by setting the 'nomem2reg'\n");
		log("        attribute on the respective module or register.\n");
		log("\n");
		log("    -mem2reg\n");
		log("        always convert memories to registers. this can also be\n");
		log("        achieved by setting the 'mem2reg' attribute on the respective\n");
		log("        module or register.\n");
		log("\n");
		log("    -ppdump\n");
		log("        dump verilog code after pre-processor\n");
		log("\n");
		log("    -nopp\n");
		log("        do not run the pre-processor\n");
		log("\n");
		log("    -lib\n");
		log("        only create empty blackbox modules\n");
		log("\n");
		log("    -noopt\n");
		log("        don't perform basic optimizations (such as const folding) in the\n");
		log("        high-level front-end.\n");
		log("\n");
		log("    -icells\n");
		log("        interpret cell types starting with '$' as internal cell types\n");
		log("\n");
		log("    -ignore_redef\n");
		log("        ignore re-definitions of modules. (the default behavior is to\n");
		log("        create an error message.)\n");
		log("\n");
		log("    -defer\n");
		log("        only read the abstract syntax tree and defer actual compilation\n");
		log("        to a later 'hierarchy' command. Useful in cases where the default\n");
		log("        parameters of modules yield invalid or not synthesizable code.\n");
		log("\n");
		log("    -setattr <attribute_name>\n");
		log("        set the specified attribute (to the value 1) on all loaded modules\n");
		log("\n");
		log("    -Dname[=definition]\n");
		log("        define the preprocessor symbol 'name' and set its optional value\n");
		log("        'definition'\n");
		log("\n");
		log("    -Idir\n");
		log("        add 'dir' to the directories which are used when searching include\n");
		log("        files\n");
		log("\n");
		log("The command 'verilog_defaults' can be used to register default options for\n");
		log("subsequent calls to 'read_verilog'.\n");
		log("\n");
		log("Note that the Verilog frontend does a pretty good job of processing valid\n");
		log("verilog input, but has not very good error reporting. It generally is\n");
		log("recommended to use a simulator (for example icarus verilog) for checking\n");
		log("the syntax of the code, rather than to rely on read_verilog for that.\n");
		log("\n");
	}
	virtual void execute(FILE *&f, std::string filename, std::vector<std::string> args, RTLIL::Design *design)
	{
		bool flag_dump_ast1 = false;
		bool flag_dump_ast2 = false;
		bool flag_dump_vlog = false;
		bool flag_nolatches = false;
		bool flag_nomem2reg = false;
		bool flag_mem2reg = false;
		bool flag_ppdump = false;
		bool flag_nopp = false;
		bool flag_lib = false;
		bool flag_noopt = false;
		bool flag_icells = false;
		bool flag_ignore_redef = false;
		bool flag_defer = false;
		std::map<std::string, std::string> defines_map;
		std::list<std::string> include_dirs;
		std::list<std::string> attributes;
		frontend_verilog_yydebug = false;

		log_header("Executing Verilog-2005 frontend.\n");

		args.insert(args.begin()+1, verilog_defaults.begin(), verilog_defaults.end());

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			std::string arg = args[argidx];
			if (arg == "-dump_ast1") {
				flag_dump_ast1 = true;
				continue;
			}
			if (arg == "-dump_ast2") {
				flag_dump_ast2 = true;
				continue;
			}
			if (arg == "-dump_vlog") {
				flag_dump_vlog = true;
				continue;
			}
			if (arg == "-yydebug") {
				frontend_verilog_yydebug = true;
				continue;
			}
			if (arg == "-nolatches") {
				flag_nolatches = true;
				continue;
			}
			if (arg == "-nomem2reg") {
				flag_nomem2reg = true;
				continue;
			}
			if (arg == "-mem2reg") {
				flag_mem2reg = true;
				continue;
			}
			if (arg == "-ppdump") {
				flag_ppdump = true;
				continue;
			}
			if (arg == "-nopp") {
				flag_nopp = true;
				continue;
			}
			if (arg == "-lib") {
				flag_lib = true;
				continue;
			}
			if (arg == "-noopt") {
				flag_noopt = true;
				continue;
			}
			if (arg == "-icells") {
				flag_icells = true;
				continue;
			}
			if (arg == "-ignore_redef") {
				flag_ignore_redef = true;
				continue;
			}
			if (arg == "-defer") {
				flag_defer = true;
				continue;
			}
			if (arg == "-setattr" && argidx+1 < args.size()) {
				attributes.push_back(RTLIL::escape_id(args[++argidx]));
				continue;
			}
			if (arg == "-D" && argidx+1 < args.size()) {
				std::string name = args[++argidx], value;
				size_t equal = name.find('=', 2);
				if (equal != std::string::npos) {
					value = arg.substr(equal+1);
					name = arg.substr(0, equal);
				}
				defines_map[name] = value;
				continue;
			}
			if (arg.compare(0, 2, "-D") == 0) {
				size_t equal = arg.find('=', 2);
				std::string name = arg.substr(2, equal-2);
				std::string value;
				if (equal != std::string::npos)
					value = arg.substr(equal+1);
				defines_map[name] = value;
				continue;
			}
			if (arg == "-I" && argidx+1 < args.size()) {
				include_dirs.push_back(args[++argidx]);
				continue;
			}
			if (arg.compare(0, 2, "-I") == 0) {
				include_dirs.push_back(arg.substr(2));
				continue;
			}
			break;
		}
		extra_args(f, filename, args, argidx);

		log("Parsing Verilog input from `%s' to AST representation.\n", filename.c_str());

		AST::current_filename = filename;
		AST::set_line_num = &frontend_verilog_yyset_lineno;
		AST::get_line_num = &frontend_verilog_yyget_lineno;

		current_ast = new AST::AstNode(AST::AST_DESIGN);
		default_nettype_wire = true;

		FILE *fp = f;
		std::string code_after_preproc;

		if (!flag_nopp) {
			code_after_preproc = frontend_verilog_preproc(f, filename, defines_map, include_dirs);
			if (flag_ppdump)
				log("-- Verilog code after preprocessor --\n%s-- END OF DUMP --\n", code_after_preproc.c_str());
			fp = fmemopen((void*)code_after_preproc.c_str(), code_after_preproc.size(), "r");
		}

		frontend_verilog_yyset_lineno(1);
		frontend_verilog_yyrestart(fp);
		frontend_verilog_yyparse();
		frontend_verilog_yylex_destroy();

		for (auto &child : current_ast->children) {
			log_assert(child->type == AST::AST_MODULE);
			for (auto &attr : attributes)
				if (child->attributes.count(attr) == 0)
					child->attributes[attr] = AST::AstNode::mkconst_int(1, false);
		}

		AST::process(design, current_ast, flag_dump_ast1, flag_dump_ast2, flag_dump_vlog, flag_nolatches, flag_nomem2reg, flag_mem2reg, flag_lib, flag_noopt, flag_icells, flag_ignore_redef, flag_defer, default_nettype_wire);

		if (!flag_nopp)
			fclose(fp);

		delete current_ast;
		current_ast = NULL;

		log("Successfully finished Verilog frontend.\n");
	}
} VerilogFrontend;

// the yyerror function used by bison to report parser errors
void frontend_verilog_yyerror(char const *fmt, ...)
{
	va_list ap;
	char buffer[1024];
	char *p = buffer;
	p += snprintf(p, buffer + sizeof(buffer) - p, "Parser error in line %s:%d: ",
			AST::current_filename.c_str(), frontend_verilog_yyget_lineno());
	va_start(ap, fmt);
	p += vsnprintf(p, buffer + sizeof(buffer) - p, fmt, ap);
	va_end(ap);
	p += snprintf(p, buffer + sizeof(buffer) - p, "\n");
	log_error("%s", buffer);
	exit(1);
}

struct VerilogDefaults : public Pass {
	VerilogDefaults() : Pass("verilog_defaults", "set default options for read_verilog") { }
	virtual void help()
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    verilog_defaults -add [options]\n");
		log("\n");
		log("Add the sepcified options to the list of default options to read_verilog.\n");
		log("\n");
		log("\n");
		log("    verilog_defaults -clear");
		log("\n");
		log("Clear the list of verilog default options.\n");
		log("\n");
		log("\n");
		log("    verilog_defaults -push");
		log("    verilog_defaults -pop");
		log("\n");
		log("Push or pop the list of default options to a stack. Note that -push does\n");
		log("not imply -clear.\n");
		log("\n");
	}
	virtual void execute(std::vector<std::string> args, RTLIL::Design*)
	{
		if (args.size() == 0)
			cmd_error(args, 1, "Missing argument.");

		if (args[1] == "-add") {
			verilog_defaults.insert(verilog_defaults.end(), args.begin()+2, args.end());
			return;
		}

		if (args.size() != 2)
			cmd_error(args, 2, "Extra argument.");

		if (args[1] == "-clear") {
			verilog_defaults.clear();
			return;
		}

		if (args[1] == "-push") {
			verilog_defaults_stack.push_back(verilog_defaults);
			return;
		}

		if (args[1] == "-pop") {
			if (verilog_defaults_stack.empty()) {
				verilog_defaults.clear();
			} else {
				verilog_defaults.swap(verilog_defaults_stack.back());
				verilog_defaults_stack.pop_back();
			}
			return;
		}
	}
} VerilogDefaults;

