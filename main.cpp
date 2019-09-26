#include <algorithm>
#include <fstream>
#include <iostream>

#include <cstring>
#include <getopt.h>

#include "aag.h"
#include "formula.h"

struct Env {
	int K = -1;
	int Debug = 0;
	bool ParserTest = false;
	std::istream *inputstream = &std::cin;
	std::ifstream file;
};

const char USAGE[] =
    " -k <steps> [-d <level>] [-f <file>] [--parse-only]\n\n"
    "-d[level]             include debug output (optionally, specify debug level)\n"
    "-k <steps>             # of steps the model checker will unwind (default k=0)\n"
    "-f <file path>         read from a file instead of console\n"
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

		c = getopt_long(argc, argv, "d::f:k:", long_options, &option_index);
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
			if (optarg == nullptr) {
				e.Debug = 1;
				break;
			}

			e.Debug = atoi(optarg);
			if (e.Debug <= 0) {
				e.Debug = 1;
			}
			break;

		case 'k':
			e.K = atoi(optarg);
			break;

		case 'f':
			if (strcmp(optarg, "-") == 0) {
				break;
			}

			e.file.open(optarg);
			if (e.file.fail()) {
				std::cout << "error: cannot open file '" << optarg << "'"
				          << std::endl;
				exit(1);
			}

			e.inputstream = &e.file;

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

	void print(std::ostream &out)
	{
		out << "Proof: " << std::endl;
		for (size_t i = 0; i < clauses.size(); i++) {
			out << "Clause " << (i + 1) << ": ";
			for (size_t j = 0; j < clauses[i].size(); j++) {
				out << var(clauses[i][j]) << ' ';
			}
			out << std::endl;
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
			this->resolve(c, clauses[cs[i + 1]], xs[i]);
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

auto boumc_simple(const Env &env, const AIG &aig) -> int
{
	// the outputs of the circuit represent the properties that are to be checked.
	// create a (P1 | P2 | ...) clause
	auto f = Formula::FromAIG(aig, aig.outputs[0])->Invert();
	for (size_t i = 1; i != aig.outputs.size(); i++) {
		const auto p = Formula::FromAIG(aig, aig.outputs[i])->Invert();
		f = Formula::Concat(f, p, '&');
	}

	for (int k = 1; k <= env.K; k++) {
		Formula::Unwind(aig, f);
	}
	f = Formula::SimplifyNegations(Formula::RemoveLatches(f));

	if (env.Debug) {
		std::cout << "Formula: " << f->String() << std::endl;
	}

	auto cnf = Formula::TseitinTransform(f, aig.lastLit + 2);
	f->Free();

	if (env.Debug >= 3) {
		std::cout << "Individual CNF Clauses:" << std::endl;
		int c = 1;
		for (auto f : cnf) {
			std::cout << "[" << (c++) << "] " << f->String() << std::endl;
		}
		std::cout << std::endl;
	}

	Trav trav;

	Solver s;
	// proof needs to be assigned before newVar() is called on the Solver...
	s.proof = new Proof(trav);

	const auto varmap = Formula::ToSolver(s, cnf);
	if (env.Debug >= 3) {
		std::cout << "Translation map:" << std::endl;
		for (const auto &entry : varmap) {
			std::cout << "  " << entry.first << " -> " << entry.second << std::endl;
		}
	}

	for (auto clause : cnf) {
		clause->Free();
	}

	if (env.Debug)
		std::cout << "Running SAT solver..." << std::endl;
	s.solve();

	// s.okay()  == true =>  SAT
	// s.okay() == false => UNSAT
	// SAT => E  a path to a Bad State.
	std::cout << std::endl << (s.okay() ? "FAIL" : "OK") << std::endl;

	if (env.Debug)
		trav.print(std::cout);

	return 0;
}

auto main(int argc, char **argv) -> int
{
	auto env = parseArgs(argc, argv);
	auto aig = AIG::FromStream(*env.inputstream);

	if (env.ParserTest) {
		return 0;
	}

	if (aig.outputs.size() == 0) {
		std::cout << "Model has no outputs. Bad states cannot be detected." << std::endl;
		std::cout << "Exiting." << std::endl;
		return 0;
	}

	std::cout << "outputs " << aig.outputs.size() << std::endl;
	std::cout << "K = " << env.K << std::endl;

	return boumc_simple(env, aig);
}
