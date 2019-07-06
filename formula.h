#pragma once
#include <vector>

#include "aag.h"

struct Formula {
	virtual ~Formula();
	virtual auto String() -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula*;

	static auto FromAIG(const AIG &, int var) -> Formula *;
};

struct True : Formula {
	virtual auto String() -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula*;
};

struct False : Formula {
	virtual auto String() -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula*;
};

extern True TrueVal;
extern False FalseVal;

struct Var : Formula {
	int var;

	explicit Var(int var);
	virtual auto String() -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula*;
};

struct BinaryOp : Formula {
	std::vector<Formula *> ff;
	char op;

	BinaryOp(char op, Formula *, Formula *);
	virtual auto String() -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula*;
};

struct Negate : Formula {
	Formula *f;

	explicit Negate(Formula*);
	virtual auto String() -> std::string;
	virtual auto Free() -> void;
	virtual auto Invert() -> Formula*;
};