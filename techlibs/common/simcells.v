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
 *  The internal logic cell simulation library.
 *
 *  This verilog library contains simple simulation models for the internal
 *  logic cells ($_INV_ , $_AND_ , ...) that are generated by the default technology
 *  mapper (see "stdcells.v" in this directory) and expected by the "abc" pass.
 *
 */

module  \$_INV_ (A, Y);
input A;
output Y;
assign Y = ~A;
endmodule

module  \$_AND_ (A, B, Y);
input A, B;
output Y;
assign Y = A & B;
endmodule

module  \$_OR_ (A, B, Y);
input A, B;
output Y;
assign Y = A | B;
endmodule

module  \$_XOR_ (A, B, Y);
input A, B;
output Y;
assign Y = A ^ B;
endmodule

module \$_MUX_ (A, B, S, Y);
input A, B, S;
output Y;
assign Y = S ? B : A;
endmodule

module  \$_SR_NN_ (S, R, Q);
input S, R;
output reg Q;
always @(negedge S, negedge R) begin
	if (R == 0)
		Q <= 0;
	else if (S == 0)
		Q <= 1;
end
endmodule

module  \$_SR_NP_ (S, R, Q);
input S, R;
output reg Q;
always @(negedge S, posedge R) begin
	if (R == 1)
		Q <= 0;
	else if (S == 0)
		Q <= 1;
end
endmodule

module  \$_SR_PN_ (S, R, Q);
input S, R;
output reg Q;
always @(posedge S, negedge R) begin
	if (R == 0)
		Q <= 0;
	else if (S == 1)
		Q <= 1;
end
endmodule

module  \$_SR_PP_ (S, R, Q);
input S, R;
output reg Q;
always @(posedge S, posedge R) begin
	if (R == 1)
		Q <= 0;
	else if (S == 1)
		Q <= 1;
end
endmodule

module  \$_DFF_N_ (D, Q, C);
input D, C;
output reg Q;
always @(negedge C) begin
	Q <= D;
end
endmodule

module  \$_DFF_P_ (D, Q, C);
input D, C;
output reg Q;
always @(posedge C) begin
	Q <= D;
end
endmodule

module  \$_DFF_NN0_ (D, Q, C, R);
input D, C, R;
output reg Q;
always @(negedge C or negedge R) begin
	if (R == 0)
		Q <= 0;
	else
		Q <= D;
end
endmodule

module  \$_DFF_NN1_ (D, Q, C, R);
input D, C, R;
output reg Q;
always @(negedge C or negedge R) begin
	if (R == 0)
		Q <= 1;
	else
		Q <= D;
end
endmodule

module  \$_DFF_NP0_ (D, Q, C, R);
input D, C, R;
output reg Q;
always @(negedge C or posedge R) begin
	if (R == 1)
		Q <= 0;
	else
		Q <= D;
end
endmodule

module  \$_DFF_NP1_ (D, Q, C, R);
input D, C, R;
output reg Q;
always @(negedge C or posedge R) begin
	if (R == 1)
		Q <= 1;
	else
		Q <= D;
end
endmodule

module  \$_DFF_PN0_ (D, Q, C, R);
input D, C, R;
output reg Q;
always @(posedge C or negedge R) begin
	if (R == 0)
		Q <= 0;
	else
		Q <= D;
end
endmodule

module  \$_DFF_PN1_ (D, Q, C, R);
input D, C, R;
output reg Q;
always @(posedge C or negedge R) begin
	if (R == 0)
		Q <= 1;
	else
		Q <= D;
end
endmodule

module  \$_DFF_PP0_ (D, Q, C, R);
input D, C, R;
output reg Q;
always @(posedge C or posedge R) begin
	if (R == 1)
		Q <= 0;
	else
		Q <= D;
end
endmodule

module  \$_DFF_PP1_ (D, Q, C, R);
input D, C, R;
output reg Q;
always @(posedge C or posedge R) begin
	if (R == 1)
		Q <= 1;
	else
		Q <= D;
end
endmodule

module  \$_DFFSR_NNN_ (C, S, R, D, Q);
input C, S, R, D;
output reg Q;
always @(negedge C, negedge S, negedge R) begin
	if (R == 0)
		Q <= 0;
	else if (S == 0)
		Q <= 1;
	else
		Q <= D;
end
endmodule

module  \$_DFFSR_NNP_ (C, S, R, D, Q);
input C, S, R, D;
output reg Q;
always @(negedge C, negedge S, posedge R) begin
	if (R == 1)
		Q <= 0;
	else if (S == 0)
		Q <= 1;
	else
		Q <= D;
end
endmodule

module  \$_DFFSR_NPN_ (C, S, R, D, Q);
input C, S, R, D;
output reg Q;
always @(negedge C, posedge S, negedge R) begin
	if (R == 0)
		Q <= 0;
	else if (S == 1)
		Q <= 1;
	else
		Q <= D;
end
endmodule

module  \$_DFFSR_NPP_ (C, S, R, D, Q);
input C, S, R, D;
output reg Q;
always @(negedge C, posedge S, posedge R) begin
	if (R == 1)
		Q <= 0;
	else if (S == 1)
		Q <= 1;
	else
		Q <= D;
end
endmodule

module  \$_DFFSR_PNN_ (C, S, R, D, Q);
input C, S, R, D;
output reg Q;
always @(posedge C, negedge S, negedge R) begin
	if (R == 0)
		Q <= 0;
	else if (S == 0)
		Q <= 1;
	else
		Q <= D;
end
endmodule

module  \$_DFFSR_PNP_ (C, S, R, D, Q);
input C, S, R, D;
output reg Q;
always @(posedge C, negedge S, posedge R) begin
	if (R == 1)
		Q <= 0;
	else if (S == 0)
		Q <= 1;
	else
		Q <= D;
end
endmodule

module  \$_DFFSR_PPN_ (C, S, R, D, Q);
input C, S, R, D;
output reg Q;
always @(posedge C, posedge S, negedge R) begin
	if (R == 0)
		Q <= 0;
	else if (S == 1)
		Q <= 1;
	else
		Q <= D;
end
endmodule

module  \$_DFFSR_PPP_ (C, S, R, D, Q);
input C, S, R, D;
output reg Q;
always @(posedge C, posedge S, posedge R) begin
	if (R == 1)
		Q <= 0;
	else if (S == 1)
		Q <= 1;
	else
		Q <= D;
end
endmodule

module  \$_DLATCH_N_ (E, D, Q);
input E, D;
output reg Q;
always @* begin
	if (E == 0)
		Q <= D;
end
endmodule

module  \$_DLATCH_P_ (E, D, Q);
input E, D;
output reg Q;
always @* begin
	if (E == 1)
		Q <= D;
end
endmodule

module  \$_DLATCHSR_NNN_ (E, S, R, D, Q);
input E, S, R, D;
output reg Q;
always @* begin
	if (R == 0)
		Q <= 0;
	else if (S == 0)
		Q <= 1;
	else if (E == 0)
		Q <= D;
end
endmodule

module  \$_DLATCHSR_NNP_ (E, S, R, D, Q);
input E, S, R, D;
output reg Q;
always @* begin
	if (R == 1)
		Q <= 0;
	else if (S == 0)
		Q <= 1;
	else if (E == 0)
		Q <= D;
end
endmodule

module  \$_DLATCHSR_NPN_ (E, S, R, D, Q);
input E, S, R, D;
output reg Q;
always @* begin
	if (R == 0)
		Q <= 0;
	else if (S == 1)
		Q <= 1;
	else if (E == 0)
		Q <= D;
end
endmodule

module  \$_DLATCHSR_NPP_ (E, S, R, D, Q);
input E, S, R, D;
output reg Q;
always @* begin
	if (R == 1)
		Q <= 0;
	else if (S == 1)
		Q <= 1;
	else if (E == 0)
		Q <= D;
end
endmodule

module  \$_DLATCHSR_PNN_ (E, S, R, D, Q);
input E, S, R, D;
output reg Q;
always @* begin
	if (R == 0)
		Q <= 0;
	else if (S == 0)
		Q <= 1;
	else if (E == 1)
		Q <= D;
end
endmodule

module  \$_DLATCHSR_PNP_ (E, S, R, D, Q);
input E, S, R, D;
output reg Q;
always @* begin
	if (R == 1)
		Q <= 0;
	else if (S == 0)
		Q <= 1;
	else if (E == 1)
		Q <= D;
end
endmodule

module  \$_DLATCHSR_PPN_ (E, S, R, D, Q);
input E, S, R, D;
output reg Q;
always @* begin
	if (R == 0)
		Q <= 0;
	else if (S == 1)
		Q <= 1;
	else if (E == 1)
		Q <= D;
end
endmodule

module  \$_DLATCHSR_PPP_ (E, S, R, D, Q);
input E, S, R, D;
output reg Q;
always @* begin
	if (R == 1)
		Q <= 0;
	else if (S == 1)
		Q <= 1;
	else if (E == 1)
		Q <= D;
end
endmodule

