#pragma once
//must be the first
#include "rapidjson/includeMePretty.h"
//******************
#include "JSONReaderConst.h"
#include "QStacker/qstacker.h"
#include "define.h"
#include "rapidFunkz/various.h"
#include "rapidjson/cursorstreamwrapper.h"

#include "rapidjson/pointer.h"
#include <QDebug>
#include <QString>

QString demangle(const char* name);

template <typename Type>
QString getTypeName() {
	auto name = typeid(Type).name();
	return demangle(name);
}

class JSONReader {
      public:
	rapidjson::Document json;
	//No idea how to have this value flow around
	rapidjson::Type mismatchedType;

	template <typename Type>
	/**
	 * @brief swapperJPtr 
	 * @param key
	 * @param value
	 */
	void swapperJPtr(const char* JSONPointer, Type& value, rapidjson::Value* src = nullptr) {
		rapidjson::Value* refJson = src;

		if (src) {
			//apply json ptr to a subblock!
			refJson = src;
		} else {
			refJson = &json;
		}

		rapidjson::Value* useMe = rapidjson::Pointer(JSONPointer).Get(*refJson);
		switch (swapper(useMe, value)) {
		case SwapRes::errorMissing:
			qCritical().noquote() << "missing JSON Path:" << JSONPointer << "for REQUIRED parameter" + QStacker16(4, QStackerOptLight);
			exit(1);
			break;
		case SwapRes::swapped:
			rapidjson::Pointer(JSONPointer).Erase(*refJson);
			break;
		case SwapRes::typeMismatch:
			qCritical().noquote() << QSL("Type mismatch! Expecting %1 found %2 for %3")
			                                 .arg(getTypeName<Type>())
			                                 .arg(printType(mismatchedType))
			                                 .arg(JSONPointer) +
			                             QStacker16Light();
			exit(1);
			break;
		case SwapRes::notFound:
			//nothing to do here
			break;
		}
	}

	template <typename Type>
	void swapper(rapidjson::Value::MemberIterator& iter, const char* key, Type& value) {
		return swapper(&iter->value, key, value);
	}

	template <typename Type>
	void swapper(rapidjson::Value& obj, const char* key, Type& value) {
		swapper(&obj, key, value);
	}

	template <typename Type>
	void swapper(rapidjson::Value* obj, const char* key, Type& value) {
		if (!obj) {
			qCritical().noquote() << "You need to check for branch existance before this point (we extract the leaf!)" << QStacker16Light();
			exit(1);
		}

		auto              iter  = obj->FindMember(key);
		rapidjson::Value* useMe = nullptr;
		if (iter != obj->MemberEnd()) {
			useMe = &iter->value;
		}

		switch (swapper(useMe, value)) {
		case SwapRes::errorMissing:
			qCritical().noquote() << "missing KEY:" << key << "for REQUIRED parameter" + QStacker16Light();
			exit(1);
			break;
		case SwapRes::swapped:
			obj->RemoveMember(iter);
			break;
		case SwapRes::typeMismatch:
			qCritical().noquote() << QSL("Type mismatch! Expecting %1 found %2 for %3")
			                                 .arg(getTypeName<Type>())
			                                 .arg(printType(mismatchedType))
			                                 .arg(key) +
			                             QStacker16Light();
			exit(1);
			break;
		case SwapRes::notFound:
			//nothing to do here
			break;
		}
	}

	enum SwapRes {
		typeMismatch,
		errorMissing, //the value was needed, but not founded
		swapped,      //all ok
		notFound      // missing but not required
	};

	template <typename Type>
	SwapRes swapper(rapidjson::Value* obj, Type& value) {
		if (obj == nullptr) {
			//check if the default value is a sensible one
			//empty is a legit value, that is why we use a canary one SET_ME
			if constexpr (std::is_same<Type, QByteArray>::value) {
				if (value == JSONReaderConst::setMe8) {
					return SwapRes::errorMissing;
				}
			}
			if constexpr (std::is_same<Type, QString>::value) {
				if (value == JSONReaderConst::setMe) {
					return SwapRes::errorMissing;
				}
			}
			//higher precedence because, is_integral bool == true!
			if constexpr (std::is_same<Type, bool>::value) {
				//no reason ATM to enforce required default also for bool
				return SwapRes::notFound;
			}
			if constexpr (std::is_integral<Type>::value) {
				if (value == JSONReaderConst::setMeInt) {
					return SwapRes::errorMissing;
				}
			}
			if constexpr (std::is_floating_point<std::remove_reference<Type>>::value) {
				if (qIsNaN(value)) {
					return SwapRes::errorMissing;
				}
			}
			return SwapRes::notFound;
		}

		//higher precedence because, is_integral bool == true!
		if constexpr (std::is_same<Type, QByteArray>::value) {
			value.clear();
			value.append(obj->GetString());
			return SwapRes::swapped;
		} else if constexpr (std::is_same<Type, QString>::value) {
			value.clear();
			value.append(obj->GetString());
			return SwapRes::swapped;
		} else { //This should handle all the other
			if (!obj->template Is<Type>()) {
				mismatchedType = obj->GetType();
				return SwapRes::typeMismatch;
			}
			value = obj->template Get<Type>();
			return SwapRes::swapped;
		}
	}

	bool parse(const QByteArray& raw) {
		rapidjson::StringStream                                 ss(raw.constData());
		rapidjson::CursorStreamWrapper<rapidjson::StringStream> csw(ss);
		json.ParseStream(csw);
		if (json.HasParseError()) {
			qCritical().noquote() << QSL("\x1B[33mProblem parsing confing file on line: %1 , pos: %2\x1B[0m").arg(csw.GetLine()).arg(csw.GetColumn());
			return false;
		}
		return true;
	}
	template <typename Type>
	void getta(const char* path, Type& def) {
		rapidjson::Value* val = rapidjson::Pointer(path).Get(json);
		if (val) {
			swapper(val, def);
			rapidjson::Pointer(path).Erase(json);
		}
	}
	QByteArray subJsonRender(rapidjson::Value* el) {
		auto&               allocator = json.GetAllocator();
		rapidjson::Document d;
		rapidjson::Value    v2(*el, allocator);

		rapidjson::StringBuffer                          buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		d.CopyFrom(*el, allocator);
		d.Accept(writer);
		QByteArray res = "\n";
		res.append(buffer.GetString(), buffer.GetSize());
		res.append("\n");
		return res;
	}

	QByteArray jsonRender() {
		rapidjson::StringBuffer                          buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		json.Accept(writer);
		QByteArray res = "\n";
		res.append(buffer.GetString(), buffer.GetSize());
		res.append("\n");
		return res;
	}

	bool stageClear(rapidjson::Value* el, bool verbose = true) {
		bool empty = true;
		//check if we have nothing left (or all set as null, so we can delete)
		if (el->IsObject()) {
			for (auto&& iter : el->GetObject()) {
				empty = empty & stageClear(&iter.value, false);
				if (!empty) {
					break;
				}
			}
		} else if (el->IsArray()) {
			for (auto&& iter : el->GetArray()) {
				empty = empty & stageClear(&iter, false);
				if (!empty) {
					break;
				}
			}
		} else if (!el->IsNull()) {
			empty = false;
		}

		if (empty) {
			el->SetNull();
			return true;
		} else if (verbose) {
			qCritical().noquote() << "non empty local JSON block" << subJsonRender(el)
			                      << "Full JSON is now" << jsonRender()
			                      << QStacker16(4, QStackerOptLight);
		}
		return false;
	}

	bool stageClear() {
		bool empty = true;
		for (auto&& iter : json.GetObject()) {
			empty = empty & stageClear(&iter.value, false);
			if (!empty) {
				break;
			}
		}
		if (!empty) {
			qCritical().noquote() << "non empty Full JSON at the end of parsing, remnant is" << jsonRender()
			                      << QStacker16(4, QStackerOptLight);
			return false;
		}
		return true;
	}
};
