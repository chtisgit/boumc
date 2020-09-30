#pragma once

#include <algorithm>
#include <exception>
#include <functional>
#include <map>
#include <set>
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
	
	std::unique_ptr<std::set<Var>> litset;
	
	template<typename It>
	void recordVariables(It begin, It end)
	{
		if(!litset)
			return;
		litset->insert(begin, end);
	}

	void recordVariables(const std::initializer_list<Var>& list)
	{
		return recordVariables(list.begin(), list.end());
	}

public:
	VecCNFer() {};
	VecCNFer(const std::function<Var()>& newv) : newv(newv) {}

	void swap(VecCNFer &other)
	{
		std::swap(v, other.v);
		std::swap(n, other.n);
		std::swap(newv, other.newv);
	}

	void setRecordUsedVariables(bool yes)
	{
		if (yes) {
			if (!litset) {
				litset = std::make_unique<std::set<Var>>();
			}
			return;
		}

		if (litset) {
			litset.reset(nullptr);
		}
	}

	virtual ~VecCNFer() {}

	void setNewVar(std::function<Var()>&& newv)
	{
		this->newv = newv;
	}

	virtual void addClause(const vec<Lit> &clause)
	{
		if(litset){
			vec<Var> vars(clause.size());
			std::transform(clause.cbegin(), clause.cend(), vars.begin(), [](const auto lit) -> Var {
				return var(lit);
			});
			recordVariables(vars.cbegin(), vars.cend());
		}

		v.push(clause);
	}

	virtual void addUnit(Lit a)
	{
		recordVariables({var(a)});
		v.push({a});
	}

	virtual void addBinary(Lit a, Lit b)
	{
		recordVariables({var(a), var(b)});
		v.push({a,b});
	}

	virtual void addTernary(Lit a, Lit b, Lit c)
	{
		recordVariables({var(a), var(b), var(c)});
		v.push({a,b,c});
	}

	virtual Var newVar()
	{
		if(newv) {
			return n = newv();
		}

		return ++n;
	}

	// copy all clauses to another CNFer.
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
	auto copyAsTseitinExpression(CNFer& s) -> Lit
	{
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

	// access the vec containing the clauses of the CNF.
	auto raw() -> container_type&
	{
		return v;
	}

	// check if the variable of the literal is contained in the CNF stored.
	// when setRecordUsedVariables(true) was called before any clauses were added,
	// this check can be performed efficiently.
	auto contains(Lit x) const -> bool
	{
		const auto xv = var(x);

		if(litset) {
			return litset->find(xv) != litset->cend();
		}

		for(const auto& clause : v) {
			for(const auto lit : clause){
				if (var(lit) == xv) {
					return true;
				}
			}
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

	// This constructor is short-hand for default + reset.
	explicit VarTranslator(CNFer *s, int numVars, int k);

	// reset reserves the variables needed for direct mappings from AIGER
	// in the CNFer. It also saves the number of variables to compute them.
	auto reset(CNFer *s, int numVars, int k) -> void;

	// toLit is used to convert a literal from the AIGER model (var) together
	// with its time index (step) to a literal usable in a CNFer.
	auto toLit(int var, int step) const -> Lit;

	// False returns the literal reserved for the constant false. Remember
	// to add True() to the assumptions of a solver.
	auto False() const -> Lit;

	// True returns the literal reserved for the constant true. Remember to
	// add it to the assumptions of a solver.
	auto True() const -> Lit;

	// timeIndex returns the time index of a literal.
	auto timeIndex(Lit lit) const -> int;

	// timeShift shifts the literals time index.
	auto timeShift(Lit lit, int shift) const -> Lit;
};

extern TranslationError ErrNegatedOutput;
extern TranslationError ErrOutputNotSingular;

// AIGtoSATer is the actual model checker.
class AIGtoSATer {
public:
	enum Result { _, OK, FAIL };

private:
	const AIG &aig;
	bool interpolation = false;

	// andgates adds the clauses representing the AND gates of the
	// AIGER model to the given CNFer. The VarTranslator is used to
	// translate AIGER literals to CNFer/Solver literals. The step is
	// the time index (starting at 0). 
	void andgates(CNFer& s, VarTranslator& vars, int step) const;
	
	// mcmillanMC performs unbounded model checking based on the McMillan paper
	// up to a bound k. To disable the bound, set k == -1.
	auto mcmillanMC(int k) const -> Result;

	// classicMC performs bounded model checking with bound k.
	auto classicMC(int k) const -> Result;

public:

	// Construct the model checker based on a parsed AIGER representation.
	AIGtoSATer(const AIG &aig);

	// I adds the initial state (and gate outputs at k=0 and zero initialized
	// latch outputs) to the given CNFer.
	void I(CNFer& s, VarTranslator& vars) const;

	// T adds a transition function (step is the time index) to the CNFer.
	void T(CNFer& s, VarTranslator& vars, int step) const;

	// F adds the final condition (the bad state detector from the AIGER
	// model) to the CNFer. It adds the variable with all time indices
	// starting at `from` up to and including `to`.
	void F(CNFer& s, VarTranslator& vars, int from, int to) const;

	// toSAT translates the AIGER model into a bounded model checking CNF, whose
	// clauses are added to the given CNFer. This is used internally in classicMC,
	// but it can also used together with a DimacsCNFer to view the generated formula.
	void toSAT(CNFer& s, VarTranslator& vars, int k) const;

	// enableInterpolation enables interpolation mode in the AIGtoSATer.
	// It cannot be disabled after that.
	void enableInterpolation();

	// check runs the model checker with a bound k. When interpolation is turned on,
	// k can be -1 in which case there is no upper bound. 
	auto check(int k) const -> Result;
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