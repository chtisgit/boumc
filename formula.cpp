#include "formula.h"

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

auto False::String() const -> std::string
{
	return "F";
}

auto False::Free() -> void
{
}

auto False::Invert() -> Formula *
{
	return dynamic_cast<Formula *>(&TrueVal);
}

Var::Var(int var_) : var(var_)
{
}

auto Var::String() const -> std::string
{
	char buf[20];
	sprintf(buf, "%d", var);
	return buf;
}

auto Var::Free() -> void
{
	delete this;
}

auto Var::Invert() -> Formula *
{
	return dynamic_cast<Formula *>(new Negate(this));
}

BinaryOp::BinaryOp(char op_) : op(op_)
{
}

BinaryOp::BinaryOp(char op_, Formula *f1_, Formula *f2_) : ff(2), op(op_)
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
	op = op == '&' ? '|' : '&';
	for (auto &f : ff) {
		f = f->Invert();
	}
	return dynamic_cast<Formula *>(this);
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

Latch::Latch(int q_, int nextQ_) : q(q_), nextQ(nextQ_), f(nullptr) {}

auto Latch::String() const -> std::string
{
	if(f == nullptr)
		return "F";
	return f->String();
}

auto Latch::Free() -> void
{
	if(f != nullptr)
		delete f;
}

auto Latch::Invert() -> Formula *
{
	return new Negate(this);
}

auto Formula::FromAIG(const AIG &aig, int var) -> Formula *
{
	if (var == 0) {
		return &TrueVal;
	} else if (var == 1) {
		return &FalseVal;
	}

	auto negated = var % 2 == 1;
	if (negated)
		var--;
	auto negater = [negated](Formula *f) -> Formula * {
		if (!negated)
			return f;

		return new Negate(f);
	};

	if (aig.IsInput(var)) {
		return negater(new Var(var / 2));
	}

	for (const auto &gate : aig.gates) {
		if (gate.out == var) {
			auto g =
			    new BinaryOp('&');
			return negater(recursiveAnds(aig, g, gate.in1, gate.in2));
		}
	}

	for (const auto &latch: aig.latches) {
		if (latch.first == var) {
			return negater(new Latch(latch.first, latch.second));
		}
	}

	char buf[50];
	sprintf(buf, "cannot find %d", var);

	throw std::runtime_error(std::string(buf));
}

auto Formula::Unwind(const AIG &aig, Formula *f) -> void
{
	auto latch = dynamic_cast<Latch*>(f);
	if(latch != nullptr) {
		if(latch->f == nullptr){
			latch->f = FromAIG(aig, latch->nextQ);
			return;
		}

		Unwind(aig, latch->f);
		return;
	}

	auto gate = dynamic_cast<BinaryOp*>(f);
	if(gate != nullptr) {
		for(auto g : gate->ff) {
			Unwind(aig, g);
		}
		return;
	}

	auto neg = dynamic_cast<Negate*>(f);
	if(neg != nullptr) {
		Unwind(aig, neg->f);
		return;
	}
}

True TrueVal;
False FalseVal;