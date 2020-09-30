#pragma once
#include <iostream>
#include <vector>

// AIG stores an AIGER hardware model. It can be parsed from
// ASCII AIGER files with the function FromStream.
struct AIG {

	// And is the internal representation used for an And gate.
	struct And {
		int out;
		int in1, in2;

		And(int out_, int in1_, int in2_) : out(out_), in1(in1_), in2(in2_)
		{
		}
	};

	std::vector<int> inputs;
	std::vector<int> outputs;
	std::vector<std::pair<int, int>> latches;
	std::vector<And> gates;
	int lastLit;

	auto IsInput(int var) const -> bool;
	auto IsOutput(int var) const -> bool;
	auto IsGateOutput(int var) const -> bool;
	auto IsLatchOutput(int var) const -> bool;

	// reads an ASCII formated AIGER file from the istream and returns an
	// AIG structure containing the parsed data.
	static auto FromStream(std::istream &) -> AIG;
};