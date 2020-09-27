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
	bool Interpolation = false;
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
					{"interpolate", no_argument, 0, 'i'},
					{0, 0, 0, 0}};
	Env e;
	while (1) {
		int option_index = 0, c;

		c = getopt_long(argc, argv, "d::f:ik:p", long_options, &option_index);
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

		case 'i':
			e.Interpolation = true;
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

const char NodeRoot[] = "root";
const char NodeChain[] = "chain";

struct Node {
	const char *type;
	vec<Lit> lit;
};

// A "listner" for the proof. Proof events will be passed onto (online mode) or replayed to
// (offline mode) this class.  Each call to 'root()' or 'chain()' produces a new clause. The first
// clause has ID 0, the next 1 and so on. These are the IDs passed to 'chain()'s 'cs' parameter.
//
struct Trav : public ProofTraverser {
	vec<Node> clauses;

	void print(std::ostream &out)
	{
		out << "Proof: " << std::endl;
		for (int i = 0; i < clauses.size(); i++) {
			out << "Clause " << (i + 1) << ": ";
			for (int j = 0; j < clauses[i].lit.size(); j++) {
				out << (sign(clauses[i].lit[j]) ? "-" : "") << var(clauses[i].lit[j])
				    << ' ';
			}
			out << std::endl;
		}
	}

	template <class T, class LessThan>
	static void sortUnique(vec<T>& v, LessThan lt)
	{
		int     size = v.size();
		T*      data = v.release();
		std::sort(data, data+size, lt);
		std::unique(data, data+size);
		v.~vec<T>();
		new (&v) vec<T>(data, size);
	}

	template <class T>
	static void sortUnique(vec<T>& v)
	{
		sortUnique(v, std::less<T>());
	}


	auto newClause(const char *type) -> Node&
	{
		clauses.push();
		auto& n = clauses.last();
		n.type = type;

		return n;
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
		c.copyTo(newClause(NodeRoot).lit);
	}

	void chain(const vec<ClauseId> &cs, const vec<Var> &xs)
	{
		clauses.push();
		auto& n = newClause(NodeChain);
		clauses[cs[0]].lit.copyTo(n.lit);
        for (int i = 0; i < xs.size(); i++)
            resolve(n.lit, clauses[cs[i+1]].lit, xs[i]);
	}

	void deleted(ClauseId c)
	{
		clauses[c].lit.clear();
	}

	virtual void done()
	{
		std::cout << "done()" << std::endl;
	}

	virtual ~Trav()
	{
	}
};

auto createSolverWithTraverser(ProofTraverser *trav) -> std::unique_ptr<CNFer>
{
	auto s = std::make_unique<SolverCNFer>();
	s->withProofTraverser(trav);
	return s;
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

	if(env.PrintDIMACS) {
		DimacsCNFer cnfer(std::cout);
		AIGtoSATer ats(aig, nullptr);
		VarTranslator vars(cnfer);
		ats.toSAT(cnfer, vars, env.K);
		return 0;
	}

	try {
		AIGtoSATer ats(aig, createSolverWithTraverser);

		if(env.Interpolation)
			ats.enableInterpolation();
		
		// true =>  SAT
		// false => UNSAT
		// SAT => E  a path to a Bad State.
		bool badstate = ats.check(env.K);

		std::cout << std::endl << (badstate ? "FAIL" : "OK") << std::endl;

	}catch(TranslationError& err) {
		std::cout << "translation error: " << err.what() << std::endl << std::endl;
		return 1;
	}catch(std::exception& err) {
		std::cout << "error: " << err.what() << std::endl << std::endl;
		return 1;
	}

	return 0;
}
