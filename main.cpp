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
		int this_option_optind = optind ? optind : 1;
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

	if(env.ParserTest){
		return 0;
	}

	std::cout << "outputs " << aig.outputs.size() << std::endl;

	for(const auto output : aig.outputs) {
		auto f = Formula::FromAIG(aig, output);
		for(int i = 0; i != 4; i++){
			std::cout << "(k=" << i << ") " << output << " = " << f->String() << std::endl;
			Formula::Unwind(aig, f);
		}
		f->Free();
	}

	return 0;
}