#pragma once
#include <memory>

namespace Hyperion
{
// Payload interfaces carry no native handles. The backend validates concrete type
// and device identity before use; identity remains valid while a payload exists.
class IRHIResource
{
public:
	virtual ~IRHIResource() = default;
	virtual const void* GetDeviceIdentity() const noexcept = 0;
};

class IRHIBuffer : public IRHIResource
{
};

class IRHITexture : public IRHIResource
{
};

class IRHIPipeline : public IRHIResource
{
};

class IRHIRecordedList : public IRHIResource
{
};

struct FBuffer
{
	std::shared_ptr<IRHIBuffer> Payload;

	explicit operator bool() const
	{
		return bool(Payload);
	}
};

struct FTexture
{
	std::shared_ptr<IRHITexture> Payload;

	explicit operator bool() const
	{
		return bool(Payload);
	}
};

struct FPipeline
{
	std::shared_ptr<IRHIPipeline> Payload;

	explicit operator bool() const
	{
		return bool(Payload);
	}
};

struct FRecordedList
{
	std::shared_ptr<IRHIRecordedList> Payload;
};
} // namespace Hyperion
