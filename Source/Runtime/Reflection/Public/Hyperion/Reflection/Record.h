#pragma once
#include "Hyperion/Reflection/ArchiveNode.h"
#include "Hyperion/Reflection/RecordCallback.h"
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <typeindex>

namespace Hyperion
{
struct FRecordDescriptor;
using FRecordVisitor = std::function<void(const FRecordDescriptor&, const void*, std::string_view)>;

struct FRecordReadContext
{
	std::string Path;
	std::vector<std::string>* Diagnostics{};

	FRecordReadContext Child(std::string_view InName) const;
	void Warn(std::string InMessage) const;
};

enum class ERecordValueKind : std::uint8_t
{
	Boolean,
	Integer,
	UnsignedInteger,
	Number,
	String,
	Record,
	Sequence,
	Map
};

struct FRecordValueShape
{
	ERecordValueKind Kind = ERecordValueKind::Record;
	bool bOptional{};
	std::size_t ElementBytes{};
	std::shared_ptr<const FRecordValueShape> Element;
	const FRecordDescriptor& (*Record)(){};
	FArchiveNode (*DefaultValue)(){};
};

enum class EPropertyWidget : std::uint8_t
{
	Default,
	Vector3
};

// Presentation is independent of persistence. Absence hides a top-level member from inspection.
struct FPropertyPresentation
{
	std::string Label;
	std::string Group;
	bool bReadOnly{};
	std::optional<double> Minimum;
	std::optional<double> Maximum;
	std::vector<std::string> Choices;
	std::string ReferenceType;
	bool bAllowResize = true;
	EPropertyWidget Widget = EPropertyWidget::Default;
	std::string Unit;
	std::string Tooltip;
	bool operator==(const FPropertyPresentation&) const = default;
};

struct FRecordMemberOptions
{
	bool bRequired{};
	bool bPersistent = true;
	std::vector<std::string> Aliases;
	std::optional<FPropertyPresentation> Inspector;
};

inline FRecordMemberOptions Inspect(std::string InLabel, std::optional<double> InMinimum = {},
                                    std::optional<double> InMaximum = {}, bool bInReadOnly = false)
{
	return {.Inspector = FPropertyPresentation{std::move(InLabel), {}, bInReadOnly, InMinimum, InMaximum}};
}

struct FRecordMember
{
	std::string Id;
	TRecordCallback<FArchiveNode(const void*)> Write;
	TRecordCallback<void(void*, const FArchiveNode&, const FRecordReadContext&)> Read;
	TRecordCallback<void(const void*, const FRecordVisitor&, std::string_view)> Visit;
	FRecordMemberOptions Options;
	const FRecordValueShape& (*Shape)(){};
};

// CPU-only editing projection. Apply receives a detached source, edited view and original view.
// Persistence continues to use the source descriptor's members.
struct FRecordDisplayLayout
{
	const FRecordDescriptor& (*Record)(){};
	TRecordCallback<std::shared_ptr<void>(const void*)> Project;
	TRecordCallback<void(void*, const void*, const void*)> Apply;
};

struct FRecordDescriptor
{
	std::string Id;
	std::uint32_t Version = 1;
	std::vector<FRecordMember> Members;
	TRecordCallback<std::shared_ptr<void>()> Create;
	TRecordCallback<void(const void*)> Validate;
	std::type_index CppType{typeid(void)};
	TRecordCallback<void(void*, void*)> Commit;
	std::uint32_t MinimumVersion = 1;
	std::map<std::uint32_t, TRecordCallback<void(FArchiveNode::FObject&)>> Migrations;
	// A migration that decodes nested records can preserve their paths and diagnostics.
	std::map<std::uint32_t, TRecordCallback<void(FArchiveNode::FObject&, const FRecordReadContext&)>> ContextMigrations;
	// Explicitly retired fields fail after migration; other unknown fields keep the normal warning policy.
	std::vector<std::string> RejectedFields;
	// Opt in where dropping a field could change the represented kind or projection.
	bool bRejectUnknownFields{};
	// Copies retain their definition identity; independently built descriptors cannot replace a registered contract.
	std::shared_ptr<const void> Definition = std::make_shared<const int>(0);
	std::shared_ptr<const FRecordDisplayLayout> DisplayLayout;
};

template<class T> const FRecordDescriptor& RecordType();

template<class TSource, class TDisplay, class TProject, class TApply>
std::shared_ptr<const FRecordDisplayLayout> MakeRecordDisplayLayout(TProject InProject, TApply InApply)
{
	return std::make_shared<const FRecordDisplayLayout>(
	    FRecordDisplayLayout{&RecordType<TDisplay>,
	                         [InProject](const void* InSource)
	                         {
		                         return std::make_shared<TDisplay>(InProject(*static_cast<const TSource*>(InSource)));
	                         },
	                         [InApply](void* InSource, const void* InEdited, const void* InOriginal)
	                         {
		                         InApply(*static_cast<TSource*>(InSource), *static_cast<const TDisplay*>(InEdited),
		                                 *static_cast<const TDisplay*>(InOriginal));
	                         }});
}

template<class T> std::span<const T> RecordEnumValues()
{
	return {};
}

void ValidateRecordDescriptor(const FRecordDescriptor& InType);
FArchiveNode WriteRecord(const FRecordDescriptor& InType, const void* InObject);
std::shared_ptr<void> ReadRecord(const FRecordDescriptor& InType, const FArchiveNode& InNode,
                                 const FRecordReadContext& InContext = {});
void ReadRecordFields(const FRecordDescriptor& InType, void* InObject, const FArchiveNode& InNode,
                      const FRecordReadContext& InContext = {});
void VisitRecord(const FRecordDescriptor& InType, const void* InObject, const FRecordVisitor& InVisitor,
                 std::string_view InPath = {});

class FRecordRegistry
{
public:
	void Register(const FRecordDescriptor& InType);
	std::shared_ptr<const FRecordDescriptor> Find(std::string_view InId) const;

	template<class T> void Register()
	{
		Register(RecordType<T>());
	}

private:
	mutable std::mutex Mutex;
	std::map<std::string, std::shared_ptr<const FRecordDescriptor>> Types;
};

// An inspection draft can be temporarily invalid. Apply only to a detached candidate, then transact it.
class FRecordDraft
{
public:
	FRecordDraft(const FRecordDescriptor& InType, const void* InValue);

	const FRecordDescriptor& GetType() const
	{
		return *Type;
	}

	std::map<std::string, FArchiveNode>& GetValues()
	{
		return Values;
	}

	void ApplyToCandidate(void* InCandidate) const;

private:
	const FRecordDescriptor* SourceType;
	const FRecordDescriptor* Type;
	std::shared_ptr<void> OriginalDisplay;
	std::map<std::string, FArchiveNode> Values;
};
} // namespace Hyperion

#include "Hyperion/Reflection/RecordValue.h"
