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

VarTranslator::VarTranslator() : s(nullptr), falseLit(0), first(0), last(0), numVars(0)
{
}

VarTranslator::VarTranslator(CNFer *s, int numVars, int k) : numVars(numVars)
{
	reset(s, numVars, k);
}

auto VarTranslator::reset(CNFer *s, int numVars, int k) -> void
{
	this->s = s;

	while((first = s->newVar()) == 0);
	last = first + numVars * (k+1);

	falseLit = Lit(first, false);
	s->addUnit(~falseLit);

	while(s->newVar() < last) {
	}
}

auto VarTranslator::False() -> Lit
{
	return falseLit;
}

auto VarTranslator::True() -> Lit
{
	return ~falseLit;
}

auto VarTranslator::toLit(int var, int step) -> Lit
{
	auto v = var / 2;
	auto sgn = var % 2 == 1;

	// handle 0 (false) and 1 (true)
	if (v == 0)
		return sgn ? True() : False();

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

static auto isGlobal(Lit lit, const VecCNFer& A, const VecCNFer& B)
{
	return A.contains(lit) && B.contains(lit);
}

static auto globals(const vec<Lit>& c, const VecCNFer& A, const VecCNFer& B, vec<Lit>& res) {
	for(const auto lit : c) {
		if(isGlobal(lit, A, B)) {
			res.push(lit);
		}
	}
}


const char NodeRoot[] = "root";
const char NodeChain[] = "chain";

struct Vertex {
	const char *type = nullptr; // NodeRoot or NodeChain

	vec<Lit> c;
	vec<ClauseId> cs;
	vec<Var> xs;

	Lit lit; // lit is a literal equivalent to pc (i.e. (lit <-> pc) is part of the interpolant) if referenced == true
	
	bool referenced = false; // lit is populated

	Vertex() {}
	
	Vertex(const char *type) : type(type) {}

	auto assignLit(const VecCNFer& A, const VecCNFer& B, Vertex *begin, Lit True, CNFer& itp) -> Lit {
		if (referenced) {
			return lit;
		}

		//std::cout << "assignLit " << type << std::endl;

		if (type == NodeRoot) {
			if (A.containsClause(c)) {
				lit = Lit(itp.newVar(), false);
				
				vec<Lit> pc; // p(c) is only a disjunction in this case
				globals(c, A, B, pc);

				// add to itp: lit <-> p(c)
				for(auto x : pc) {
					itp.addBinary(~x, lit);
				}

				vec<Lit> clause{pc};
				clause.push(~lit);
				itp.addClause(clause);
			} else {
				lit = True;
			}

			referenced = true;
		}else if(type == NodeChain) {
			const auto xssz = xs.size();

			lit = begin[cs[0]].assignLit(A, B, begin, True, itp);

			for(auto i = 0; i < xssz; i++) {
				const auto c2 = cs[i+1];
				const auto v = xs[i];

				const auto pc1 = lit;
				const auto pc2 = begin[c2].assignLit(A, B, begin, True, itp);
				lit = Lit(itp.newVar(), false);

				if(isGlobal(Lit(v, false), A, B)) {
					// i.e. lit <-> (p(c1) ^ p(c2))
					itp.addBinary(pc1, ~lit);
					itp.addBinary(pc2, ~lit);
					itp.addTernary(lit, ~pc1, ~pc2);
				}else{
					// i.e. lit <-> (p(c1) v p(c2))
					itp.addBinary(~pc1, lit);
					itp.addBinary(~pc2, lit);
					itp.addTernary(~lit, pc1, pc2);
				}
			}

			referenced = true;
		}else{
			throw std::domain_error("type of vertex: neither root nor chain");
		}
		
		assert(referenced);
		
		return lit;
	}
};


static std::unique_ptr<Solver> newSolver(ProofTraverser *trav)
{
	auto s = std::make_unique<Solver>();
	s->proof = nullptr;
	
	// proof needs to be assigned before newVar() is called on the Solver...
	if(trav != nullptr)
		s->proof = new Proof{*trav};

	return s;
}

AIGtoSATer::AIGtoSATer(const AIG &aig) : aig(aig)
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
	const auto K = k;

	VarTranslator vars;
	{
		auto s = newSolver(nullptr);
		SolverCNFer scnfer{*s};
		vars.reset(&scnfer, aig.lastLit/2, K);

		I(scnfer, vars);
		F(scnfer, vars, 0, 0);

		if(s->solve()) {
			return true;
		}
	}

	for(auto k = 1; k <= K; k++){
		Var newVarCounter = K * aig.lastLit/2;
		auto newVar = [&]() -> Var {
			return ++newVarCounter;
		};

		VecCNFer R(newVar);
		I(R, vars);
		if(R.raw().size() == 0)
			R.addUnit(vars.True());

		// TODO: transform R here.

		for(auto i = 0;;i++){
			std::cout << "ITERATION " << i << " WITH K=" << k << std::endl;
			VecCNFer A, B;

			R.copyTo(A);
			T(A, vars, 0);

			for(auto i = 1; i != k; i++) {
				T(B, vars, i);
			}

			F(B, vars, 0, k);

			std::vector<Vertex> proof;
			VecCNFer itp(newVar);

			auto deletedCalled = false;
			auto proofTraverser = makeTraverser([&](const auto& c){ // root
				Vertex vx(NodeRoot);
				vx.c = c;
				proof.push_back(std::move(vx));
			}, [&](const auto& cs, const auto& xs){ // chain
				Vertex vx(NodeChain);

				assert(cs.size() == xs.size()+1);

				vx.cs = cs;
				vx.xs = xs;

				proof.push_back(std::move(vx));
			}, [&]{ // done
			}, [&](ClauseId c){ // deleted
				proof[c].c.clear();
				proof[c].cs.clear();
				proof[c].xs.clear();

				deletedCalled = true;
			});
			auto s = newSolver(&proofTraverser);
			SolverCNFer scnfer{*s};
			vars.reset(&scnfer, aig.lastLit/2, K);
			
			std::cout << index(vars.True()) << " " << var(vars.True()) << " " << sign(vars.True()) << std::endl;

			A.copyTo(scnfer);
			B.copyTo(scnfer);

#if 0
			DimacsCNFer abd(std::cout);
			vars.reset(&abd, aig.lastLit/2, K);
			std::cout << "  A" << std::endl;
			A.copyTo(abd);
			std::cout << "  B" << std::endl;
			B.copyTo(abd);
#endif

			if(s->solve()) { // SAT
				std::cout << "A ^ B SAT" << std::endl;
				if(i == 0)
					return true;
				else
					break; // try again with k -> k+1
			}

			// UNSAT
			if (deletedCalled) 
				throw std::runtime_error("deleted called");

			auto R2lit = proof.back().assignLit(A, B, proof.data(), vars.True(), itp);

			VecCNFer Rt(newVar); // R transformed
			auto Rtlit = R.copyAsTseitinExpression(Rt);

			Solver rrs;
			SolverCNFer rrscnfer(rrs);
			vars.reset(&rrscnfer, aig.lastLit/2, K);

			itp.copyTo(rrscnfer);
			R.copyTo(rrscnfer);

			// check if R2 -> R (<=> R -> R2 is UNSAT)
			
			// which one?
			//rrscnfer.addUnit(~Rtlit);
			//rrscnfer.addUnit(R2lit);
			rrscnfer.addBinary(~Rtlit, R2lit);


#if 0
			DimacsCNFer d(std::cout);
			vars.reset(&d, aig.lastLit/2, K);
			std::cout << "  ITP" << std::endl;
			itp.copyTo(d);
			std::cout << "  Rt" << std::endl;
			Rt.copyTo(d);
			std::cout << "  check R -> R'" << std::endl;
			d.addUnit(~Rtlit);
			d.addUnit(R2lit);
#endif

			if(!rrs.solve()) {
				std::cout << "R' -> R" << std::endl;
				return false;
			}

			// Rt v R2 -> Rt
			itp.copyTo(Rt);
			Rt.addBinary(Rtlit, R2lit);

			R.swap(Rt);

			//if(i == 1)
			//	throw std::runtime_error("stop");
		}
	}

	std::cout << "undecided. increase k." << std::endl;

	return true;
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