#include "dimacs.h"


DimacsCNFer::DimacsCNFer(std::ostream &o) : o(o), n(0)
{
}
DimacsCNFer::~DimacsCNFer()
{
}

void DimacsCNFer::put(Lit x)
{
    o << (sign(x) ? "-" : "") << (var(x));
}

void DimacsCNFer::addClause(const vec<Lit> &v)
{
    for (auto i = 0; i != v.size(); i++) {
        put(v[i]);
        o << " ";
    }
    o << "0" << std::endl;
}

void DimacsCNFer::addUnit(Lit a)
{
    put(a);
    o << " 0" << std::endl;
}

void DimacsCNFer::addBinary(Lit a, Lit b)
{
    put(a);
    o << " ";
    put(b);
    o << " 0" << std::endl;
}

void DimacsCNFer::addTernary(Lit a, Lit b, Lit c)
{
    put(a);
    o << " ";
    put(b);
    o << " ";
    put(c);
    o << " 0" << std::endl;
}

auto DimacsCNFer::newVar() -> Var
{
    return n++;
}
