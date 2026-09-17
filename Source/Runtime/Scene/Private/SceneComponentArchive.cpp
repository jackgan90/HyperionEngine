#include "Hyperion/Scene/SceneManifest.h"
#include <set>

namespace Hyperion
{
namespace
{
template<class T> std::string ComponentId(const FSceneNodeEntry& InEntry)
{
	const auto& Type = RecordType<T>().Id;
	const auto It = InEntry.ComponentIds.find(Type);
	return It == InEntry.ComponentIds.end() ? Type : It->second;
}

FArchiveNode Envelope(std::string InId, std::string InType, FArchiveNode InState)
{
	return FArchiveNode(
	    FArchiveNode::FObject{{"id", WriteValue(InId)}, {"type", WriteValue(InType)}, {"state", std::move(InState)}});
}

template<class T>
void Append(FArchiveNode::FArray& InValues, const FSceneNodeEntry& InEntry, const std::optional<T>& InValue)
{
	if (InValue)
	{
		InValues.push_back(Envelope(ComponentId<T>(InEntry), RecordType<T>().Id, WriteValue(*InValue)));
	}
}

FArchiveNode WriteComponents(const void* InObject)
{
	const auto& Entry = *static_cast<const FSceneNodeEntry*>(InObject);
	if (!Entry.Extensions.Unknown().empty())
	{
		throw std::runtime_error("Cannot save unknown components: install their component registration first");
	}
	FArchiveNode::FArray Values;
	Values.push_back(Envelope(ComponentId<FSceneTransform>(Entry), RecordType<FSceneTransform>().Id,
	                          WriteValue(FSceneTransform{Entry.Parent, Entry.Transform})));
	if (Entry.Model)
	{
		Values.push_back(Envelope(ComponentId<FSceneModelComponent>(Entry), RecordType<FSceneModelComponent>().Id,
		                          WriteValue(*Entry.Model)));
	}
	Append(Values, Entry, Entry.Camera);
	Append(Values, Entry, Entry.DirectionalLight);
	Append(Values, Entry, Entry.EnvironmentLight);
	Append(Values, Entry, Entry.PointLight);
	Append(Values, Entry, Entry.SpotLight);
	for (const auto& Component : Entry.Extensions.All())
	{
		if (Component.Get())
		{
			Values.push_back(
			    Envelope(Component.Id, Component.Type->Id, WriteRecord(*Component.Type->Record, Component.Get())));
		}
	}
	return FArchiveNode(std::move(Values));
}

bool ReadBuiltin(FSceneNodeEntry& InEntry, const std::string& InType, const FArchiveNode& InState,
                 const FRecordReadContext& InContext)
{
	if (InType == RecordType<FSceneTransform>().Id)
	{
		const auto Transform = ReadValue<FSceneTransform>(InState, InContext);
		InEntry.Parent = Transform.Parent;
		InEntry.Transform = Transform.Local;
	}
	else if (InType == RecordType<FSceneModelComponent>().Id)
	{
		InEntry.Model = ReadValue<FSceneNodeModel>(InState, InContext);
	}
	else if (InType == RecordType<FSceneCamera>().Id)
	{
		InEntry.Camera = ReadValue<FSceneCamera>(InState, InContext);
	}
	else if (InType == RecordType<FSceneDirectionalLight>().Id)
	{
		InEntry.DirectionalLight = ReadValue<FSceneDirectionalLight>(InState, InContext);
	}
	else if (InType == RecordType<FSceneEnvironmentLight>().Id)
	{
		InEntry.EnvironmentLight = ReadValue<FSceneEnvironmentLight>(InState, InContext);
	}
	else if (InType == RecordType<FScenePointLight>().Id)
	{
		InEntry.PointLight = ReadValue<FScenePointLight>(InState, InContext);
	}
	else if (InType == RecordType<FSceneSpotLight>().Id)
	{
		InEntry.SpotLight = ReadValue<FSceneSpotLight>(InState, InContext);
	}
	else
	{
		return false;
	}
	return true;
}

void ReadComponents(void* InObject, const FArchiveNode& InNode, const FRecordReadContext& InContext)
{
	auto& Entry = *static_cast<FSceneNodeEntry*>(InObject);
	std::set<std::string> Ids;
	for (const auto& Value : std::get<FArchiveNode::FArray>(InNode.Value))
	{
		const auto& Fields = std::get<FArchiveNode::FObject>(Value.Value);
		const auto Id = ReadValue<std::string>(Fields.at("id"), InContext);
		const auto Type = ReadValue<std::string>(Fields.at("type"), InContext);
		if (Fields.size() != 3 || Id.empty() || Type.empty() || !Ids.insert(Id).second)
		{
			throw std::invalid_argument("Invalid or duplicate component envelope");
		}
		const auto& State = Fields.at("state");
		if (ReadBuiltin(Entry, Type, State, InContext.Child(Id)))
		{
			if (!Entry.ComponentIds.emplace(Type, Id).second)
			{
				throw std::invalid_argument("Duplicate unique scene component");
			}
		}
		else if (const auto Descriptor = SceneComponentRegistry().TryFind(Type))
		{
			Entry.Extensions.Add(Id, Type);
			ReadRecordFields(*Descriptor->Record, Entry.Extensions.Find(Id)->Edit(), State, InContext.Child(Id));
		}
		else
		{
			Entry.Extensions.AddOpaque(Id, std::make_shared<const FArchiveNode>(Value));
		}
	}
	if (!Entry.ComponentIds.contains(RecordType<FSceneTransform>().Id))
	{
		throw std::invalid_argument("Scene object is missing its required Transform component");
	}
}

void VisitComponents(const void* InObject, const FRecordVisitor& InVisitor, std::string_view InPath)
{
	const auto& Entry = *static_cast<const FSceneNodeEntry*>(InObject);
	// Match the canonical envelope array, including its always-present Transform at index zero.
	std::size_t Index = 1;
	const auto NextPath = [&]
	{
		return std::string(InPath) + "[" + std::to_string(Index++) + "][state]";
	};
	const auto VisitOptional = [&](const auto& InValue)
	{
		if (InValue)
		{
			VisitValue(*InValue, InVisitor, NextPath());
		}
	};
	VisitOptional(Entry.Model);
	VisitOptional(Entry.Camera);
	VisitOptional(Entry.DirectionalLight);
	VisitOptional(Entry.EnvironmentLight);
	VisitOptional(Entry.PointLight);
	VisitOptional(Entry.SpotLight);
	for (const auto& Component : Entry.Extensions.All())
	{
		if (Component.Get())
		{
			VisitRecord(*Component.Type->Record, Component.Get(), InVisitor, NextPath());
		}
	}
}
} // namespace

FRecordMember SceneComponentArchiveMember()
{
	return {"components", WriteComponents, ReadComponents, VisitComponents, {true}};
}

void MigrateSceneComponents(FArchiveNode::FObject& InFields)
{
	FArchiveNode::FArray Values;
	FSceneTransform Transform;
	if (const auto It = InFields.find("parent"); It != InFields.end())
	{
		Transform.Parent = ReadValue<std::string>(It->second);
		InFields.erase(It);
	}
	if (const auto It = InFields.find("transform"); It != InFields.end())
	{
		Transform.Local = ReadValue<FMat4>(It->second);
		InFields.erase(It);
	}
	Values.push_back(
	    Envelope(RecordType<FSceneTransform>().Id, RecordType<FSceneTransform>().Id, WriteValue(Transform)));
	const std::pair<const char*, std::string> Types[] = {{"model", RecordType<FSceneModelComponent>().Id},
	                                                     {"camera", RecordType<FSceneCamera>().Id},
	                                                     {"directionalLight", RecordType<FSceneDirectionalLight>().Id},
	                                                     {"environmentLight", RecordType<FSceneEnvironmentLight>().Id},
	                                                     {"pointLight", RecordType<FScenePointLight>().Id},
	                                                     {"spotLight", RecordType<FSceneSpotLight>().Id}};
	for (const auto& [Field, Type] : Types)
	{
		if (const auto It = InFields.find(Field); It != InFields.end())
		{
			if (!std::holds_alternative<std::monostate>(It->second.Value))
			{
				Values.push_back(Envelope(Type, Type, std::move(It->second)));
			}
			InFields.erase(It);
		}
	}
	if (!InFields.emplace("components", FArchiveNode(std::move(Values))).second)
	{
		throw std::invalid_argument("Legacy scene contains conflicting component representations");
	}
}
} // namespace Hyperion
