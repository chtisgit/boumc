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

class SolverCNFer : public CNFer {
	Solver s;
	ProofTraverser *pt;

public:
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

	void withProofTraverser(ProofTraverser* trav) {
		pt = nullptr;
		s.proof = nullptr;
		if(trav == nullptr)
			return;

		// proof needs to be assigned before newVar() is called on the Solver...
		s.proof = new Proof{*trav};
		pt = trav;
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
	CNFer &s;
	std::map<std::pair<int, int>, Lit> varmap;

public:
	explicit VarTranslator(CNFer &s);
	auto toLit(int var, int step) -> Lit;
};

extern TranslationError ErrNegatedOutput;
extern TranslationError ErrOutputNotSingular;

using CreateSolverFunc = std::unique_ptr<CNFer> (*)(ProofTraverser*);

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
	void toSAT(CNFer& s, VarTranslator& vars, int k);

	void enableInterpolation();

	bool check(int k);
};
