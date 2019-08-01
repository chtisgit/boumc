#include <iostream>

#include <getopt.h>

#include "aag.h"
#include "formula.h"

struct Env {
	int K = 0;
	bool ParserTest = false;
};

static option long_options[] = {{"parse-only", no_argument, 0, 0}, {0, 0, 0, 0}};
auto parseArgs(int argc, char **argv) -> Env
{
	Env e;
	while (1) {
		int option_index = 0, c;

		c = getopt_long(argc, argv, "k:", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 0:
			if (option_index == 0) {
				e.ParserTest = true;
			}
			break;
		case 'k':
			e.K = atoi(optarg);
			break;
		}
	}

	return e;
}

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

	for (const auto output : aig.outputs) {
		auto f = Formula::FromAIG(aig, output);
		for (int i = 0; i <= env.K; i++) {
			std::cout << "(k=" << i << ") " << output << " = " << f->String()
				  << std::endl;

			auto cnf = Formula::TseitinTransform(f, aig.lastLit+2);
			Solver solver;
			Formula::ToSolver(solver, cnf);

			for (auto clause : cnf) {
				clause->Free();
			}

			if(solver.solve()){
				std::cout << "FAIL (k=" << env.K << ")" << std::endl << std::endl;
				std::cout << "proof = " << solver.proof << std::endl;
				return 0;
			}


			Formula::Unwind(aig, f);
		}
		f->Free();
	}

	std::cout << "OK" << std::endl;

	return 0;
}