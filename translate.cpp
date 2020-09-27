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

TranslationError ErrCannotTranslateVar{"VarTranslator: cannot translate var"};

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

TranslationError ErrNegatedOutput{"AIGtoSATer: outputs are expected to be non-negated"};
TranslationError ErrOutputNotSingular{"AIGtoSATer: only exactly one output is supported"};

AIGtoSATer::AIGtoSATer(const AIG &aig, CreateSolverFunc newSolver) : aig(aig), newSolver(newSolver)
{
}

void AIGtoSATer::andgates(CNFer& s, VarTranslator& vars, int step)
{
	for (const auto &gate : aig.gates) {
		auto x = vars.toLit(gate.out, step);
		auto y = vars.toLit(gate.in1, step);
		auto z = vars.toLit(gate.in2, step);

		if (gate.out % 2 != 0) {
			throw ErrNegatedOutput;
		}

		s.addBinary(~x, y);
		s.addBinary(~x, z);
		s.addTernary(~y, ~z, x);
	}
}

void AIGtoSATer::I(CNFer& s, VarTranslator& vars)
{
	// Initial latch output is zero.
	for (const auto &latch : aig.latches) {
		if (latch.first % 2 != 0) {
			throw ErrNegatedOutput;
		}

		// std::cout << "var " << latch.first << "_0 <- 0 (Latch output, input = "
		// << latch.second << ")" << std::endl;
		s.addUnit(~vars.toLit(latch.first, 0));
	}

	andgates(s, vars, 0);
}

void AIGtoSATer::T(CNFer& s, VarTranslator& vars, int step)
{
	// Latch transition function: q(n+1) <-> d(n).
	for (const auto &latch : aig.latches) {
		// std::cout << " " << latch.first << "_"  << (step+1) << " <-> " <<
		// latch.second << "_" << step << std::endl;
		s.addBinary(~vars.toLit(latch.first, step + 1), vars.toLit(latch.second, step));
		s.addBinary(vars.toLit(latch.first, step + 1), ~vars.toLit(latch.second, step));
	}

	andgates(s, vars, step + 1);
}

void AIGtoSATer::toSAT(CNFer& s, VarTranslator& vars, int k) {
	if (aig.outputs.size() != 1) {
		throw ErrOutputNotSingular;
	}

	I(s, vars);

	for (int i = 0; i != k; i++) {
		T(s, vars, i);
	}

	vec<Lit> clause(k + 1);
	for (int i = 0; i <= k; i++) {
		clause[i] = vars.toLit(aig.outputs[0], i);
	}
	s.addClause(clause);
}

void AIGtoSATer::enableInterpolation() {
	interpolation = true;
}

bool AIGtoSATer::mcmillanMC(int k)
{
	throw std::runtime_error("not implemented");
}

bool AIGtoSATer::classicMC(int k)
{
	auto s = newSolver(nullptr);
	VarTranslator vars{*s};

	toSAT(*s, vars, k);

	auto *scnfer = dynamic_cast<SolverCNFer*>(s.get());
	if(scnfer == nullptr)
		return false;

	return scnfer->solver().solve();
}


bool AIGtoSATer::check(int k)
{
	if(interpolation){
		return mcmillanMC(k);
	}

	return classicMC(k);
}