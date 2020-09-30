#include "aag.h"
#include <algorithm>
#include <cctype>
#include <functional>
#include <vector>

static auto skipwhite(std::istream &in, bool multiline = false) -> std::istream &
{
	if (multiline)
		while (!in.eof() && isspace(in.peek()))
			in.get();
	else
		while (!in.eof() && isblank(in.peek()))
			in.get();
	return in;
}

static auto nextline(std::istream &in) -> std::istream &
{
	do {
		while (!in.eof() && in.peek() != '\n')
			in.get();
		while (!in.eof() && isspace(in.peek()))
			in.get();
	} while (!in.eof() && !isdigit(in.peek()));

	return in;
}

static auto readnum(std::istream &in) -> int
{
	int n = 0;

	while (!in.eof() && isdigit(in.peek())) {
		n = 10 * n + (in.peek() - '0');
		in.get();
	}

	return n;
}

static auto matchMagic(std::istream &in) -> bool
{
	std::vector<char> r(5);
	in.get(r.data(), r.size());
	std::string s(r.begin(), --r.end());
	return s == "aag ";
}

template <typename Func>
auto readline(std::istream &in, int nums, Func f) -> std::istream &
{
	std::vector<int> v(nums);
	size_t i = 0;

	while (!in.eof() && i != v.size()) {
		if (!isdigit(in.peek()))
			throw std::runtime_error("number expected");
		v[i++] = readnum(in);
		skipwhite(in);
	}
	nextline(in);

	if (i != v.size())
		throw std::runtime_error("too few numbers on line");

	f(const_cast<std::vector<int> &>(v));

	return in;
}

auto AIG::IsInput(int var) const -> bool
{
	return std::find(inputs.cbegin(), inputs.cend(), var) != inputs.cend();
}
auto AIG::IsOutput(int var) const -> bool
{
	return std::find(outputs.cbegin(), outputs.cend(), var) != outputs.cend();
}
auto AIG::IsGateOutput(int var) const -> bool
{
	return std::find_if(gates.cbegin(), gates.cend(),
	                    [var](const AIG::And &g) { return g.out == var; }) != gates.cend();
}
auto AIG::IsLatchOutput(int var) const -> bool
{
	return std::find_if(latches.cbegin(), latches.cend(), [var](const std::pair<int, int> &l) {
		       return l.first == var;
	       }) != latches.cend();
}

auto AIG::FromStream(std::istream &in) -> AIG
{
	skipwhite(in, true);

	if (!matchMagic(in))
		throw std::runtime_error("not AIGer ASCII format!");

	const auto maxVarInd = 2*readnum(in);
	skipwhite(in);
	const auto inputLines = readnum(in);
	skipwhite(in);
	const auto stateLines = readnum(in);
	skipwhite(in);
	const auto outputLines = readnum(in);
	skipwhite(in);
	const auto gateLines = readnum(in);
	skipwhite(in);
	nextline(in);

	AIG aig;
	aig.lastLit = maxVarInd;

	auto rec = [&aig](std::initializer_list<int> list) {
		aig.lastLit = std::max(std::max(list), aig.lastLit);
	};

	int count;
	for (count = inputLines; !in.eof() && count != 0; --count) {
		readline(in, 1, [&](const std::vector<int> &v) { aig.inputs.push_back(v[0]); rec({v[0]}); });
	}

	for (count = stateLines; !in.eof() && count != 0; --count) {
		readline(in, 2,
		         [&](const std::vector<int> &v) { aig.latches.emplace_back(v[0], v[1]); rec({v[0], v[1]}); });
	}

	for (count = outputLines; !in.eof() && count != 0; --count) {
		readline(in, 1, [&](const std::vector<int> &v) { aig.outputs.push_back(v[0]); rec({v[0]}); });
	}

	for (count = gateLines; !in.eof() && count != 0; --count) {
		readline(in, 3, [&](const std::vector<int> &v) {
			aig.gates.emplace_back(v[0], v[1], v[2]);
			 rec({v[0], v[1], v[2]});
		});
	}

	if (aig.lastLit/2 > maxVarInd) {
		std::cout << "warning: the number of variables is not correct in your aag file! (" << maxVarInd << " given, " << aig.lastLit << " correct)" << std::endl;
	}

	return aig;
}
