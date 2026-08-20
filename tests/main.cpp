#include "Tests.h"

int main(int argc, char** argv) {
	// argc/argv lets --shuffle / --seed=N / --repeat=N / --timeout=MS be
	// passed on the command line; e.g.: ./example --shuffle --repeat=2
	return BT_RUN_ALL(argc, argv);
}