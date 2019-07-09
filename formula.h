#pragma once
#include <vector>

#include "aag.h"

struct Formula {
	virtual ~Formula();
	virtual auto String() const -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula*;

	static auto FromAIG(const AIG &, int var) -> Formula *;
	static auto Unwind(const AIG &, Formula *f) -> void;
};

struct True : Formula {
	virtual auto String() const -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula*;
};

struct False : Formula {
	virtual auto String() const -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula*;
};

extern True TrueVal;
extern False FalseVal;

struct Var : Formula {
	int var;

	explicit Var(int var);
	virtual auto String() const -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula*;
};

struct BinaryOp : Formula {
	std::vector<Formula *> ff;
	char op;

	explicit BinaryOp(char op);
	BinaryOp(char op, Formula *, Formula *);
	virtual auto String() const -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula*;
};

struct Negate : Formula {
	Formula *f;

	explicit Negate(Formula*);
	virtual auto String() const -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula*;
};

struct Latch : Formula {
	int q, nextQ;
	Formula *f;

	Latch(int q, int nextQ);
	virtual auto String() const -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula*;
};
