#pragma once
#include "Hyperion/RHI/RHIGraphicsState.h"
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

// Immutable physical description of the base level; no native handles are exposed.
struct FRHITextureInfo
{
	std::uint32_t Width{};
	std::uint32_t Height{};
	ERHIDepthFormat DepthFormat = ERHIDepthFormat::None;
	ERHIColorFormat ColorFormat = ERHIColorFormat::Rgba8Unorm;
	bool bColorTarget{};
};

class IRHITexture : public IRHIResource
{
public:
	virtual FRHITextureInfo GetInfo() const noexcept = 0;
};

class IRHIPipeline : public IRHIResource
{
};

class IRHIRecordedList : public IRHIResource
{
};

class IRHISampler : public IRHIResource
{
};

class IRHIResourceBindingLayout : public IRHIResource
{
};

class IRHIResourceBindingSet : public IRHIResource
{
};

struct FSampler
{
	std::shared_ptr<IRHISampler> Payload;
	bool operator==(const FSampler&) const = default;

	explicit operator bool() const
	{
		return bool(Payload);
	}
};

struct FResourceBindingLayout
{
	std::shared_ptr<IRHIResourceBindingLayout> Payload;
	bool operator==(const FResourceBindingLayout&) const = default;

	explicit operator bool() const
	{
		return bool(Payload);
	}
};

struct FResourceBindingSet
{
	std::shared_ptr<IRHIResourceBindingSet> Payload;
	bool operator==(const FResourceBindingSet&) const = default;

	explicit operator bool() const
	{
		return bool(Payload);
	}
};

struct FBuffer
{
	std::shared_ptr<IRHIBuffer> Payload;
	bool operator==(const FBuffer&) const = default;

	explicit operator bool() const
	{
		return bool(Payload);
	}
};

struct FTexture
{
	std::shared_ptr<IRHITexture> Payload;
	bool operator==(const FTexture&) const = default;

	explicit operator bool() const
	{
		return bool(Payload);
	}
};

struct FPipeline
{
	std::shared_ptr<IRHIPipeline> Payload;
	bool operator==(const FPipeline&) const = default;

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
