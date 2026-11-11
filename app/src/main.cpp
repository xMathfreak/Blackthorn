#include <Blackthorn/Blackthorn.hpp>

int main(int argc, char const *argv[]) {
	Blackthorn::Engine engine;

	if (!engine.init())
		return -1;

	engine.run();

	return 0;
}