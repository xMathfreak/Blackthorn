#include "engine/include/Core/Engine.h"

int main(int argc, char const *argv[]) {
	Blackthorn::Engine engine;
	engine.init();
	engine.run();
	return 0;
}