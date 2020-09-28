#pragma once

#include <exception>
#include <map>
#include <memory>

#include "MiniSat-p_v1.14/Solver.h"
#include "aag.h"

struct TranslationError : public std::runtime_error {
	explicit TranslationError(const char *s);
	explicit TranslationError(const std::string &s);
	virtual ~TranslationError();
};

struct CNFer {
	virtual void addClause(const vec<Lit> &) = 0;
	virtual void addUnit(Lit) = 0;
	virtual void addBinary(Lit, Lit) = 0;
	virtual void addTernary(Lit, Lit, Lit) = 0;
	virtual Var newVar() = 0;
	virtual ~CNFer(){};
};

class VecCNFer : public CNFer {
	using container_type = vec<vec<Lit>>;
	container_type &v;
	Var n = 0;

public:
	VecCNFer(container_type& v) : v(v) {}

	virtual ~VecCNFer() {}

	virtual void addClause(const vec<Lit> &clause)
	{
		v.push(clause);
	}

	virtual void addUnit(Lit a)
	{
		vec<Lit> clause(1);
		clause[0] = a;
		v.push(clause);
	}

	virtual void addBinary(Lit a, Lit b)
	{
		vec<Lit> clause(2);
		clause[0] = a;
		clause[1] = b;
		v.push(clause);
	}

	virtual void addTernary(Lit a, Lit b, Lit c)
	{
		vec<Lit> clause(3);
		clause[0] = a;
		clause[1] = b;
		clause[2] = c;
		v.push(clause);
	}

	virtual Var newVar()
	{
		return ++n;
	}
};

class SolverCNFer : public CNFer {
	Solver &s;

public:
	SolverCNFer(Solver &s) : s(s) {}

	virtual ~SolverCNFer()
	{
	}

	virtual void addClause(const vec<Lit> &v)
	{
		s.addClause(v);
	}

	virtual void addUnit(Lit a)
	{
		s.addUnit(a);
	}

	virtual void addBinary(Lit a, Lit b)
	{
		s.addBinary(a, b);
	}

	virtual void addTernary(Lit a, Lit b, Lit c)
	{
		s.addTernary(a, b, c);
	}

	virtual Var newVar()
	{
		return s.newVar();
	}

	Solver& solver()
	{
		return s;
	}
};

class DimacsCNFer : public CNFer {
	std::ostream &o;
	int n;

public:
	DimacsCNFer(std::ostream &o) : o(o), n(0)
	{
	}
	virtual ~DimacsCNFer()
	{
	}

	void put(Lit x)
	{
		o << (sign(x) ? "-" : "") << var(x);
	}

	virtual void addClause(const vec<Lit> &v)
	{
		for (auto i = 0; i != v.size(); i++) {
			put(v[i]);
			o << " ";
		}
		o << std::endl;
	}

	virtual void addUnit(Lit a)
	{
		put(a);
		o << std::endl;
	}

	virtual void addBinary(Lit a, Lit b)
	{
		put(a);
        o << " ";
		put(b);
		o << std::endl;
	}

	virtual void addTernary(Lit a, Lit b, Lit c)
	{
		put(a);
        o << " ";
		put(b);
        o << " ";
		put(c);
		o << std::endl;
	}

	virtual Var newVar()
	{
		return ++n;
	}
};

extern TranslationError ErrCannotTranslateVar;

class VarTranslator {
	CNFer *s;
	Lit falseLit;
	int first, last;
	int numVars;
	std::map<std::pair<int,int>, int> m;

public:
	explicit VarTranslator(CNFer *s, int numVars, int k);
	auto reset(CNFer *s, int numVars, int k) -> void;
	auto toLit(int var, int step) -> Lit;
};

extern TranslationError ErrNegatedOutput;
extern TranslationError ErrOutputNotSingular;

using CreateSolverFunc = std::unique_ptr<Solver> (*)(ProofTraverser*);

class AIGtoSATer {
	const AIG &aig;
	CreateSolverFunc newSolver;
	bool interpolation = false;

	void andgates(CNFer& s, VarTranslator& vars, int step);
	bool mcmillanMC(int k);
	bool classicMC(int k);

public:
	AIGtoSATer(const AIG &aig, CreateSolverFunc newSolver);

	void I(CNFer& s, VarTranslator& vars);
	void T(CNFer& s, VarTranslator& vars, int step);
	void F(CNFer& s, VarTranslator& vars, int from, int to);

	void toSAT(CNFer& s, VarTranslator& vars, int k);

	void enableInterpolation();

	bool check(int k);
};
