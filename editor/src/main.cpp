#include "Editor.h"

int main(int argc, char const *argv[]) {
	Blackthorn::Editor::Application app;

	if (!app.init()) {
		return -1;
	}

	app.run();

	return 0;
}