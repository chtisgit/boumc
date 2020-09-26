#include <algorithm>
#include <fstream>
#include <iostream>

#include <cstring>
#include <getopt.h>

#include "aag.h"
#include "translate.h"

struct Env {
	int K = -1;
	int Debug = 0;
	std::istream *inputstream = &std::cin;
	std::ifstream file;
	bool ParserTest = false;
	bool ShowProof = false;
	bool PrintDIMACS = false;
};

const char USAGE[] =
    " -k <steps> [-d <level>] [-f <file>] [--parse-only]\n\n"
    "-d[level]             include debug output (optionally, specify debug level)\n"
    "-k <steps>             # of steps the model checker will unwind (default k=0)\n"
    "-f <file path>         read from a file instead of console\n"
    "-p | --proof           show proof\n"
    "--parse-only           Only parse ASCII AIGer file (for testing)\n";

auto usage(const char *prog) -> void
{
	std::cout << "Usage: " << prog << USAGE << std::endl;
}

auto parseArgs(int argc, char **argv) -> Env
{
	static option long_options[] = {{"parse-only", no_argument, 0, 0},
					{"help", no_argument, 0, 0},
					{"dimacs", no_argument, 0, 0},
					{"proof", no_argument, 0, 'p'},
					{0, 0, 0, 0}};
	Env e;
	while (1) {
		int option_index = 0, c;

		c = getopt_long(argc, argv, "d::f:k:p", long_options, &option_index);
		if (c == -1)
			break;

		switch (c + (c == 0 ? option_index : 0)) {
		case 0: // --parse-only
			e.ParserTest = true;
			break;

		case 1: // --help
			usage(argv[0]);
			exit(0);
			break;
		
		case 2: // --dimacs
			e.PrintDIMACS = true;
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

		case 'p':
			e.ShowProof = true;
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
		for (int i = 0; i < clauses.size(); i++) {
			out << "Clause " << (i + 1) << ": ";
			for (int j = 0; j < clauses[i].size(); j++) {
				out << (sign(clauses[i][j]) ? "-" : "") << var(clauses[i][j])
				    << ' ';
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
		auto end = std::unique(data, data + size);
		v.~vec<T>();
		new (&v) vec<T>(data, end - data);
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

	Solver s;

	if(env.PrintDIMACS) {
		DimacsCNFer cnfer(std::cout);
		AIGtoSATer ats(aig, cnfer, env.K);
		ats.toSAT();
		return 0;
	}

	Trav trav;
	// proof needs to be assigned before newVar() is called on the Solver...
	s.proof = new Proof(trav);

	try {
		SolverCNFer cnfer(s);
		AIGtoSATer ats(aig, cnfer, env.K);
		ats.toSAT();
	}catch(TranslationError& err) {
		std::cout << "translation error: " << err.what() << std::endl << std::endl;
		return 1;
	}catch(std::exception& err) {
		std::cout << "error: " << err.what() << std::endl << std::endl;
		return 1;
	}

	std::cout << "Running SAT solver..." << std::endl;
	s.solve();

	// s.okay()  == true =>  SAT
	// s.okay() == false => UNSAT
	// SAT => E  a path to a Bad State.
	std::cout << std::endl << (s.okay() ? "FAIL" : "OK") << std::endl;

	if (env.ShowProof)
		trav.print(std::cout);

	return 0;
}
