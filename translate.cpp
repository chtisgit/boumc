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

VarTranslator::VarTranslator(CNFer *s, int numVars, int k) : numVars(0)
{
	reset(s, numVars, k);
}

auto VarTranslator::reset(CNFer *s, int numVars, int k) -> void
{
	this->s = s;
	if(this->numVars != 0 && this->numVars != numVars)
		throw std::runtime_error("numVars changed");
	
	this->numVars = numVars;

	while((first = s->newVar()) < 2);
	assert(first == 2);
	falseLit = Lit(first-1, false);
	last = first + numVars * (k+2);

	//std::cout << "first=" << first << " last=" << last << std::endl;
	//s->addUnit(~falseLit);

	while(s->newVar() < last) {
	}
}

auto VarTranslator::False() const -> Lit
{
	return falseLit;
}

auto VarTranslator::True() const -> Lit
{
	return ~falseLit;
}

auto VarTranslator::toLit(int var, int step) const -> Lit
{
	auto v = var / 2;
	auto sgn = var % 2 == 1;

	// handle 0 (false) and 1 (true)
	if (v == 0)
		return sgn ? True() : False();

	const auto n = first + (v-1) + step*numVars; 

	//std::cout << "numVars = " << numVars << std::endl;
	//std::cout << "(" << var << "," << step << ") -> " << n << std::endl;

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

	//std::cout << "first=" << first << " last=" << last << std::endl;
	//std::cout << "(" << var << "," << step << ") -> " << n << std::endl;
	assert(n >= first && n < last);

	return Lit(n, sgn);
};

auto VarTranslator::timeIndex(Lit lit) const -> int
{
	if(var(lit) == 0 || numVars == 0) {
		return 0;
	}

	return (var(lit)-first)/numVars;
}

auto VarTranslator::timeShift(Lit lit, int shift) const -> Lit
{
	if(var(lit) == 0)
		throw "cannot shift true/false constants";
	
	return Lit(var(lit) + shift*numVars, sign(lit));
}

TranslationError ErrNegatedOutput{"AIGtoSATer: outputs are expected to be non-negated"};
TranslationError ErrOutputNotSingular{"AIGtoSATer: only exactly one output is supported"};

const char VertexRoot[] = "root";
const char VertexChain[] = "chain";

struct Vertex {
	const char *type = nullptr; // VertexRoot or VertexChain

	vec<Lit> c;
	vec<ClauseId> cs;
	vec<Var> xs;
	bool partOfA;

	Lit lit; // lit is a literal equivalent to pc (i.e. (lit <-> pc) is part of the interpolant) if referenced == true
	
	bool referenced = false; // lit is populated
	bool deleted = false; // deleted was called on the vertex.

	Vertex() {}
	
	Vertex(const char *type) : type(type) {}

	auto assignLit(const VecCNFer& A, const VecCNFer& B, const VarTranslator& vars, Vertex *begin, CNFer& itp) -> Lit {
		if (deleted) {
			std::cout << "warning: chain on deleted clause" << std::endl;
			//throw std::runtime_error("chain on deleted clause");
			return vars.False();
		}

		if (referenced) {
			return lit;
		}

		//std::cout << "assignLit " << type << std::endl;

		if (type == VertexRoot) {
			if (partOfA) {
				lit = Lit(itp.newVar(), false);
				
				vec<Lit> pc; // p(c) is only a disjunction in this case

				// compute globals by only adding the variables to p(c) that are in B.
				// We know they are in A because of the if condition above.
				std::for_each(c.begin(), c.end(), [&](const auto lit) {
					if(B.contains(lit)) {
						pc.push(lit);
					}
				});

				if (pc.size() != 0) {
					// add to itp: lit <-> p(c)
		
					for(auto x : pc) {
						itp.addBinary(~x, lit);
					}

					pc.push(~lit);
					itp.addClause(pc);
				}else{
					lit = vars.False();
				}
			} else {
				lit = vars.True();
			}

			referenced = true;
		}else if(type == VertexChain) {
			const auto xssz = xs.size();

			lit = begin[cs[0]].assignLit(A, B, vars, begin, itp);

			for(auto i = 0; i < xssz; i++) {
				const auto c2 = cs[i+1];
				const auto v = xs[i]; // pivot variable

				//std::cout << "pivot " << v << std::endl;

				const auto pc1 = lit;
				const auto pc2 = begin[c2].assignLit(A, B, vars, begin, itp);

				if(B.contains(Lit(v, false))) {
					// i.e. lit <-> (p(c1) ^ p(c2))
					if(pc1 == vars.True()) {
						lit = pc2;
						continue;
					}else if(pc2 == vars.True()) {
						lit = pc1;
						continue;
					}else if(pc1 == vars.False() || pc2 == vars.False()) {
						lit = vars.False();
						continue;
					}

					lit = Lit(itp.newVar(), false);
					itp.addBinary(pc1, ~lit);
					itp.addBinary(pc2, ~lit);
					itp.addTernary(lit, ~pc1, ~pc2);
				}else{
					// i.e. lit <-> (p(c1) v p(c2))
					if(pc1 == vars.False()) {
						lit = pc2;
						continue;
					}else if(pc2 == vars.False()) {
						lit = pc1;
						continue;
					}else if(pc1 == vars.True() || pc2 == vars.True()) {
						lit = vars.True();
						continue;
					}

					lit = Lit(itp.newVar(), false);
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

void AIGtoSATer::andgates(CNFer& s, VarTranslator& vars, int step) const
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

void AIGtoSATer::I(CNFer& s, VarTranslator& vars) const
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

void AIGtoSATer::T(CNFer& s, VarTranslator& vars, int step) const
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

void AIGtoSATer::F(CNFer& s, VarTranslator& vars, int from, int to) const
{
	vec<Lit> clause(to-from+1);

	for(int i = from, j = 0; i <= to; i++) {
		clause[j++] = vars.toLit(aig.outputs[0], i);
	}

	s.addClause(clause);
}

void AIGtoSATer::toSAT(CNFer& s, VarTranslator& vars, int k) const
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

auto AIGtoSATer::mcmillanMC(int k) const -> Result
{
	const auto K = k;
	const auto numVars = aig.lastLit/2;

	VarTranslator vars;
	{
		auto s = newSolver(nullptr);
		SolverCNFer scnfer{*s};
		vars.reset(&scnfer, numVars, 0);

		I(scnfer, vars);
		F(scnfer, vars, 0, 0);

		if(s->solve({vars.True()})) {
			return FAIL;
		}
	}

	VecCNFer firstR;
	I(firstR, vars);
	if(firstR.raw().size() == 0)
		firstR.addUnit(vars.True());

	for(k = 1; k <= K || K == -1 ; k++){

		// lambda for generating unique variables.
		Var newVarCounter = (k+2) * numVars + 10;
		auto newVar = [&]() -> Var {
			return ++newVarCounter;
		};

		firstR.setNewVar(newVar);
		VecCNFer R(newVar); // R transformed
		auto Rlit = firstR.copyAsTseitinExpression(R);

		// compute B here, because it does not change in inner loop.
		VecCNFer B;
		B.setRecordUsedVariables(true);
		
		for(auto i = 1; i != k; i++) {
			T(B, vars, i);
		}

		F(B, vars, 0, k);

		for(auto i = 0;;i++){
			std::cout << "ITERATION " << i << " WITH K=" << k << std::endl;
			VecCNFer A;


			R.copyTo(A);
			A.addUnit(Rlit);
			T(A, vars, 0);

			std::vector<Vertex> proof;
			VecCNFer itp(newVar);

			// lambda based proof traverser creates the refutation DAG in
			// the `proof` vector from above.
			auto partOfA = true;
			auto proofTraverser = makeTraverser([&](const auto& c){ // root
				Vertex vx(VertexRoot);
				vx.c = c;
				vx.partOfA = partOfA;

				proof.push_back(std::move(vx));
			}, [&](const auto& cs, const auto& xs){ // chain
				Vertex vx(VertexChain);

				assert(cs.size() == xs.size()+1);

				vx.cs = cs;
				vx.xs = xs;

				proof.push_back(std::move(vx));
			}, [&]{ /* done */ }, [&](ClauseId c){ // deleted
				proof[c].c.clear();
				proof[c].cs.clear();
				proof[c].xs.clear();
				proof[c].deleted = true;
			});
			auto s = newSolver(&proofTraverser);
			SolverCNFer scnfer{*s};
			vars.reset(&scnfer, numVars, k);
			
			A.copyTo(scnfer);
			partOfA = false;
			B.copyTo(scnfer);

#if 0
			DimacsCNFer abd(std::cout);
			vars.reset(&abd, numVars, k);
			std::cout << "  <A>" << std::endl;
			A.copyTo(abd);
			std::cout << "  </A>" << std::endl;
			std::cout << "  <B>" << std::endl;
			B.copyTo(abd);
			std::cout << "  </B>" << std::endl;
#endif

			if(s->solve({vars.True()})) { // SAT
				std::cout << "A ^ B SAT" << std::endl;
				if(i == 0)
					return FAIL;
				else
					break; // try again with k -> k+1
			}

			// UNSAT

			// compute interpolant ITP recursively. R2lit is the literal
			// that is equisatisfiable to the interpolant.
			auto R2lit = proof.back().assignLit(A, B, vars, proof.data(), itp);

			// shift indices k = 1 -> k = 0 in ITP
			for(auto& c : itp.raw()){
				for(auto& lit : c) {
					if (vars.timeIndex(lit) == 1) {
						lit = vars.timeShift(lit, -1);
					}
				}
			}

			// Solver that checks if R2 -> R.
			Solver rrs;
			SolverCNFer rrscnfer(rrs);
			vars.reset(&rrscnfer, numVars, k);

			itp.copyTo(rrscnfer);
			R.copyTo(rrscnfer);

			// check if R2 -> R (<=> R = FALSE & R2 = TRUE is UNSAT)
			rrscnfer.addUnit(~Rlit);
			rrscnfer.addUnit(R2lit);

#if 0
			DimacsCNFer d(std::cout);
			vars.reset(&d, numVars, k);
			std::cout << "  <ITP>" << std::endl;
			itp.copyTo(d);
			std::cout << "  </ITP>" << std::endl;
			std::cout << "  <R>" << std::endl;
			R.copyTo(d);
			std::cout << "  </R>" << std::endl;
			std::cout << "  <check if R' -> R>" << std::endl;
			d.addUnit(~Rlit);
			d.addUnit(R2lit);
			std::cout << "  </check if R' -> R>" << std::endl;
#endif

			if(!rrs.solve({vars.True()})) {
				std::cout << "R' -> R" << std::endl;
				return OK;
			}

			auto newRlit = Lit(newVar(), false);

			// (R v R2) <-> newR
			itp.copyTo(R);

			R.addBinary(~Rlit, newRlit);
			R.addBinary(~R2lit, newRlit);
			R.addTernary(~newRlit, Rlit, R2lit);

			// newR becomes R
			Rlit = newRlit;
		}
	}

	std::cout << "undecided. increase k." << std::endl;

	return FAIL;
}

auto AIGtoSATer::classicMC(int k) const -> Result
{
	auto s = newSolver(nullptr);
	SolverCNFer scnfer{*s};
	VarTranslator vars{&scnfer, aig.lastLit/2, k};

	toSAT(scnfer, vars, k);

	// true =>  SAT
	// false => UNSAT
	// SAT => E  a path to a Bad State.
	return s->solve({vars.True()}) ? FAIL : OK;
}


auto AIGtoSATer::check(int k) const -> Result
{
	if(interpolation){
		return mcmillanMC(k);
	}

	return classicMC(k);
}