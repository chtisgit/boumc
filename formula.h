#pragma once
#include <array>
#include <cassert>

#include "MiniSat-p_v1.14/Solver.h"
#include "aag.h"

struct Formula {
	virtual ~Formula();
	virtual auto String() const -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula *;
	virtual auto Copy() const -> Formula *;

	static auto FromAIG(const AIG &, int var) -> Formula *;
	static auto Unwind(const AIG &, Formula *f) -> void;
	static auto TseitinTransform(const Formula *f, int firstLit) -> std::vector<Formula *>;
	static auto RemoveLatches(Formula *f) -> Formula *;
	static auto Concat(Formula*, Formula*, char op = '&') -> Formula *;
	static auto SimplifyNegations(Formula *f) -> Formula*;

	static auto ToSolver(Solver &s, const std::vector<Formula *> &ff_) -> void;

	static constexpr auto InvertOp(char op) -> char {
		return (op == And || op == Or) ? (op == And ? Or : And) : throw std::domain_error("operator is neither And nor Or");
	}
	static constexpr char And = '&', Or = '|';
};

struct True : Formula {
	virtual auto String() const -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula *;
	virtual auto Copy() const -> Formula *;
};

struct False : Formula {
	virtual auto String() const -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula *;
	virtual auto Copy() const -> Formula *;
};

extern True TrueVal;
extern False FalseVal;

struct FormulaVar : Formula {
	int var;

	explicit FormulaVar(int var);
	virtual auto String() const -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula *;
	virtual auto Copy() const -> Formula *;
};

struct BinaryOp : Formula {
	std::array<Formula *, 2> ff;
	char op;

	explicit BinaryOp(char op);
	BinaryOp(char op, Formula *, Formula *);
	virtual auto String() const -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula *;
	virtual auto Copy() const -> Formula *;
};

struct Negate : Formula {
	Formula *f;

	explicit Negate(Formula *);
	virtual auto String() const -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula *;
	virtual auto Copy() const -> Formula *;
};

struct Latch : Formula {
	int q, nextQ;
	Formula *f;

	Latch(int q, int nextQ);
	virtual auto String() const -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula *;
	virtual auto Copy() const -> Formula *;
};
