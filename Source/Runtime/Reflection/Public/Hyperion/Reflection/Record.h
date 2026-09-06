#pragma once
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace Hyperion
{
struct FBulkData
{
	std::string Element;
	std::vector<std::byte> Bytes;
};

struct FArchiveNode
{
	using FArray = std::vector<FArchiveNode>;
	using FObject = std::map<std::string, FArchiveNode>;
	std::variant<bool, std::int64_t, double, std::string, FBulkData, FArray, FObject> Value;
	FArchiveNode() = default;

	template<class T> explicit FArchiveNode(T InValue) : Value(std::move(InValue))
	{
	}
};

struct FRecordMember
{
	std::string Id;
	std::function<FArchiveNode(const void*)> Write;
	std::function<void(void*, const FArchiveNode&)> Read;
};

struct FRecordDescriptor
{
	std::string Id;
	std::uint32_t Version = 1;
	std::vector<FRecordMember> Members;
	std::function<std::shared_ptr<void>()> Create;
	std::function<void(const void*)> Validate;
};

template<class T> const FRecordDescriptor& RecordType();
FArchiveNode WriteRecord(const FRecordDescriptor& InType, const void* InObject);
std::shared_ptr<void> ReadRecord(const FRecordDescriptor& InType, const FArchiveNode& InNode);
void ReadRecordFields(const FRecordDescriptor& InType, void* InObject, const FArchiveNode& InNode);

template<class T> std::string BulkElement()
{
	return std::string(std::is_floating_point_v<T> ? "f"
	                   : std::is_signed_v<T>       ? "i"
	                                               : "u") +
	       std::to_string(sizeof(T) * 8);
}

template<class T> FArchiveNode WriteValue(const T& InValue)
{
	if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, std::string>)
	{
		return FArchiveNode(InValue);
	}
	else if constexpr (std::is_enum_v<T>)
	{
		return WriteValue(static_cast<std::underlying_type_t<T>>(InValue));
	}
	else if constexpr (std::is_integral_v<T>)
	{
		if constexpr (std::is_unsigned_v<T>)
		{
			if (InValue > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
			{
				throw std::runtime_error("Archive integer out of range");
			}
		}
		return FArchiveNode(static_cast<std::int64_t>(InValue));
	}
	else if constexpr (std::is_floating_point_v<T>)
	{
		if (!std::isfinite(InValue))
		{
			throw std::runtime_error("Non-finite archive number");
		}
		return FArchiveNode(static_cast<double>(InValue));
	}
	else if constexpr (requires {
		                   typename T::value_type;
		                   InValue.size();
	                   })
	{
		using FElement = typename T::value_type;
		if constexpr ((std::is_arithmetic_v<FElement> && !std::is_same_v<FElement, bool>) ||
		              std::is_same_v<FElement, std::byte>)
		{
			static_assert(std::endian::native == std::endian::little,
			              "Bulk archive conversion is required on big-endian hosts");
			if constexpr (std::is_floating_point_v<FElement>)
			{
				for (const auto Item : InValue)
				{
					if (!std::isfinite(Item))
					{
						throw std::runtime_error("Non-finite bulk value");
					}
				}
			}
			FBulkData Data{BulkElement<FElement>(), std::vector<std::byte>(InValue.size() * sizeof(FElement))};
			if (!Data.Bytes.empty())
			{
				std::memcpy(Data.Bytes.data(), InValue.data(), Data.Bytes.size());
			}
			return FArchiveNode(std::move(Data));
		}
		else
		{
			FArchiveNode::FArray Items;
			Items.reserve(InValue.size());
			for (const auto& Item : InValue)
			{
				Items.push_back(WriteValue(Item));
			}
			return FArchiveNode(std::move(Items));
		}
	}
	else
	{
		return WriteRecord(RecordType<T>(), &InValue);
	}
}

template<class T> T ReadValue(const FArchiveNode& InNode)
{
	if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, std::string>)
	{
		return std::get<T>(InNode.Value);
	}
	else if constexpr (std::is_enum_v<T>)
	{
		return static_cast<T>(ReadValue<std::underlying_type_t<T>>(InNode));
	}
	else if constexpr (std::is_integral_v<T>)
	{
		const auto Value = std::get<std::int64_t>(InNode.Value);
		if (static_cast<long double>(Value) < static_cast<long double>(std::numeric_limits<T>::lowest()) ||
		    static_cast<long double>(Value) > static_cast<long double>(std::numeric_limits<T>::max()))
		{
			throw std::runtime_error("Archive integer out of range");
		}
		return static_cast<T>(Value);
	}
	else if constexpr (std::is_floating_point_v<T>)
	{
		const auto Value = std::get<double>(InNode.Value);
		if (!std::isfinite(Value) || std::abs(Value) > std::numeric_limits<T>::max())
		{
			throw std::runtime_error("Invalid archive number");
		}
		return static_cast<T>(Value);
	}
	else if constexpr (requires { typename T::value_type; })
	{
		using FElement = typename T::value_type;
		T Value{};
		if constexpr ((std::is_arithmetic_v<FElement> && !std::is_same_v<FElement, bool>) ||
		              std::is_same_v<FElement, std::byte>)
		{
			const auto& Data = std::get<FBulkData>(InNode.Value);
			if (Data.Element != BulkElement<FElement>() || Data.Bytes.size() % sizeof(FElement))
			{
				throw std::runtime_error("Archive bulk element mismatch");
			}
			const auto Count = Data.Bytes.size() / sizeof(FElement);
			if constexpr (requires { Value.resize(Count); })
			{
				Value.resize(Count);
			}
			else if (Value.size() != Count)
			{
				throw std::runtime_error("Archive fixed array size mismatch");
			}
			if (!Data.Bytes.empty())
			{
				std::memcpy(Value.data(), Data.Bytes.data(), Data.Bytes.size());
			}
			if constexpr (std::is_floating_point_v<FElement>)
			{
				for (const auto Item : Value)
				{
					if (!std::isfinite(Item))
					{
						throw std::runtime_error("Non-finite bulk value");
					}
				}
			}
		}
		else
		{
			const auto& Items = std::get<FArchiveNode::FArray>(InNode.Value);
			if constexpr (requires { Value.reserve(Items.size()); })
			{
				Value.reserve(Items.size());
				for (const auto& Item : Items)
				{
					Value.push_back(ReadValue<FElement>(Item));
				}
			}
			else
			{
				if (Value.size() != Items.size())
				{
					throw std::runtime_error("Archive fixed array size mismatch");
				}
				for (std::size_t Index = 0; Index < Items.size(); ++Index)
				{
					Value[Index] = ReadValue<FElement>(Items[Index]);
				}
			}
		}
		return Value;
	}
	else
	{
		T Value{};
		ReadRecordFields(RecordType<T>(), &Value, InNode);
		return Value;
	}
}

template<class T, class M> FRecordMember Member(std::string InId, M T::* InMember)
{
	return {std::move(InId),
	        [InMember](const void* InObject)
	        {
		        return WriteValue(static_cast<const T*>(InObject)->*InMember);
	        },
	        [InMember](void* InObject, const FArchiveNode& InNode)
	        {
		        static_cast<T*>(InObject)->*InMember = ReadValue<M>(InNode);
	        }};
}

template<class T>
FRecordDescriptor MakeRecord(std::string InId, std::vector<FRecordMember> InMembers, std::uint32_t InVersion = 1,
                             std::function<void(const T&)> InValidate = {})
{
	return {std::move(InId), InVersion, std::move(InMembers),
	        []
	        {
		        return std::make_shared<T>();
	        },
	        [InValidate](const void* InObject)
	        {
		        if (InValidate)
		        {
			        InValidate(*static_cast<const T*>(InObject));
		        }
	        }};
}
} // namespace Hyperion
