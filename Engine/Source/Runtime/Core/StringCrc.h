#pragma once

#include "Hashers/StringHash.hpp"

namespace engine
{

template<typename T>
class TStringCrc final
{
public:
	TStringCrc() = default;
	explicit constexpr TStringCrc(std::string_view sv) : m_hashValue(cd::StringHash<T>(sv)) {}
	explicit constexpr TStringCrc(const char* str, std::size_t n) : m_hashValue(cd::StringHash<T>(str, n)) {}
	TStringCrc(const TStringCrc&) = default;
	TStringCrc& operator=(const TStringCrc&) = default;
	TStringCrc(TStringCrc&&) = default;
	TStringCrc& operator=(TStringCrc&&) = default;
	~TStringCrc() = default;

	constexpr T Value() const { return m_hashValue; }
	bool operator==(const TStringCrc& other) const { return m_hashValue == other.m_hashValue; }
	bool operator!=(const TStringCrc& other) const { return m_hashValue != other.m_hashValue; }

private:
	T m_hashValue;
};

using StringCrc = TStringCrc<uint32_t>;

}