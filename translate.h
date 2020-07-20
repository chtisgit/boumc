#pragma once

#include <exception>
#include <map>

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
	Solver &s;

public:
	SolverCNFer(Solver &s) : s(s)
	{
	}

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

class VarTranslator {
	CNFer &s;
	std::map<std::pair<int, int>, Lit> varmap;

public:
	explicit VarTranslator(CNFer &s);
	auto toLit(int var, int step) -> Lit;
};

class AIGtoSATer {
	const AIG &aig;
	CNFer &s;
	VarTranslator vars;
	const int k;

	void andgates(int step);
	void I();
	void T(int step);

public:
	AIGtoSATer(const AIG &aig, CNFer &s, int k);

	void toSAT();
};
