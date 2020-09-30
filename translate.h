#pragma once

#include <exception>
#include <map>
#include <memory>

#include "MiniSat-p_v1.14/Solver.h"
#include "aag.h"
#include "cnfer.h"

struct TranslationError : public std::runtime_error {
	explicit TranslationError(const char *s);
	explicit TranslationError(const std::string &s);
	virtual ~TranslationError();
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