#pragma once
#include <stdexcept>
#include <string>
#define HYP_CHECK(Condition)                                                                                           \
	do                                                                                                                 \
	{                                                                                                                  \
		if (!(Condition))                                                                                              \
		{                                                                                                              \
			throw std::runtime_error(std::string("Check failed: ") + #Condition);                                      \
		}                                                                                                              \
	} while (false)
