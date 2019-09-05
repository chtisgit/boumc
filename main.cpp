#include <algorithm>
#include <iostream>

#include <getopt.h>

#include "aag.h"
#include "formula.h"

struct Env {
	int K = -1;
	bool Debug = false;
	bool ParserTest = false;
};

const char USAGE[] =
    " -k <steps> [-d] [--parse-only]\n\n"
    "-d                     include debug output\n"
    "-k <steps>             # of steps the model checker will unwind (default k=0)\n"
    "--parse-only           Only parse ASCII AIGer file (for testing)\n";

auto usage(const char *prog) -> void
{
	std::cout << "Usage: " << prog << USAGE << std::endl;
}

auto parseArgs(int argc, char **argv) -> Env
{
	static option long_options[] = {
	    {"parse-only", no_argument, 0, 0}, {"help", no_argument, 0, 0}, {0, 0, 0, 0}};
	Env e;
	while (1) {
		int option_index = 0, c;

		c = getopt_long(argc, argv, "dk:", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 0:
			if (option_index == 0) {
				e.ParserTest = true;
			} else if (option_index == 1) {
				usage(argv[0]);
				exit(0);
			}
			break;

		case 'd':
			e.Debug = true;
			break;

		case 'k':
			e.K = atoi(optarg);
			break;
		}
	}

	if (e.K < 0 && !e.ParserTest) {
		usage(argv[0]);
		std::cout << "Parameter k was not given" << std::endl;
		exit(0);
	}

	return e;
}

// A "listner" for the proof. Proof events will be passed onto (online mode) or replayed to
// (offline mode) this class.  Each call to 'root()' or 'chain()' produces a new clause. The first
// clause has ID 0, the next 1 and so on. These are the IDs passed to 'chain()'s 'cs' parameter.
//
struct Trav : public ProofTraverser {
	vec<vec<Lit>> clauses;

	void print()
	{
		for (size_t i = 0; i < clauses.size(); i++) {
			std::cout << "Clause " << (i + 1) << ": ";
			for (size_t j = 0; j < clauses[i].size(); j++) {
				std::cout << var(clauses[i][j]) << ' ';
			}
			std::cout << std::endl;
		}
	}

	template <class T, class LessThan>
	static void sortUnique(vec<T> &v, LessThan lt)
	{
		int size = v.size();
		T *data = v.release();
		std::sort(data, data + size, lt);
		std::unique(data, data + size);
		v.~vec<T>();
		new (&v) vec<T>(data, size);
	}

	template <class T>
	static void sortUnique(vec<T> &v)
	{
		sortUnique(v, std::less<T>());
	}

	static void resolve(vec<Lit> &main, vec<Lit> &other, Var x)
	{
		Lit p;
		bool ok1 = false, ok2 = false;
		for (int i = 0; i < main.size(); i++) {
			if (var(main[i]) == x) {
				ok1 = true, p = main[i];
				main[i] = main.last();
				main.pop();
				break;
			}
		}

		for (int i = 0; i < other.size(); i++) {
			if (var(other[i]) != x)
				main.push(other[i]);
			else {
				if (p != ~other[i])
					printf("PROOF ERROR! Resolved on variable with SAME "
					       "polarity in both clauses: %d\n",
					       x + 1);
				ok2 = true;
			}
		}

		if (!ok1 || !ok2)
			printf("PROOF ERROR! Resolved on missing variable: %d\n", x + 1);

		sortUnique(main);
	}

	void root(const vec<Lit> &c)
	{
		clauses.push();
		c.copyTo(clauses.last());
	}

	void chain(const vec<ClauseId> &cs, const vec<Var> &xs)
	{
		clauses.push();
		vec<Lit> &c = clauses.last();
		clauses[cs[0]].copyTo(c);
		for (int i = 0; i < xs.size(); i++)
			resolve(c, clauses[cs[i + 1]], xs[i]);
	}

	void deleted(ClauseId c)
	{
		clauses[c].clear();
	}

	virtual void done()
	{
		std::cout << "done()" << std::endl;
	}

	virtual ~Trav()
	{
	}
};

auto main(int argc, char **argv) -> int
{
	auto env = parseArgs(argc, argv);
	auto aig = AIG::FromStream(std::cin);

	if (env.ParserTest) {
		return 0;
	}

	std::cout << "outputs " << aig.outputs.size() << std::endl;
	std::cout << "K = " << env.K << std::endl;
	std::cout << std::endl;

	// the outputs of the circuit represent the properties that are to be checked.
	// create a (-P1 | -P2 | ...) clause
	assert(aig.outputs.size() != 0);
	auto f = Formula::FromAIG(aig, aig.outputs[0])->Invert();
	for (size_t i = 1; i != aig.outputs.size(); i++) {
		const auto p = Formula::FromAIG(aig, aig.outputs[i]);
		f = Formula::Concat(f, p->Invert(), '|');
	}

	for (int k = 0; k <= env.K; k++) {
		f = Formula::SimplifyNegations(f);
		if (env.Debug) {
			std::cout << "(k=" << k << ") = " << f->String() << std::endl;
		}else{
			std::cout << "\rk = " << k << std::flush;
		}

		auto cnf = Formula::TseitinTransform(f, aig.lastLit + 2);

		if (env.Debug) {
			std::cout << "CNF = ";
			for (auto f : cnf) {
				std::cout << f->String() << " & ";
			}
			std::cout << std::endl;
		}

		Solver solver;
		Formula::ToSolver(solver, cnf);

		for (auto clause : cnf) {
			clause->Free();
		}

		if (solver.solve()) {
			std::cout << "FAIL (k=" << k << ")" << std::endl << std::endl;
			// std::cout << "proof = " << solver.proof << std::endl;
			return 0;
		}

		Formula::Unwind(aig, f);
	}
	std::cout << std::endl << std::endl;

	f->Free();

	std::cout << "OK" << std::endl;

	return 0;
}