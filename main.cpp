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
    "-d[level]              include debug output (optionally, specify debug level)\n"
	"--dimacs               print CNF of classical BMC check in DIMACS format\n"
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

	try {
		if(env.PrintDIMACS) {
			if (env.Interpolation) {
				std::cout << "Cannot print DIMACS for interpolation method, sry." << std::endl;
				return 0;
			}

			DimacsCNFer cnfer(std::cout);
			AIGtoSATer ats{aig};
			VarTranslator vars(&cnfer, aig.lastLit, env.K);
			ats.toSAT(cnfer, vars, env.K);
			return 0;
		}

		AIGtoSATer ats{aig};

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
