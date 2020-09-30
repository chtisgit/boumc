#pragma once
#include "cnfer.h"

#include <ostream>

// DimacsCNFer can be used to print mostly DIMACS formated CNF on-the-fly. It
// does not generate the first line "p cnf x y" though, because it writes the
// data to the given ostream as it gets it, so the DimacsCNFer cannot know
// how many clauses are to come, and how many variables there will be.
// This is not used for the actual model checking, it can still be used to
// look at formulas. 
class DimacsCNFer : public CNFer {
	std::ostream &o;
	int n;

	void put(Lit x);

public:
	DimacsCNFer(std::ostream &o);
	virtual ~DimacsCNFer();

	virtual void addClause(const vec<Lit> &v);
	virtual void addUnit(Lit a);
	virtual void addBinary(Lit a, Lit b);
	virtual void addTernary(Lit a, Lit b, Lit c);
	virtual auto newVar() -> Var;
};
