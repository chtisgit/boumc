#include "translate.h"

TranslationError::TranslationError(const char *s) : std::runtime_error(s)
{
}
TranslationError::TranslationError(const std::string &s) : std::runtime_error(s)
{
}
TranslationError::~TranslationError()
{
}

VarTranslator::VarTranslator(CNFer &s) : s(s)
{
	auto falseLit = Lit(s.newVar(), false);
	varmap[std::make_pair(0, 0)] = falseLit;
	s.addUnit(~falseLit);
}

auto VarTranslator::toLit(int var, int step) -> Lit
{
	auto v = var / 2;
	auto sgn = var % 2 == 1;

	if (v == 0)
		step = 0;

	auto p = std::make_pair(v, step);
	auto it = varmap.find(p);
	if (it == varmap.end()) {
		int t = s.newVar();
		varmap[p] = Lit(t, false);
		return Lit(t, sgn);
	}

	return sgn ? ~it->second : it->second;
};

AIGtoSATer::AIGtoSATer(const AIG &aig, CNFer &s, int k) : aig(aig), s(s), vars(s), k(k)
{
}

void AIGtoSATer::andgates(int step)
{
	for (const auto &gate : aig.gates) {
		auto x = vars.toLit(gate.out, step);
		auto y = vars.toLit(gate.in1, step);
		auto z = vars.toLit(gate.in2, step);

		if (gate.out % 2 != 0) {
			throw TranslationError("outputs are expected to be non-negated");
		}

		s.addBinary(~x, y);
		s.addBinary(~x, z);
		s.addTernary(~y, ~z, x);
	}
}

void AIGtoSATer::I()
{
	// Initial latch output is zero.
	for (const auto &latch : aig.latches) {
		if (latch.first % 2 != 0) {
			throw TranslationError("outputs are expected to be non-negated");
		}

		// std::cout << "var " << latch.first << "_0 <- 0 (Latch output, input = "
		// << latch.second << ")" << std::endl;
		s.addUnit(~vars.toLit(latch.first, 0));
	}

	andgates(0);
}

void AIGtoSATer::T(int step)
{
	// Latch transition function: q(n+1) <-> d(n).
	for (const auto &latch : aig.latches) {
		// std::cout << " " << latch.first << "_"  << (step+1) << " <-> " <<
		// latch.second << "_" << step << std::endl;
		s.addBinary(~vars.toLit(latch.first, step + 1), vars.toLit(latch.second, step));
		s.addBinary(vars.toLit(latch.first, step + 1), ~vars.toLit(latch.second, step));
	}

	andgates(step + 1);
}

void AIGtoSATer::toSAT()
{
	if (aig.outputs.size() != 1) {
		throw TranslationError("only exactly one output is supported");
	}

	I();

	for (int i = 0; i != k; i++) {
		T(i);
	}

	vec<Lit> clause(k + 1);
	for (int i = 0; i <= k; i++) {
		clause[i] = vars.toLit(aig.outputs[0], i);
	}
	s.addClause(clause);
}
