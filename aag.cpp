#include "aag.h"
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

auto AIG::FromStream(std::istream &in) -> AIG
{
	skipwhite(in, true);

	if (!matchMagic(in))
		throw std::runtime_error("not AIGer ASCII format!");

	const auto maxVarInd = readnum(in);
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

	std::cout << "parsing inputs ..." << std::endl;
	int count;
	for (count = inputLines; !in.eof() && count != 0; --count) {
		readline(in, 1, [&](const std::vector<int> &v) {
			aig.inputs.push_back(v[0]);
			std::cout << "input " << v[0] << std::endl;
		});
	}

	std::cout << "parsing latches ..." << std::endl;
	for (count = stateLines; !in.eof() && count != 0; --count) {
		readline(in, 2, [&](const std::vector<int> &v) {
			aig.latches.emplace_back(v[0], v[1]);
			std::cout << "latch " << v[0] << " " << v[1] << std::endl;
		});
	}

	std::cout << "parsing outputs ..." << std::endl;
	for (count = outputLines; !in.eof() && count != 0; --count) {
		readline(in, 1, [&](const std::vector<int> &v) {
			aig.outputs.push_back(v[0]);
			std::cout << "output " << v[0] << std::endl;
		});
	}

	std::cout << "parsing gates ..." << std::endl;
	for (count = gateLines; !in.eof() && count != 0; --count) {
		readline(in, 3, [&](const std::vector<int> &v) {
			aig.gates.emplace_back(v[0], v[1], v[2]);
			std::cout << "gate " << v[0] << " " << v[1] << " " << v[2] << std::endl;
		});
	}

	return aig;
}
