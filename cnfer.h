#pragma once

#include "MiniSat-p_v1.14/Solver.h"

#include <functional>
#include <memory>
#include <set>

// CNFer is an interface for objects storing or processing
// clauses of a CNF.
struct CNFer {
	virtual void addClause(const vec<Lit> &) = 0;
	virtual void addUnit(Lit) = 0;
	virtual void addBinary(Lit, Lit) = 0;
	virtual void addTernary(Lit, Lit, Lit) = 0;
	virtual auto newVar() -> Var = 0;
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
	VecCNFer();
	VecCNFer(const std::function<Var()>& newv);
	virtual ~VecCNFer();

	void swap(VecCNFer &other);

	void setRecordUsedVariables(bool yes);
	void setNewVar(std::function<Var()>&& newv);

	virtual void addClause(const vec<Lit> &clause);
	virtual void addUnit(Lit a);
	virtual void addBinary(Lit a, Lit b);
	virtual void addTernary(Lit a, Lit b, Lit c);
	virtual auto newVar() -> Var;

	// copy all clauses to another CNFer.
	void copyTo(CNFer& s) const;

	// adds the saved formula as (formula iff lit) to the CNFer. The lit
	// is returned. When added to the CNFer, it holds an equisatisfiable formula
	// compared to the one saved.
	auto copyAsTseitinExpression(CNFer& s) -> Lit;

	// access the vec containing the clauses of the CNF.
	auto raw() -> container_type&
	{
		return v;
	}

	// check if the variable of the literal is contained in the CNF stored.
	// when setRecordUsedVariables(true) was called before any clauses were added,
	// this check can be performed efficiently.
	auto contains(Lit x) const -> bool;
};

// SolverCNFer is an adapter that is used to plug a MiniSat Solver into a
// function parameter that takes a CNFer.
class SolverCNFer : public CNFer {
	Solver &s;

	void ensureVars(Var upTo);

public:
	SolverCNFer(Solver &s);
	virtual ~SolverCNFer();

	virtual void addClause(const vec<Lit> &v);
	virtual void addUnit(Lit a);
	virtual void addBinary(Lit a, Lit b);
	virtual void addTernary(Lit a, Lit b, Lit c);
	virtual auto newVar() -> Var;

	auto solver() -> Solver&;
};
