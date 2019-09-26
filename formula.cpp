#include "formula.h"
#include <map>

Formula::~Formula()
{
}

auto Formula::String() const -> std::string
{
	return "<Formula>";
}

auto Formula::Free() -> void
{
	throw std::runtime_error("Formula::Free");
}

auto Formula::Invert() -> Formula *
{
	throw std::runtime_error("Formula::Invert");
}

auto Formula::Copy() const -> Formula *
{
	throw std::runtime_error("Formula::Copy");
}

auto True::String() const -> std::string
{
	return "T";
}

auto True::Free() -> void
{
}

auto True::Invert() -> Formula *
{
	return &FalseVal;
}

auto True::Copy() const -> Formula *
{
	return &TrueVal;
}

auto False::String() const -> std::string
{
	return "F";
}

auto False::Free() -> void
{
}

auto False::Invert() -> Formula *
{
	return &TrueVal;
}

auto False::Copy() const -> Formula *
{
	return &FalseVal;
}

FormulaVar::FormulaVar(int var_) : var(var_)
{
}

auto FormulaVar::String() const -> std::string
{
	char buf[20];
	sprintf(buf, "%d", var);
	return buf;
}

auto FormulaVar::Free() -> void
{
	delete this;
}

auto FormulaVar::Invert() -> Formula *
{
	return dynamic_cast<Formula *>(new Negate(this));
}

auto FormulaVar::Copy() const -> Formula *
{
	return new FormulaVar(var);
}

BinaryOp::BinaryOp(char op_) : op(op_)
{
	std::fill(ff.begin(), ff.end(), nullptr);
}

BinaryOp::BinaryOp(char op_, Formula *f1_, Formula *f2_) : op(op_)
{
	ff[0] = f1_;
	ff[1] = f2_;
}

auto BinaryOp::String() const -> std::string
{
	std::string s = "(" + ff[0]->String();
	for (size_t i = 1; i < ff.size(); i++) {
		s += " " + std::string(&op, 1) + " " + ff[i]->String();
	}

	return s + ")";
}

auto BinaryOp::Free() -> void
{
	for (auto f : ff) {
		f->Free();
	}

	delete this;
}

auto BinaryOp::Invert() -> Formula *
{
	op = InvertOp(op);
	for (auto &f : ff) {
		f = f->Invert();
	}
	return dynamic_cast<Formula *>(this);
}

auto BinaryOp::Copy() const -> Formula *
{
	return new BinaryOp(op, ff[0]->Copy(), ff[1]->Copy());
}

Negate::Negate(Formula *f_) : f(f_)
{
}

auto Negate::String() const -> std::string
{
	return "-" + f->String();
}

auto Negate::Free() -> void
{
	if (f != nullptr)
		f->Free();
	delete this;
}

auto Negate::Invert() -> Formula *
{
	auto *res = f;
	f = nullptr;
	Free();
	return dynamic_cast<Formula *>(res);
}

auto Negate::Copy() const -> Formula *
{
	return new Negate(f->Copy());
}

#if 0
auto recursiveAnds(const AIG& aig, BinaryOp *bop, int a, int b) -> Formula *
{
	std::vector<int> v{a,b};
	for(size_t i = 0; i != v.size(); ){
		auto gateOutput = false;

		for(const auto &gate : aig.gates) {
			if(gate.out == v[i]){
				v.erase(v.begin()+i);
				v.push_back(gate.in1);
				v.push_back(gate.in2);
				gateOutput = true;
				break;
			}
		}

		if(!gateOutput) {
			bop->ff.push_back(Formula::FromAIG(aig, v[i]));
			++i;
		}
	}

	return bop;
}
#endif

Latch::Latch(int q_, int nextQ_) : q(q_), nextQ(nextQ_), f(nullptr)
{
}

auto Latch::String() const -> std::string
{
	if (f == nullptr)
		return "F";
	return f->String();
}

auto Latch::Free() -> void
{
	if (f != nullptr)
		delete f;
}

auto Latch::Invert() -> Formula *
{
	return new Negate(this);
}

auto Latch::Copy() const -> Formula *
{
	auto copy = new Latch(q, nextQ);
	if (f != nullptr) {
		copy->f = f->Copy();
	}
	return copy;
}

static auto neg(Formula *f, bool n = true) -> Formula *
{
	if (!n) {
		return f;
	}

	auto t = dynamic_cast<True *>(f);
	if (t != nullptr) {
		return &FalseVal;
	}

	auto fa = dynamic_cast<False *>(f);
	if (fa != nullptr) {
		return &TrueVal;
	}

	return f->Invert();
}

auto Formula::FromAIG(const AIG &aig, int var) -> Formula *
{
	if (var == 0) {
		return &FalseVal;
	} else if (var == 1) {
		return &TrueVal;
	}

	auto negated = var % 2 == 1;
	if (negated)
		var--;

	if (aig.IsInput(var)) {
		return neg(new FormulaVar(var), negated);
	}

	for (const auto &gate : aig.gates) {
		if (gate.out == var) {
			return neg(
			    new BinaryOp(And, FromAIG(aig, gate.in1), FromAIG(aig, gate.in2)),
			    negated);
		}
	}

	for (const auto &latch : aig.latches) {
		if (latch.first == var) {
			return neg(new Latch(latch.first, latch.second), negated);
		}
	}

	char buf[50];
	sprintf(buf, "cannot find %d", var);

	throw std::runtime_error(std::string(buf));
}

auto Formula::Unwind(const AIG &aig, Formula *f) -> void
{
	auto latch = dynamic_cast<Latch *>(f);
	if (latch != nullptr) {
		if (latch->f == nullptr) {
			latch->f = FromAIG(aig, latch->nextQ);
			return;
		}

		Unwind(aig, latch->f);
		return;
	}

	auto gate = dynamic_cast<BinaryOp *>(f);
	if (gate != nullptr) {
		for (auto g : gate->ff) {
			Unwind(aig, g);
		}
		return;
	}

	auto neg = dynamic_cast<Negate *>(f);
	if (neg != nullptr) {
		Unwind(aig, neg->f);
		return;
	}
}

static auto getVar(Formula *f, int &nameCount) -> std::pair<int, bool>
{
	if (f == nullptr) {
		return std::make_pair(nameCount++, false);
	}

	auto fv = dynamic_cast<FormulaVar *>(f);
	if (fv == nullptr) {
		return std::make_pair(nameCount++, false);
	}

	return std::make_pair(fv->var, true);
}

static auto Tseitin2(Formula *f, std::vector<Formula *> &clauses, int name, int &nameCount)
    -> Formula *
{
	auto n = dynamic_cast<Negate *>(f);
	if (n != nullptr) {
		auto next = getVar(n->f, nameCount);

		clauses.push_back(new BinaryOp(Formula::Or, new Negate(new FormulaVar(name)),
		                               new Negate(new FormulaVar(next.first))));
		clauses.push_back(
		    new BinaryOp(Formula::Or, new FormulaVar(name), new FormulaVar(next.first)));

		if (!next.second) {
			Tseitin2(n->f, clauses, next.first, nameCount);
		}

		return nullptr;
	}

	auto b = dynamic_cast<BinaryOp *>(f);
	if (b != nullptr) {
		auto f0 = getVar(b->ff[0], nameCount);
		auto f1 = getVar(b->ff[1], nameCount);

		if (b->op == Formula::And) {
			clauses.push_back(new BinaryOp(Formula::Or,
			                               new Negate(new FormulaVar(f0.first)),
			                               new FormulaVar(name)));
			clauses.push_back(new BinaryOp(Formula::Or,
			                               new Negate(new FormulaVar(f1.first)),
			                               new FormulaVar(name)));
			clauses.push_back(
			    new BinaryOp(Formula::Or, new Negate(new FormulaVar(name)),
			                 new BinaryOp(Formula::Or, new FormulaVar(f0.first),
			                              new FormulaVar(f1.first))));

		} else {
			clauses.push_back(new BinaryOp(Formula::Or,
			                               new Negate(new FormulaVar(name)),
			                               new FormulaVar(f0.first)));
			clauses.push_back(new BinaryOp(Formula::Or,
			                               new Negate(new FormulaVar(name)),
			                               new FormulaVar(f1.first)));
			clauses.push_back(new BinaryOp(
			    Formula::Or, new FormulaVar(name),
			    new BinaryOp(Formula::Or, new Negate(new FormulaVar(f0.first)),
			                            new Negate(new FormulaVar(f1.first)))));
		}

		if (!f0.second) {
			Tseitin2(b->ff[0], clauses, f0.first, nameCount);
		}

		if (!f1.second) {
			Tseitin2(b->ff[1], clauses, f1.first, nameCount);
		}

		return nullptr;
	}

	auto t = dynamic_cast<True *>(f);
	if (t != nullptr) {
		clauses.push_back(new FormulaVar(name));
		return nullptr;
	}

	auto fa = dynamic_cast<False *>(f);
	if (fa != nullptr) {
		clauses.push_back(new Negate(new FormulaVar(name)));
		return nullptr;
	}

	auto v = dynamic_cast<FormulaVar *>(f);
	if (v != nullptr) {
		clauses.push_back(
		    new BinaryOp(Formula::Or, new Negate(new FormulaVar(name)), v->Copy()));
		clauses.push_back(
		    new BinaryOp(Formula::Or, new FormulaVar(name), new Negate(v->Copy())));
		return nullptr;
	}

	throw std::runtime_error("Tseitin2 EOF");
}

auto Formula::TseitinTransform(const Formula *f_, int firstLit) -> std::vector<Formula *>
{
	auto old = RemoveLatches(f_->Copy());

	int name = firstLit;
	auto root = name++;
	std::vector<Formula *> clauses{new FormulaVar(root)};
	Tseitin2(old, clauses, root, name);

	old->Free();

	for(auto& f : clauses){
		f = SimplifyNegations(f);
	}

	return clauses;
}

auto Formula::ToSolver(Solver &s, const std::vector<Formula *> &ff) -> void
{
	vec<Lit> clause;

	std::map<int, int> varmap;
	auto translateVar = [&](int var) -> int {
		auto it = varmap.find(var);
		if (it == varmap.end()) {
			int t = s.newVar();
			varmap[var] = t;
			return t;
		}

		return it->second;
	};

	for (auto it = ff.begin(); it != ff.end(); ++it) {
		auto f = *it;
		clause.clear();

		std::vector<Formula *> todo{f};

		for (size_t i = 0; i != todo.size(); ++i) {
			auto f = todo[i];

			auto b = dynamic_cast<const BinaryOp *>(f);
			if (b != nullptr) {
				todo.push_back(b->ff[0]);
				todo.push_back(b->ff[1]);
				continue;
			}

			auto n = dynamic_cast<const Negate *>(f);
			if (n != nullptr) {
				auto v = dynamic_cast<const FormulaVar *>(n->f);
				if (v == nullptr) {
					throw std::runtime_error("complex negation");
				}

				clause.push(Lit(translateVar(v->var), true));
				continue;
			}

			auto v = dynamic_cast<const FormulaVar *>(f);
			if (v != nullptr) {
				clause.push(Lit(translateVar(v->var), false));
			}
		}

		s.addClause(clause);
	}
}

auto Formula::RemoveLatches(Formula *f) -> Formula *
{
	auto l = dynamic_cast<Latch *>(f);
	if (l != nullptr) {
		auto inner = l->f;
		l->f = nullptr;
		l->Free();

		if (inner == nullptr) {
			return &FalseVal;
		}

		return RemoveLatches(inner);
	}

	auto n = dynamic_cast<Negate *>(f);
	if (n != nullptr) {
		n->f = RemoveLatches(n->f);
		return n;
	}

	auto b = dynamic_cast<BinaryOp *>(f);
	if (b != nullptr) {
		for (auto &f_ : b->ff) {
			f_ = RemoveLatches(f_);
		}

		return b;
	}

	return f;
}

auto Formula::Concat(Formula *f1, Formula *f2, char op) -> Formula *
{
	return new BinaryOp(op, f1, f2);
}

auto Formula::SimplifyNegations(Formula *f) -> Formula*
{
	auto n = dynamic_cast<Negate *>(f);
	if (n != nullptr) {		
		auto innerN = dynamic_cast<Negate*>(n->f);
		if(innerN != nullptr){
			auto res = innerN->f;
			innerN->f = nullptr;
			n->Free();

			return SimplifyNegations(res);
		}

		auto innerT = dynamic_cast<True*>(n->f);
		if(innerT != nullptr){
			n->Free();
			return &FalseVal;
		}

		auto innerF = dynamic_cast<False*>(n->f);
		if(innerF != nullptr){
			n->Free();
			return &TrueVal;
		}
		
		n->f = SimplifyNegations(n->f);
	}

	auto b = dynamic_cast<BinaryOp *>(f);
	if (b != nullptr) {
		b->ff[0] = SimplifyNegations(b->ff[0]);
		b->ff[1] = SimplifyNegations(b->ff[1]);
	}

	auto l = dynamic_cast<Latch*>(f);
	if(l != nullptr && l->f != nullptr) {
		l->f = SimplifyNegations(l->f);
	}

	return f;
}


True TrueVal;
False FalseVal;
