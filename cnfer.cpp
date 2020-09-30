#include "cnfer.h"

#include <algorithm>

VecCNFer::VecCNFer()
{
}

VecCNFer::VecCNFer(const std::function<Var()>& newv) : newv(newv)
{
}

VecCNFer::~VecCNFer()
{
}

void VecCNFer::swap(VecCNFer &other)
{
    std::swap(v, other.v);
    std::swap(n, other.n);
    std::swap(newv, other.newv);
}

void VecCNFer::setRecordUsedVariables(bool yes)
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

void VecCNFer::setNewVar(std::function<Var()>&& newv)
{
    this->newv = newv;
}

void VecCNFer::addClause(const vec<Lit> &clause)
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

void VecCNFer::addUnit(Lit a)
{
    recordVariables({var(a)});
    v.push({a});
}

void VecCNFer::addBinary(Lit a, Lit b)
{
    recordVariables({var(a), var(b)});
    v.push({a,b});
}

void VecCNFer::addTernary(Lit a, Lit b, Lit c)
{
    recordVariables({var(a), var(b), var(c)});
    v.push({a,b,c});
}

Var VecCNFer::newVar()
{
    if(newv) {
        return n = newv();
    }

    return ++n;
}

void VecCNFer::copyTo(CNFer& s) const
{
    for(const auto& clause : v) {
        s.addClause(clause);
    }

    while (n > s.newVar()) {
    }
}

auto VecCNFer::copyAsTseitinExpression(CNFer& s) -> Lit
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

// check if the variable of the literal is contained in the CNF stored.
// when setRecordUsedVariables(true) was called before any clauses were added,
// this check can be performed efficiently.
auto VecCNFer::contains(Lit x) const -> bool
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

SolverCNFer::SolverCNFer(Solver &s) : s(s)
{
}

void SolverCNFer::ensureVars(Var upTo) {
    while(upTo > s.newVar()){}
} 

SolverCNFer::~SolverCNFer()
{
}

void SolverCNFer::addClause(const vec<Lit> &v)
{
    assert(v.size() != 0);
    ensureVars(var(*std::max_element(v.begin(), v.end())));
    s.addClause(v);
}

void SolverCNFer::addUnit(Lit a)
{
    ensureVars(var(a));
    s.addUnit(a);
}

void SolverCNFer::addBinary(Lit a, Lit b)
{
    ensureVars(var(std::max(a,b)));
    s.addBinary(a, b);
}

void SolverCNFer::addTernary(Lit a, Lit b, Lit c)
{
    ensureVars(var(std::max(a,std::max(b,c))));
    s.addTernary(a, b, c);
}

Var SolverCNFer::newVar()
{
    return s.newVar();
}

Solver& SolverCNFer::solver()
{
    return s;
}