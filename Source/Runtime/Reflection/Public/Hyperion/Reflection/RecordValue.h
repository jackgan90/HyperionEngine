#pragma once
#include "Hyperion/Reflection/Record.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <type_traits>

namespace Hyperion
{
template<class T> std::string BulkElement()
{
	return std::string(std::is_floating_point_v<T> ? "f"
	                   : std::is_signed_v<T>       ? "i"
	                                               : "u") +
	       std::to_string(sizeof(T) * 8);
}

template<class T> void ValidateRecordEnum(T InValue)
{
	const auto Values = RecordEnumValues<T>();
	if (std::find(Values.begin(), Values.end(), InValue) == Values.end())
	{
		throw std::runtime_error("Invalid or unregistered archive enum value");
	}
}

template<class T> FArchiveNode WriteValue(const T& InValue);
template<class T> T ReadValue(const FArchiveNode& InNode, const FRecordReadContext& InContext = {});
template<class T> void VisitValue(const T& InValue, const FRecordVisitor& InVisitor, std::string_view InPath);

template<class T> FArchiveNode WriteBulk(const T& InValue)
{
	using FElement = typename T::value_type;
	static_assert(std::endian::native == std::endian::little, "Bulk arrays require little-endian conversion");
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

template<class T> FArchiveNode WriteValue(const T& InValue)
{
	if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, std::string>)
	{
		return FArchiveNode(InValue);
	}
	else if constexpr (std::is_enum_v<T>)
	{
		ValidateRecordEnum(InValue);
		return WriteValue(static_cast<std::underlying_type_t<T>>(InValue));
	}
	else if constexpr (std::is_integral_v<T>)
	{
		using FInteger = std::conditional_t<std::is_unsigned_v<T>, std::uint64_t, std::int64_t>;
		return FArchiveNode(static_cast<FInteger>(InValue));
	}
	else if constexpr (std::is_floating_point_v<T>)
	{
		if (!std::isfinite(InValue))
		{
			throw std::runtime_error("Non-finite archive number");
		}
		return FArchiveNode(static_cast<double>(InValue));
	}
	else if constexpr (requires { InValue.has_value(); })
	{
		return InValue ? WriteValue(*InValue) : FArchiveNode(std::monostate{});
	}
	else if constexpr (requires { typename T::mapped_type; })
	{
		static_assert(std::is_same_v<typename T::key_type, std::string>);
		FArchiveNode::FObject Items;
		for (const auto& [Key, Value] : InValue)
		{
			Items.emplace(Key, WriteValue(Value));
		}
		return FArchiveNode(std::move(Items));
	}
	else if constexpr (requires { typename T::value_type; })
	{
		using FElement = typename T::value_type;
		if constexpr ((std::is_arithmetic_v<FElement> && !std::is_same_v<FElement, bool>) ||
		              std::is_same_v<FElement, std::byte>)
		{
			return WriteBulk(InValue);
		}
		else
		{
			FArchiveNode::FArray Items;
			Items.reserve(InValue.size());
			for (const auto& Item : InValue)
			{
				Items.push_back(WriteValue(static_cast<const FElement&>(Item)));
			}
			return FArchiveNode(std::move(Items));
		}
	}
	else
	{
		return WriteRecord(RecordType<T>(), &InValue);
	}
}

template<class T> T ReadBulk(const FArchiveNode& InNode)
{
	using FElement = typename T::value_type;
	const auto& Data = std::get<FBulkData>(InNode.Value);
	const auto Bytes = Data.Data();
	if (Data.Element != BulkElement<FElement>() || Bytes.size() % sizeof(FElement))
	{
		throw std::runtime_error("Archive bulk element mismatch");
	}
	const auto Count = Bytes.size() / sizeof(FElement);
	T Value{};
	if constexpr (requires { Value.resize(Count); })
	{
		Value.resize(Count);
	}
	else if (Value.size() != Count)
	{
		throw std::runtime_error("Archive fixed array size mismatch");
	}
	if (!Bytes.empty())
	{
		std::memcpy(Value.data(), Bytes.data(), Bytes.size());
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
	return Value;
}

template<class T> T ReadInteger(const FArchiveNode& InNode)
{
	if (const auto* Value = std::get_if<std::uint64_t>(&InNode.Value))
	{
		if (*Value > static_cast<std::uint64_t>(std::numeric_limits<T>::max()))
		{
			throw std::runtime_error("Archive integer out of range");
		}
		return static_cast<T>(*Value);
	}
	const auto Value = std::get<std::int64_t>(InNode.Value);
	if constexpr (std::is_unsigned_v<T>)
	{
		if (Value < 0 || static_cast<std::uint64_t>(Value) > std::numeric_limits<T>::max())
		{
			throw std::runtime_error("Archive integer out of range");
		}
	}
	else if (Value < std::numeric_limits<T>::lowest() || Value > std::numeric_limits<T>::max())
	{
		throw std::runtime_error("Archive integer out of range");
	}
	return static_cast<T>(Value);
}

template<class T> T ReadContainer(const FArchiveNode& InNode, const FRecordReadContext& InContext)
{
	using FElement = typename T::value_type;
	if constexpr ((std::is_arithmetic_v<FElement> && !std::is_same_v<FElement, bool>) ||
	              std::is_same_v<FElement, std::byte>)
	{
		return ReadBulk<T>(InNode);
	}
	else
	{
		T Value{};
		const auto& Items = std::get<FArchiveNode::FArray>(InNode.Value);
		if constexpr (requires { Value.resize(Items.size()); })
		{
			Value.resize(Items.size());
		}
		else if (Value.size() != Items.size())
		{
			throw std::runtime_error("Archive fixed array size mismatch");
		}
		for (std::size_t Index = 0; Index < Items.size(); ++Index)
		{
			Value[Index] = ReadValue<FElement>(Items[Index], InContext.Child("[" + std::to_string(Index) + "]"));
		}
		return Value;
	}
}

template<class T> T ReadValue(const FArchiveNode& InNode, const FRecordReadContext& InContext)
{
	try
	{
		if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, std::string>)
		{
			return std::get<T>(InNode.Value);
		}
		else if constexpr (std::is_enum_v<T>)
		{
			const auto Value = static_cast<T>(ReadInteger<std::underlying_type_t<T>>(InNode));
			ValidateRecordEnum(Value);
			return Value;
		}
		else if constexpr (std::is_integral_v<T>)
		{
			return ReadInteger<T>(InNode);
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
		else if constexpr (requires(T InValue) { InValue.has_value(); })
		{
			return std::holds_alternative<std::monostate>(InNode.Value)
			           ? T{}
			           : T{ReadValue<typename T::value_type>(InNode, InContext)};
		}
		else if constexpr (requires { typename T::mapped_type; })
		{
			T Value;
			for (const auto& [Key, Item] : std::get<FArchiveNode::FObject>(InNode.Value))
			{
				Value.emplace(Key, ReadValue<typename T::mapped_type>(Item, InContext.Child("[" + Key + "]")));
			}
			return Value;
		}
		else if constexpr (requires { typename T::value_type; })
		{
			return ReadContainer<T>(InNode, InContext);
		}
		else
		{
			return std::move(*std::static_pointer_cast<T>(ReadRecord(RecordType<T>(), InNode, InContext)));
		}
	}
	catch (const std::exception& Error)
	{
		const std::string Message = Error.what();
		if (InContext.Path.empty() || Message.starts_with(InContext.Path))
		{
			throw;
		}
		throw std::runtime_error(InContext.Path + ": " + Message);
	}
}

template<class T> void VisitValue(const T& InValue, const FRecordVisitor& InVisitor, std::string_view InPath)
{
	if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T> || std::is_same_v<T, std::string> ||
	              std::is_same_v<T, std::byte>)
	{
		return;
	}
	else if constexpr (requires { InValue.has_value(); })
	{
		if (InValue)
		{
			VisitValue(*InValue, InVisitor, InPath);
		}
	}
	else if constexpr (requires { typename T::mapped_type; })
	{
		for (const auto& [Key, Value] : InValue)
		{
			VisitValue(Value, InVisitor, std::string(InPath) + "[" + Key + "]");
		}
	}
	else if constexpr (requires { typename T::value_type; })
	{
		using FElement = typename T::value_type;
		if constexpr (!std::is_arithmetic_v<FElement> && !std::is_same_v<FElement, std::byte>)
		{
			for (std::size_t Index = 0; Index < InValue.size(); ++Index)
			{
				VisitValue(InValue[Index], InVisitor, std::string(InPath) + "[" + std::to_string(Index) + "]");
			}
		}
	}
	else
	{
		VisitRecord(RecordType<T>(), &InValue, InVisitor, InPath);
	}
}

template<class T, class M> FRecordMember Member(std::string InId, M T::* InMember, FRecordMemberOptions InOptions = {})
{
	return {std::move(InId),
	        [InMember](const void* InObject)
	        {
		        return WriteValue(static_cast<const T*>(InObject)->*InMember);
	        },
	        [InMember](void* InObject, const FArchiveNode& InNode, const FRecordReadContext& InContext)
	        {
		        static_cast<T*>(InObject)->*InMember = ReadValue<M>(InNode, InContext);
	        },
	        [InMember](const void* InObject, const FRecordVisitor& InVisitor, std::string_view InPath)
	        {
		        VisitValue(static_cast<const T*>(InObject)->*InMember, InVisitor, InPath);
	        },
	        std::move(InOptions)};
}

template<class T>
FRecordDescriptor MakeRecord(std::string InId, std::vector<FRecordMember> InMembers, std::uint32_t InVersion = 1,
                             std::function<void(const T&)> InValidate = {})
{
	return {std::move(InId),
	        InVersion,
	        std::move(InMembers),
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
	        },
	        typeid(T),
	        [](void* InDestination, void* InSource)
	        {
		        if constexpr (std::is_nothrow_move_assignable_v<T>)
		        {
			        *static_cast<T*>(InDestination) = std::move(*static_cast<T*>(InSource));
		        }
		        else
		        {
			        (void)InDestination;
			        (void)InSource;
			        throw std::logic_error("Use ReadValue/ReadRecord for a type without noexcept assignment");
		        }
	        }};
}
} // namespace Hyperion
