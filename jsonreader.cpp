#include "jsonreader.h"
#include <cxxabi.h>
#include <memory>
#include <string>

using namespace rapidjson;
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

QByteArray JSONReader::subJsonRender(rapidjson::Value* el) {
	// https://github.com/Tencent/rapidjson/issues/1035

	StringBuffer               sb;
	PrettyWriter<StringBuffer> writer(sb);
	el->Accept(writer);
	return sb.GetString();

	// OLD
	//		auto&               allocator = json.GetAllocator();
	//		rapidjson::Document d;
	//		rapidjson::Value    v2(*el, allocator);

	//		rapidjson::StringBuffer                          buffer;
	//		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	//		d.CopyFrom(*el, allocator);
	//		d.Accept(writer);
	//		QByteArray res = "\n";
	//		res.append(buffer.GetString(), buffer.GetSize());
	//		res.append("\n");
	//		return res;
}
