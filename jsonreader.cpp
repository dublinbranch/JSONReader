#include "jsonreader.h"
#include <cxxabi.h>
#include <memory>
#include <string>

QString demangle(const char* name) {
	//Just copy pasted from stack overflow, I have no exact idea how this works
	int status = -4; // some arbitrary value to eliminate the compiler warning

	QString res = abi::__cxa_demangle(name, NULL, NULL, &status);

	if (status == 0) {
		return res;
	} else {
		//something did not worked, sorry
		return name;
	}
}
