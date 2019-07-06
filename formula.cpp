#include "formula.h"

Formula::~Formula()
{
}

auto Formula::String() -> std::string
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

auto True::String() -> std::string
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

auto False::String() -> std::string
{
	return "F";
}

auto False::Free() -> void
{
}

auto False::Invert() -> Formula *
{
	return dynamic_cast<Formula*>(&TrueVal);
}

Var::Var(int var_) : var(var_)
{
}

auto Var::String() -> std::string
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
	return dynamic_cast<Formula*>(new Negate(this));
}

BinaryOp::BinaryOp(char op_, Formula *f1_, Formula *f2_) : ff(2), op(op_)
{
	ff[0] = f1_;
	ff[1] = f2_;
}

auto BinaryOp::String() -> std::string
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
	return dynamic_cast<Formula*>(this);
}

Negate::Negate(Formula *f_) : f(f_)
{
}

auto Negate::String() -> std::string
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
	return dynamic_cast<Formula*>(res);
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

	for (const auto input : aig.inputs) {
		if (input == var) {
			return negater(new Var(var / 2));
		}
	}

	for (const auto &gate : aig.gates) {
		if (gate.out == var) {
			return negater(
			    new BinaryOp('&', FromAIG(aig, gate.in1), FromAIG(aig, gate.in2)));
		}
	}

	char buf[50];
	sprintf(buf, "cannot find %d", var);

	throw std::runtime_error(std::string(buf));
}

True TrueVal;
False FalseVal;