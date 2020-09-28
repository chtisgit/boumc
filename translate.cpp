#include "translate.h"

#include <algorithm>

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

VarTranslator::VarTranslator(CNFer *s, int numVars, int k) : numVars(numVars)
{
	reset(s, numVars, k);
}

auto VarTranslator::reset(CNFer *s, int numVars, int k) -> void
{
	this->s = s;

	first = s->newVar();
	last = first + numVars * (k+1);

	falseLit = Lit(first, false);
	s->addUnit(~falseLit);

	while(s->newVar() != last) {
	}
}

auto VarTranslator::toLit(int var, int step) -> Lit
{
	auto v = var / 2;
	auto sgn = var % 2 == 1;

	if (v == 0)
		return sgn ? ~falseLit : falseLit;

	const auto n = first + v + step*numVars; 
	
#if 0
	auto index = std::make_pair(v,step);
	auto it = m.find(index);
	if(it != m.end()) {
		if (it->second != n) {
			std::cout << "error!" << std::endl;
			throw "shit";
		}
	}
	m[index] = n;
#endif

	while (n > last) {
		last = s->newVar();
	}

	return Lit(n, sgn);
};

TranslationError ErrNegatedOutput{"AIGtoSATer: outputs are expected to be non-negated"};
TranslationError ErrOutputNotSingular{"AIGtoSATer: only exactly one output is supported"};

const char NodeRoot[] = "root";
const char NodeChain[] = "chain";

struct Vertex {
	const char *type; // NodeRoot or NodeChain
	int pred1, pred2; // empty for NodeRoot
	vec<Lit> lit; // literals of a clause
};

struct Trav : public ProofTraverser {
	vec<Vertex> clauses;

	void print(std::ostream &out)
	{
		out << "Proof: " << std::endl;
		for (int i = 0; i < clauses.size(); i++) {
			out << "Clause " << (i + 1) << ": ";
			for (int j = 0; j < clauses[i].lit.size(); j++) {
				out << (sign(clauses[i].lit[j]) ? "-" : "") << var(clauses[i].lit[j])
				    << ' ';
			}
			out << std::endl;
		}
	}

	template <class T, class LessThan>
	static void sortUnique(vec<T>& v, LessThan lt)
	{
		int     size = v.size();
		T*      data = v.release();
		std::sort(data, data+size, lt);
		std::unique(data, data+size);
		v.~vec<T>();
		new (&v) vec<T>(data, size);
	}

	template <class T>
	static void sortUnique(vec<T>& v)
	{
		sortUnique(v, std::less<T>());
	}


	auto newClause(const char *type) -> Vertex&
	{
		clauses.push();
		auto& n = clauses.last();
		n.type = type;

		return n;
	}

	static void resolve(vec<Lit> &main, vec<Lit> &other, Var x)
	{
		Lit p;
		bool ok1 = false, ok2 = false;
		for (int i = 0; i < main.size(); i++) {
			if (var(main[i]) == x) {
				ok1 = true, p = main[i];
				main[i] = main.last();
				main.pop();
				break;
			}
		}

		for (int i = 0; i < other.size(); i++) {
			if (var(other[i]) != x)
				main.push(other[i]);
			else {
				if (p != ~other[i])
					printf("PROOF ERROR! Resolved on variable with SAME "
					       "polarity in both clauses: %d\n",
					       x + 1);
				ok2 = true;
			}
		}

		if (!ok1 || !ok2)
			printf("PROOF ERROR! Resolved on missing variable: %d\n", x + 1);

		sortUnique(main);
	}

	void root(const vec<Lit> &c)
	{
		c.copyTo(newClause(NodeRoot).lit);
	}

	void chain(const vec<ClauseId> &cs, const vec<Var> &xs)
	{
		clauses.push();
		auto& n = newClause(NodeChain);
		clauses[cs[0]].lit.copyTo(n.lit);
        for (int i = 0; i < xs.size(); i++)
            resolve(n.lit, clauses[cs[i+1]].lit, xs[i]);
	}

	void deleted(ClauseId c)
	{
		clauses[c].lit.clear();
	}

	virtual void done()
	{
		std::cout << "done()" << std::endl;
	}

	virtual ~Trav()
	{
	}
};

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

void AIGtoSATer::F(CNFer& s, VarTranslator& vars, int from, int to)
{
	vec<Lit> clause(to-from+1);

	for(int i = from, j = 0; i <= to; i++) {
		clause[j++] = vars.toLit(aig.outputs[0], i);
	}

	s.addClause(clause);
}

void AIGtoSATer::toSAT(CNFer& s, VarTranslator& vars, int k)
{
	if (aig.outputs.size() != 1) {
		throw ErrOutputNotSingular;
	}

	I(s, vars);

	for (int i = 0; i != k; i++) {
		T(s, vars, i);
	}

	F(s, vars, 0, k);
}

void AIGtoSATer::enableInterpolation() {
	interpolation = true;
}

bool AIGtoSATer::mcmillanMC(int k)
{
	{
		auto s = newSolver(nullptr);
		SolverCNFer scnfer{*s};
		VarTranslator vars{&scnfer, aig.lastLit/2, k};


		I(scnfer, vars);
		F(scnfer, vars, 0, 0);

		if(s->solve()) {
			return true;
		}
	}

	vec<vec<Lit>> R;


	throw std::runtime_error("not implemented");
}

bool AIGtoSATer::classicMC(int k)
{
	auto s = newSolver(nullptr);
	SolverCNFer scnfer{*s};
	VarTranslator vars{&scnfer, aig.lastLit/2, k};

	toSAT(scnfer, vars, k);

	return s->solve();
}


bool AIGtoSATer::check(int k)
{
	if(interpolation){
		return mcmillanMC(k);
	}

	return classicMC(k);
}