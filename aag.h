#pragma once
#include <iostream>
#include <vector>

struct AIG {

	struct And {
		int in1, in2;
		int out;

		And(int in1_, int in2_, int out_) : in1(in1_), in2(in2_), out(out_)
		{
		}
	};

	std::vector<int> inputs;
	std::vector<int> outputs;
	std::vector<std::pair<int, int>> latches;
	std::vector<And> gates;

	static auto FromStream(std::istream &) -> AIG;
};