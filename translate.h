#pragma once

#include <algorithm>
#include <exception>
#include <functional>
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
	container_type v;
	Var n = 0;

	std::function<Var()> newv;

public:
	VecCNFer() {};
	VecCNFer(const std::function<Var()>& newv) : newv(newv) {}

	void swap(VecCNFer &other) {
		std::swap(v, other.v);
		std::swap(n, other.n);
		std::swap(newv, other.newv);
	}

	virtual ~VecCNFer() {}

	void setNewVar(std::function<Var()>&& newv) {
		this->newv = newv;
	}

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
		if(newv) {
			return n = newv();
		}

		return ++n;
	}

	void copyTo(CNFer& s) const
	{
		for(const auto& clause : v) {
			s.addClause(clause);
		}

		while (n > s.newVar()) {
		}
	}

	// adds the saved formula as (formula iff lit) to the CNFer. The lit
	// is returned. When added to the CNFer, it holds an equisatisfiable formula
	// compared to the one saved.
	auto copyAsTseitinExpression(CNFer& s) -> Lit {
		vec<Lit> lits;
		
		for(const auto& clause : v) {
			auto lit = Lit(newVar(), false);

			// every var of clause implies lit
			for(const auto& x : clause) {
				s.addBinary(~x, lit);
			}

			// lit implies the clause
			auto c{clause};
			c.push(~lit);
			s.addClause(c);

			lits.push(lit);
		}

		if (lits.size() == 1) {
			return lits[0];
		}

		auto lit = Lit(newVar(), false);
		for(const auto& x : lits) {
			s.addBinary(~lit, x);
		}
		
		for(auto& x : lits) {
			x = ~x;
		}
		lits.push(lit);
		s.addClause(lits);

		return lit;
	}

	auto raw() -> container_type&
	{
		return v;
	}

	auto contains(Lit x) const -> bool
	{
		const auto xv = var(x);
		for(const auto& clause : v) {
			for(const auto lit : clause){
				if (var(lit) == xv) {
					return true;
				}
			}
		}

		return false;
	}

	auto containsClause(const vec<Lit>& clause) const -> bool
	{
		auto vsz = v.size();
		for (auto i = 0; i != vsz; i++) {
			auto visz = v[i].size();
			if (visz != clause.size())
				continue;
			
			auto ok = true;
			for (auto j = 0; j != visz; j++) {
				if (v[i][j] != clause[j]) {
					ok = false;
					break;
				}
			}

			if(ok)
				return true;
		}

		return false;
	}
};

class SolverCNFer : public CNFer {
	Solver &s;

public:
	SolverCNFer(Solver &s) : s(s) {}

	void ensureVars(Var upTo) {
		while(upTo > s.newVar()){}
	} 

	virtual ~SolverCNFer()
	{
	}

	virtual void addClause(const vec<Lit> &v)
	{
		assert(v.size() != 0);
		ensureVars(var(*std::max_element(v.begin(), v.end())));
		s.addClause(v);
	}

	virtual void addUnit(Lit a)
	{
		ensureVars(var(a));
		s.addUnit(a);
	}

	virtual void addBinary(Lit a, Lit b)
	{
		ensureVars(var(std::max(a,b)));
		s.addBinary(a, b);
	}

	virtual void addTernary(Lit a, Lit b, Lit c)
	{
		ensureVars(var(std::max(a,std::max(b,c))));
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
		o << (sign(x) ? "-" : "") << (var(x));
	}

	virtual void addClause(const vec<Lit> &v)
	{
		for (auto i = 0; i != v.size(); i++) {
			put(v[i]);
			o << " ";
		}
		o << "0" << std::endl;
	}

	virtual void addUnit(Lit a)
	{
		put(a);
		o << " 0" << std::endl;
	}

	virtual void addBinary(Lit a, Lit b)
	{
		put(a);
        o << " ";
		put(b);
		o << " 0" << std::endl;
	}

	virtual void addTernary(Lit a, Lit b, Lit c)
	{
		put(a);
        o << " ";
		put(b);
        o << " ";
		put(c);
		o << " 0" << std::endl;
	}

	virtual Var newVar()
	{
		return n++;
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
	explicit VarTranslator();
	explicit VarTranslator(CNFer *s, int numVars, int k);
	auto reset(CNFer *s, int numVars, int k) -> void;
	auto toLit(int var, int step) const -> Lit;
	auto False() const -> Lit;
	auto True() const -> Lit;
	auto bounds(int step) const -> std::pair<int,int>;
	auto timeIndex(Lit lit) const -> int;
	auto timeShift(Lit lit, int shift) const -> Lit;
};

extern TranslationError ErrNegatedOutput;
extern TranslationError ErrOutputNotSingular;

class AIGtoSATer {
	const AIG &aig;
	bool interpolation = false;

	void andgates(CNFer& s, VarTranslator& vars, int step) const;
	bool mcmillanMC(int k) const;
	bool classicMC(int k) const;

public:
	AIGtoSATer(const AIG &aig);

	void I(CNFer& s, VarTranslator& vars) const;
	void T(CNFer& s, VarTranslator& vars, int step) const;
	void F(CNFer& s, VarTranslator& vars, int from, int to) const;

	void toSAT(CNFer& s, VarTranslator& vars, int k) const;

	void enableInterpolation();

	bool check(int k) const;
};

template<class RootFunc, class ChainFunc, class DoneFunc, class DeletedFunc>
struct LambdaTraverser : public ProofTraverser {
	RootFunc rf;
	ChainFunc cf;
	DoneFunc donef;
	DeletedFunc delf;

	LambdaTraverser(RootFunc rf, ChainFunc cf, DoneFunc donef, DeletedFunc delf) : rf(rf), cf(cf), donef(donef), delf(delf)
	{
	}

	virtual void root(const vec<Lit> &c)
	{
		rf(c);
	}

	virtual void chain(const vec<ClauseId> &cs, const vec<Var> &xs)
	{
		cf(cs, xs);
	}

	virtual void done()
	{
		donef();
	}

	virtual void deleted(ClauseId c)
	{
		delf(c);
	}

	virtual ~LambdaTraverser()
	{
	}
};


template<class RootFunc, class ChainFunc, class DoneFunc, class DeletedFunc>
auto makeTraverser(RootFunc rf, ChainFunc cf, DoneFunc donef, DeletedFunc delf) -> LambdaTraverser<RootFunc, ChainFunc, DoneFunc, DeletedFunc> {
	return LambdaTraverser<RootFunc, ChainFunc, DoneFunc, DeletedFunc>(rf, cf, donef, delf);
}