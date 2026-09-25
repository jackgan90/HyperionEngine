#pragma once
#include "Hyperion/AssetImport/ImportWorkspace.h"
#include "Hyperion/Gui/Gui.h"

namespace Hyperion
{
class FAssetImportPanel
{
public:
	FAssetImportPanel(FAssetImportWorkspace* InImports, FContentRootService& InRoots);
	void Draw(FGui& InGui);
	void Process(FNativeSurface InOwner);
	void Show();
	bool bOpen{};
	FImportRequest Request;
	FVec4 SourceBounds{};
	FVec4 OutputBounds{};
	FVec4 ImportBounds{};
	FVec4 CloseBounds{};
	FVec4 PreviewNameBounds{};
	FVec4 UndoBounds{};
	FVec4 RedoBounds{};
	std::string DraftId;

private:
	void DrawSource(FGui& InGui);
	void DrawSettings(FGui& InGui);
	void DrawOutput(FGui& InGui);
	void DrawTasks(FGui& InGui);
	void UpdateSource();
	FImportRequest Snapshot() const;
	std::string RequestKey() const;
	void ProcessDraft();
	void DrawPreview(FGui& InGui);
	void DrawDraftProperties(FGui& InGui, FImportDraftInfo& InInfo);
	void DrawDraftModel(FGui& InGui, FImportDraftInfo& InInfo);
	void DrawDraftMaterial(FGui& InGui, FImportDraftInfo& InInfo);
	void ApplyDraftEdit(const FImportDraftInfo& InInfo, FImportPropertyEdits InEdits);
	FImportDraftInfo PreviewInfo;
	std::string PreparedKey;
	bool bPreviewOpen{};
	bool bPreviewFocus{};
	bool bSourceEditing{};
	bool bAutoPrepare{};
	bool bRefreshDraft{};
	std::uint32_t PreviewOffset{};
	std::size_t SelectedNode{};
	std::size_t SelectedPrimitive{};
	FAssetImportWorkspace* Imports{};
	FContentRootService& Roots;
	std::string PreviousSource;
	std::string SuggestedOutput;
	std::string Message;
	std::size_t TypeIndex{};
	std::size_t EncodingIndex = 1;
	bool bBakeSettings{};
	bool bAdvanced{};
	bool bBrowse{};
	bool bFocus{};
	FEnvironmentBakeSettings Bake;
};
} // namespace Hyperion
