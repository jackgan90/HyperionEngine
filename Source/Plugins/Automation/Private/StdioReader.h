#pragma once
#include <memory>
#include <string>

namespace Hyperion
{
class FStdioReader
{
public:
	FStdioReader();
	~FStdioReader();
	// Nonblocking pipe reads; regular files read a bounded chunk. No detached stdin worker.
	std::string Read();
	bool IsEof() const;

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
} // namespace Hyperion
