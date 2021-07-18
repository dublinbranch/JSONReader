#pragma once
#include "rapidjson/includeMe.h"

#include "rapidjson/cursorstreamwrapper.h"
#include "rapidjson/pointer.h"
#include "rapidjson/prettywriter.h"

#include "JSONReaderConst.h"
#include "QStacker/qstacker.h"
#include "define.h"
#include "rapidFunkz/various.h"

#include "magicEnum/magic_from_string.hpp"
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
		switch (swapperInner(useMe, value)) {
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
	Type swapper(rapidjson::Value* obj, const char* key) {
		Type value;
		swapper(obj, key, value);
		return value;
	}

	template <typename Type>
	void swapper(rapidjson::Value* obj, const char* key, Type& value) {
		if (!obj) {
			qCritical().noquote() << "You need to check for branch existance before this point (we extract the leaf!)" << QStacker16Light();
			exit(1);
		}
		if (!obj->IsObject()) {
			qCritical().noquote() << QSL("You are searching %1 inside this element, but this is not an object, is a %2").arg(key).arg(obj->GetType()) << QStacker16Light();
			exit(1);
		}

		auto              iter  = obj->FindMember(key);
		rapidjson::Value* useMe = nullptr;
		if (iter != obj->MemberEnd()) {
			useMe = &iter->value;
		}

		auto swapRes = swapperInner(useMe, value);
		switch (swapRes) {
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

	template <typename Type>
	void swapper(rapidjson::Value* obj, Type& value) {
		switch (swapperInner(obj, value)) {
		case SwapRes::swapped:
			obj->SetNull();
			break;
		case SwapRes::typeMismatch:
			qCritical().noquote() << QSL("Type mismatch! Expecting %1 found %2 for %3")
			                                 .arg(getTypeName<Type>())
			                                 .arg(printType(mismatchedType)) +
			                             QStacker16Light();
			exit(1);
		case SwapRes::errorMissing:
			qCritical().noquote() << "missing value for REQUIRED parameter" + QStacker16Light();
			exit(1);
			break;
		case SwapRes::notFound:
			//nothing to do here
			break;
		}
	}

	template <typename Type>
	Type swapGet(rapidjson::Value* obj) {
		Type temp;
		swapper(obj, temp);
		return temp;
	}

	enum SwapRes {
		typeMismatch,
		errorMissing, //the value was needed, but not founded
		swapped,      //all ok
		notFound      // missing but not required
	};

	template <typename Type>
	void unroll(rapidjson::Value* el, Type vector) {
		vector.clear();

		auto array = el->GetArray();

		for (auto iter = array.begin(); iter != array.end();) {
			typename Type::value_type value;
			auto                      res = swapperInner(iter, value);
			switch (res) {
			case SwapRes::typeMismatch:
				qCritical().noquote() << QSL("Type mismatch! Expecting %1 found %2 for %3")
				                                 .arg(getTypeName<Type>())
				                                 .arg(printType(mismatchedType)) +
				                             QStacker16Light();
				exit(1);
			case SwapRes::swapped:
				vector.push_back(value);
				break;
			default:
				qCritical().noquote() << QSL("This should not happen: %1").arg(res) +
				                             QStacker16Light();
				exit(1);
			}
			iter = array.Erase(iter);
		}
		stageClear(el);
	}

      private:
	template <typename Type>
	SwapRes swapperInner(rapidjson::Value* obj, Type& value) {
		if (obj == nullptr) {
			//check if the default value is a sensible one
			//empty is a legit value, that is why we use a canary one `SET_ME`
			if constexpr (std::is_same<Type, QByteArray>::value) {
				if (value == JSONReaderConst::setMe8) {
					return SwapRes::errorMissing;
				}
			} else if constexpr (std::is_same<Type, QString>::value) {
				if (value == JSONReaderConst::setMe) {
					return SwapRes::errorMissing;
				}
				//higher precedence because, is_integral bool == true!
			} else if constexpr (std::is_same<Type, bool>::value) {
				//no reason ATM to enforce required default also for bool
				return SwapRes::notFound;
			} else if constexpr (std::is_integral<Type>::value) {
				if (value == JSONReaderConst::setMeInt) {
					return SwapRes::errorMissing;
				}
			} else if constexpr (std::is_floating_point<Type>::value) {
				if (qIsNaN(value)) {
					return SwapRes::errorMissing;
				}
			} else if constexpr (std::is_same<Type, QStringList>::value) {
				if (!value.isEmpty() && value[0] == JSONReaderConst::setMe) {
					return SwapRes::errorMissing;
				}
			} else if constexpr (std::is_same<Type, std::string>::value) {
				if (!value.empty() && value == JSONReaderConst::setMeSS) {
					return SwapRes::errorMissing;
				}
			} else if constexpr (std::is_enum<Type>::value) {
				if (value == Type::null) {
					return SwapRes::errorMissing;
				}
			} else {
				//poor man static assert that will also print for which type it failed
				typedef typename Type::something_made_up X;

				X x; //To avoid complain that X is defined but not used
			}
			return SwapRes::notFound;
		}

		if constexpr (std::is_same_v<Type, QByteArray>) {
			value.clear();
			value.append(obj->GetString());
			return SwapRes::swapped;
		} else if constexpr (std::is_same_v<Type, QString>) {
			value.clear();
			value.append(obj->GetString());
			return SwapRes::swapped;
		} else if constexpr (std::is_same_v<Type, std::string>) {
			value.clear();
			value.append(obj->GetString());
			return SwapRes::swapped;
		} else if constexpr (std::is_same_v<Type, QStringList>) {
			value.clear();
			for (auto& iter : obj->GetArray()) {
				value.append(iter.GetString());
			}
			return SwapRes::swapped;
			// not ok in this way because we do not check the type
			//		} else if constexpr (std::is_same<Type, double>::value) {
			//			value = obj->GetDouble();
			//			return SwapRes::swapped;
		} else if constexpr (std::is_enum_v<Type>) {
			magic_enum::fromString(obj->GetString(), value);
			return SwapRes::swapped;
		} else { //This should handle all the other
			if (obj->GetType() == rapidjson::Type::kNumberType && std::is_arithmetic_v<Type>) {
				//as suggested in https://github.com/Tencent/rapidjson/issues/823
				value = obj->GetDouble();
				return SwapRes::swapped;
			} else if (obj->template Is<Type>()) {
				value = obj->template Get<Type>();
				return SwapRes::swapped;
			}
			mismatchedType = obj->GetType();
			return SwapRes::typeMismatch;
		}
	}

      public:
	bool parse(const QByteArray& raw) {
		rapidjson::StringStream                                 ss(raw.constData());
		rapidjson::CursorStreamWrapper<rapidjson::StringStream> csw(ss);
		json.ParseStream(csw);
		if (json.HasParseError()) {
			qCritical().noquote() << QSL("\x1B[33mProblem parsing json on line: %1 , pos: %2\x1B[0m")
									 .arg(csw.GetLine())
									 .arg(csw.GetColumn()) << QStacker16Light();
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
	QByteArray subJsonRender(rapidjson::Value* el);

	QByteArray jsonRender() {
		rapidjson::StringBuffer                          buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		json.Accept(writer);
		QByteArray res = "\n";
		res.append(buffer.GetString(), buffer.GetSize());
		res.append("\n");
		return res;
	}

	//FIXME fare overload perché funzioni anche con tipi primitivi
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
