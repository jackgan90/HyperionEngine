# Editor placement resources

The five primitive models, shared PBR material, white texture and three light icon textures are native Engine content. Regenerate them from the repository root after building `hyperion_asset_tool`:

```powershell
./out/build/debug/bin/hyperion_asset_tool.exe --authoring build-placement
```

`Scene/PrimitiveShapes` defines deterministic unit-sized geometry with stable IDs, normals, UVs and tangents. The generator uses the existing model material conversion path for complete PBR defaults. Fixed asset identities and content-derived revisions make repeated generation byte-stable.

`Icons/Generation.json` records the built-in image generation tool and full prompts. The transparent PNG originals are retained; imported native textures use the generated mip chain starting at a maximum dimension of 512. Pixels remain display encoded for the GuiRenderer UNORM sampling path. The bulb, sun and spotlight represent point, directional and spot lights; direction arrows are drawn separately by the editor.
