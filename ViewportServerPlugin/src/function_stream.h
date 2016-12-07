#pragma once

#include <streambuf>
#include <ostream>
#include <functional>

typedef std::function<void(std::string)> function_type;

class functionbuf : public std::streambuf
{
public:
	explicit functionbuf(function_type const& function)
		: d_function(function) {
		this->setp(this->d_buffer, this->d_buffer + sizeof(this->d_buffer) - 1);
	}

private:
	typedef std::streambuf::traits_type traits_type;
	function_type d_function;
	char          d_buffer[1024];
	int overflow(int c) override
	{
		if (!traits_type::eq_int_type(c, traits_type::eof())) {
			*this->pptr() = traits_type::to_char_type(c);
			this->pbump(1);
		}
		return this->sync() ? traits_type::not_eof(c) : traits_type::eof();
	}
	int sync() override
	{
		if (this->pbase() != this->pptr()) {
			this->d_function(std::string(this->pbase(), this->pptr()));
			this->setp(this->pbase(), this->epptr());
		}
		return 0;
	}
};

class ofunctionstream : private virtual functionbuf, public std::ostream
{
public:
	explicit ofunctionstream(function_type const& function, bool flush_on_inserting = false)
		: functionbuf(function)
		, std::ostream(static_cast<std::streambuf*>(this))
	{
		if (flush_on_inserting)
			this->flags(std::ios_base::unitbuf);
	}
};