// Migration baseline: shipped layouts and wire identities captured before automatic packing.
// These expected values are independent of the layout builder and its declaration visitors.
HYP_EXPECT_UNIFORM(FrameInfo, 16, "")
HYP_EXPECT_FIELD("Time", 0, "Engine.Frame.Time", Frame, Float, 1, 1, General, false, false, Default)
HYP_EXPECT_FIELD("Index", 4, "Engine.Frame.Index", Frame, Uint, 1, 1, General, false, false, Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(HyperionViewV1, 80, "")
HYP_EXPECT_FIELD("ViewProjection", 0, "Engine.View.ViewProjection", View, Float, 4, 4, General, true, false, Default)
HYP_EXPECT_FIELD("CameraPosition", 64, "Engine.View.CameraPosition", View, Float, 3, 1, General, true, false, Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(ClusterViewV1, 80, "")
HYP_EXPECT_FIELD("ClusterViewport", 0, "Engine.View.ClusterViewport", View, Float, 4, 1, General, false, false, Default)
HYP_EXPECT_FIELD("ClusterGrid", 16, "Engine.View.ClusterGrid", View, Float, 4, 1, General, false, false, Default)
HYP_EXPECT_FIELD("ClusterCamera", 32, "Engine.View.ClusterCamera", View, Float, 4, 1, General, false, false, Default)
HYP_EXPECT_FIELD("ClusterForward", 48, "Engine.View.ClusterForward", View, Float, 4, 1, General, false, false, Default)
HYP_EXPECT_FIELD("ClusterDepth", 64, "Engine.View.ClusterDepth", View, Float, 4, 1, General, false, false, Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(EnvironmentV1, 176, "")
HYP_EXPECT_FIELD("EnvironmentControl", 0, "Engine.Scene.EnvironmentControl", Scene, Float, 4, 1, Environment, true,
                 true, Default)
HYP_EXPECT_FIELD("EnvironmentRotation", 16, "Engine.Scene.EnvironmentRotation", Scene, Float, 4, 1, Environment, true,
                 true, Default)
HYP_EXPECT_FIELD("EnvironmentSh0", 32, "Engine.Scene.EnvironmentSh0", Scene, Float, 4, 1, Environment, true, true,
                 Default)
HYP_EXPECT_FIELD("EnvironmentSh1", 48, "Engine.Scene.EnvironmentSh1", Scene, Float, 4, 1, Environment, true, true,
                 Default)
HYP_EXPECT_FIELD("EnvironmentSh2", 64, "Engine.Scene.EnvironmentSh2", Scene, Float, 4, 1, Environment, true, true,
                 Default)
HYP_EXPECT_FIELD("EnvironmentSh3", 80, "Engine.Scene.EnvironmentSh3", Scene, Float, 4, 1, Environment, true, true,
                 Default)
HYP_EXPECT_FIELD("EnvironmentSh4", 96, "Engine.Scene.EnvironmentSh4", Scene, Float, 4, 1, Environment, true, true,
                 Default)
HYP_EXPECT_FIELD("EnvironmentSh5", 112, "Engine.Scene.EnvironmentSh5", Scene, Float, 4, 1, Environment, true, true,
                 Default)
HYP_EXPECT_FIELD("EnvironmentSh6", 128, "Engine.Scene.EnvironmentSh6", Scene, Float, 4, 1, Environment, true, true,
                 Default)
HYP_EXPECT_FIELD("EnvironmentSh7", 144, "Engine.Scene.EnvironmentSh7", Scene, Float, 4, 1, Environment, true, true,
                 Default)
HYP_EXPECT_FIELD("EnvironmentSh8", 160, "Engine.Scene.EnvironmentSh8", Scene, Float, 4, 1, Environment, true, true,
                 Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(HyperionSceneV1, 48, "")
HYP_EXPECT_FIELD("MainLightDirection", 0, "Engine.Scene.MainDirectionalLightDirection", Scene, Float, 3, 1, General,
                 true, true, Default)
HYP_EXPECT_FIELD("MainLightColor", 16, "Engine.Scene.MainDirectionalLightColor", Scene, Float, 3, 1, General, true,
                 true, Default)
HYP_EXPECT_FIELD("AmbientColor", 32, "Engine.Scene.AmbientColor", Scene, Float, 3, 1, General, true, true, Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(ShadowViewV1, 352, "")
HYP_EXPECT_FIELD("ShadowMatrix0", 0, "Engine.View.ShadowMatrix0", View, Float, 4, 4, Shadow, false, false, Default)
HYP_EXPECT_FIELD("ShadowMatrix1", 64, "Engine.View.ShadowMatrix1", View, Float, 4, 4, Shadow, false, false, Default)
HYP_EXPECT_FIELD("ShadowMatrix2", 128, "Engine.View.ShadowMatrix2", View, Float, 4, 4, Shadow, false, false, Default)
HYP_EXPECT_FIELD("ShadowMatrix3", 192, "Engine.View.ShadowMatrix3", View, Float, 4, 4, Shadow, false, false, Default)
HYP_EXPECT_FIELD("ShadowSplits", 256, "Engine.View.ShadowSplits", View, Float, 4, 1, Shadow, false, false, Default)
HYP_EXPECT_FIELD("ShadowTexels", 272, "Engine.View.ShadowTexels", View, Float, 4, 1, Shadow, false, false, Default)
HYP_EXPECT_FIELD("ShadowRanges", 288, "Engine.View.ShadowRanges", View, Float, 4, 1, Shadow, false, false, Default)
HYP_EXPECT_FIELD("ShadowCamera", 304, "Engine.View.ShadowCamera", View, Float, 4, 1, Shadow, false, false, Default)
HYP_EXPECT_FIELD("ShadowFilter", 320, "Engine.View.ShadowFilter", View, Float, 4, 1, Shadow, false, false, Default)
HYP_EXPECT_FIELD("ShadowControl", 336, "Engine.View.ShadowControl", View, Float, 4, 1, Shadow, false, false, Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(HyperionObjectV1, 144, "")
HYP_EXPECT_FIELD("World", 0, "Engine.Object.World", Object, Float, 4, 4, General, false, false, Default)
HYP_EXPECT_FIELD("Normal", 64, "Engine.Object.Normal", Object, Float, 4, 4, General, false, false, Default)
HYP_EXPECT_FIELD("OrientationSign", 128, "Engine.Object.OrientationSign", Object, Float, 1, 1, General, false, false,
                 Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(DrawConstants, 64, "")
HYP_EXPECT_FIELD("TransformMatrix", 0, "Engine.Object.WorldViewProjection", Object, Float, 4, 4, General, false, false,
                 Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(HyperionMaterialV1, 96, "")
HYP_EXPECT_FIELD("BaseColor", 0, "Pbr.BaseColorFactor", Material, Float, 4, 1, Pbr, false, false, Color)
HYP_EXPECT_FIELD("Emissive", 16, "Pbr.EmissiveFactor", Material, Float, 3, 1, Pbr, false, false, HdrColor)
HYP_EXPECT_FIELD("NormalScale", 28, "Pbr.NormalScale", Material, Float, 1, 1, Pbr, false, false, Default)
HYP_EXPECT_FIELD("Metallic", 32, "Pbr.MetallicFactor", Material, Float, 1, 1, Pbr, false, false, Default)
HYP_EXPECT_FIELD("Roughness", 36, "Pbr.RoughnessFactor", Material, Float, 1, 1, Pbr, false, false, Default)
HYP_EXPECT_FIELD("OcclusionStrength", 40, "Pbr.OcclusionStrength", Material, Float, 1, 1, Pbr, false, false, Default)
HYP_EXPECT_FIELD("AlphaCutoff", 44, "Pbr.AlphaCutoff", Material, Float, 1, 1, Pbr, false, false, Default)
HYP_EXPECT_FIELD("BaseColorUv", 48, "Pbr.BaseColorUvSet", Material, Uint, 1, 1, Pbr, false, false, UvSet)
HYP_EXPECT_FIELD("MetallicRoughnessUv", 52, "Pbr.MetallicRoughnessUvSet", Material, Uint, 1, 1, Pbr, false, false,
                 UvSet)
HYP_EXPECT_FIELD("NormalUv", 56, "Pbr.NormalUvSet", Material, Uint, 1, 1, Pbr, false, false, UvSet)
HYP_EXPECT_FIELD("OcclusionUv", 60, "Pbr.OcclusionUvSet", Material, Uint, 1, 1, Pbr, false, false, UvSet)
HYP_EXPECT_FIELD("EmissiveUv", 64, "Pbr.EmissiveUvSet", Material, Uint, 1, 1, Pbr, false, false, UvSet)
HYP_EXPECT_FIELD("AlphaMode", 68, "Pbr.AlphaMode", Material, Uint, 1, 1, Pbr, false, false, Default)
HYP_EXPECT_FIELD("bDoubleSided", 72, "Pbr.DoubleSided", Material, Bool, 1, 1, Pbr, false, false, Default)
HYP_EXPECT_FIELD("bUnlit", 76, "Pbr.Unlit", Material, Bool, 1, 1, Pbr, false, false, Default)
HYP_EXPECT_FIELD("bHasNormal", 80, "Pbr.HasNormal", Material, Bool, 1, 1, Pbr, false, false, Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(ContactV1, 192, "ContactShadow")
HYP_EXPECT_FIELD("ContactShadow.ViewProjection", 0, "ContactShadow.ContactV1.ViewProjection", View, Float, 4, 4,
                 General, false, false, Default)
HYP_EXPECT_FIELD("ContactShadow.InverseViewProjection", 64, "ContactShadow.ContactV1.InverseViewProjection", View,
                 Float, 4, 4, General, false, false, Default)
HYP_EXPECT_FIELD("ContactShadow.Viewport", 128, "ContactShadow.ContactV1.Viewport", View, Float, 4, 1, General, false,
                 false, Default)
HYP_EXPECT_FIELD("ContactShadow.Eye", 144, "ContactShadow.ContactV1.Eye", View, Float, 3, 1, General, false, false,
                 Default)
HYP_EXPECT_FIELD("ContactShadow.LightDirection", 160, "ContactShadow.ContactV1.LightDirection", Scene, Float, 3, 1,
                 General, false, false, Default)
HYP_EXPECT_FIELD("ContactShadow.RayLength", 172, "ContactShadow.ContactV1.RayLength", Pass, Float, 1, 1, General, false,
                 false, Default)
HYP_EXPECT_FIELD("ContactShadow.Thickness", 176, "ContactShadow.ContactV1.Thickness", Pass, Float, 1, 1, General, false,
                 false, Default)
HYP_EXPECT_FIELD("ContactShadow.Bias", 180, "ContactShadow.ContactV1.Bias", Pass, Float, 1, 1, General, false, false,
                 Default)
HYP_EXPECT_FIELD("ContactShadow.MaxSteps", 184, "ContactShadow.ContactV1.MaxSteps", Pass, Uint, 1, 1, General, false,
                 false, Default)
HYP_EXPECT_FIELD("ContactShadow.bReversed", 188, "ContactShadow.ContactV1.bReversed", Pass, Bool, 1, 1, General, false,
                 false, Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(DeferredLightV1, 160, "DeferredLighting")
HYP_EXPECT_FIELD("DeferredLighting.InverseViewProjection", 0, "DeferredLighting.DeferredLightV1.InverseViewProjection",
                 View, Float, 4, 4, General, false, false, Default)
HYP_EXPECT_FIELD("DeferredLighting.Viewport", 64, "DeferredLighting.DeferredLightV1.Viewport", View, Float, 4, 1,
                 General, false, false, Default)
HYP_EXPECT_FIELD("DeferredLighting.DepthRange", 80, "DeferredLighting.DeferredLightV1.DepthRange", View, Float, 2, 1,
                 General, false, false, Default)
HYP_EXPECT_FIELD("DeferredLighting.Eye", 96, "DeferredLighting.DeferredLightV1.Eye", View, Float, 3, 1, General, false,
                 false, Default)
HYP_EXPECT_FIELD("DeferredLighting.LightDirection", 112, "DeferredLighting.DeferredLightV1.LightDirection", Scene,
                 Float, 3, 1, General, false, false, Default)
HYP_EXPECT_FIELD("DeferredLighting.LightColor", 128, "DeferredLighting.DeferredLightV1.LightColor", Scene, Float, 3, 1,
                 General, false, false, Default)
HYP_EXPECT_FIELD("DeferredLighting.Ambient", 144, "DeferredLighting.DeferredLightV1.Ambient", Scene, Float, 3, 1,
                 General, false, false, Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(LocalVolumeV1, 128, "LocalVolume")
HYP_EXPECT_FIELD("LocalVolume.ViewProjection", 0, "DeferredLighting.LocalVolumeV1.ViewProjection", View, Float, 4, 4,
                 General, false, false, Default)
HYP_EXPECT_FIELD("LocalVolume.World", 64, "DeferredLighting.LocalVolumeV1.World", Object, Float, 4, 4, General, false,
                 false, Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(LocalLightV1, 176, "LocalLighting")
HYP_EXPECT_FIELD("LocalLighting.InverseViewProjection", 0, "DeferredLighting.LocalLightV1.InverseViewProjection", View,
                 Float, 4, 4, General, false, false, Default)
HYP_EXPECT_FIELD("LocalLighting.Viewport", 64, "DeferredLighting.LocalLightV1.Viewport", View, Float, 4, 1, General,
                 false, false, Default)
HYP_EXPECT_FIELD("LocalLighting.DepthRange", 80, "DeferredLighting.LocalLightV1.DepthRange", View, Float, 2, 1, General,
                 false, false, Default)
HYP_EXPECT_FIELD("LocalLighting.Eye", 96, "DeferredLighting.LocalLightV1.Eye", View, Float, 3, 1, General, false, false,
                 Default)
HYP_EXPECT_FIELD("LocalLighting.Position", 112, "DeferredLighting.LocalLightV1.Position", Draw, Float, 3, 1, General,
                 false, false, Default)
HYP_EXPECT_FIELD("LocalLighting.Radiance", 128, "DeferredLighting.LocalLightV1.Radiance", Draw, Float, 3, 1, General,
                 false, false, Default)
HYP_EXPECT_FIELD("LocalLighting.Direction", 144, "DeferredLighting.LocalLightV1.Direction", Draw, Float, 3, 1, General,
                 false, false, Default)
HYP_EXPECT_FIELD("LocalLighting.ConeRange", 160, "DeferredLighting.LocalLightV1.ConeRange", Draw, Float, 4, 1, General,
                 false, false, Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(GBufferDebugV1, 16, "GBufferDebug")
HYP_EXPECT_FIELD("GBufferDebug.Mode", 0, "DeferredLighting.GBufferDebugV1.Mode", Pass, Uint, 1, 1, General, false,
                 false, Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(PreviewV1, 32, "Preview")
HYP_EXPECT_FIELD("Preview.Mip", 0, "DepthPreview.PreviewV1.Mip", Pass, Uint, 1, 1, General, false, false, Default)
HYP_EXPECT_FIELD("Preview.bInvert", 4, "DepthPreview.PreviewV1.bInvert", Pass, Bool, 1, 1, General, false, false,
                 Default)
HYP_EXPECT_FIELD("Preview.Viewport", 16, "DepthPreview.PreviewV1.Viewport", View, Float, 4, 1, General, false, false,
                 Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(HZBCopyV1, 48, "HierarchicalCopy")
HYP_EXPECT_FIELD("HierarchicalCopy.Width", 0, "HierarchicalDepth.HZBCopyV1.Width", Pass, Uint, 1, 1, General, false,
                 false, Default)
HYP_EXPECT_FIELD("HierarchicalCopy.Height", 4, "HierarchicalDepth.HZBCopyV1.Height", Pass, Uint, 1, 1, General, false,
                 false, Default)
HYP_EXPECT_FIELD("HierarchicalCopy.DepthRange", 8, "HierarchicalDepth.HZBCopyV1.DepthRange", View, Float, 2, 1, General,
                 false, false, Default)
HYP_EXPECT_FIELD("HierarchicalCopy.Viewport", 16, "HierarchicalDepth.HZBCopyV1.Viewport", View, Float, 4, 1, General,
                 false, false, Default)
HYP_EXPECT_FIELD("HierarchicalCopy.FarDepth", 32, "HierarchicalDepth.HZBCopyV1.FarDepth", Pass, Float, 1, 1, General,
                 false, false, Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(HZBReduceV1, 32, "HierarchicalReduce")
HYP_EXPECT_FIELD("HierarchicalReduce.Width", 0, "HierarchicalDepth.HZBReduceV1.Width", Pass, Uint, 1, 1, General, false,
                 false, Default)
HYP_EXPECT_FIELD("HierarchicalReduce.Height", 4, "HierarchicalDepth.HZBReduceV1.Height", Pass, Uint, 1, 1, General,
                 false, false, Default)
HYP_EXPECT_FIELD("HierarchicalReduce.SourceWidth", 8, "HierarchicalDepth.HZBReduceV1.SourceWidth", Pass, Uint, 1, 1,
                 General, false, false, Default)
HYP_EXPECT_FIELD("HierarchicalReduce.SourceHeight", 12, "HierarchicalDepth.HZBReduceV1.SourceHeight", Pass, Uint, 1, 1,
                 General, false, false, Default)
HYP_EXPECT_FIELD("HierarchicalReduce.bMaximum", 16, "HierarchicalDepth.HZBReduceV1.bMaximum", Pass, Bool, 1, 1, General,
                 false, false, Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(OutlineV1, 16, "Outline")
HYP_EXPECT_FIELD("Outline.Parameters", 0, "Outline.OutlineV1.Parameters", Pass, Float, 4, 1, General, false, false,
                 Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(OutlineColorV1, 16, "OutlineColor")
HYP_EXPECT_FIELD("OutlineColor.Color", 0, "Outline.OutlineColorV1.Color", Pass, Float, 4, 1, General, false, false,
                 Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(OutputV1, 16, "Output")
HYP_EXPECT_FIELD("Output.Exposure", 0, "Output.OutputV1.Exposure", Pass, Float, 1, 1, General, false, false, Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM(SkyViewV1, 112, "Sky")
HYP_EXPECT_FIELD("Sky.InverseSkyViewProjection", 0, "Sky.SkyViewV1.InverseSkyViewProjection", View, Float, 4, 4,
                 General, false, false, Default)
HYP_EXPECT_FIELD("Sky.SkyViewport", 64, "Sky.SkyViewV1.SkyViewport", View, Float, 4, 1, General, false, false, Default)
HYP_EXPECT_FIELD("Sky.SkyRotationIntensity", 80, "Sky.SkyViewV1.SkyRotationIntensity", View, Float, 4, 1, General,
                 false, false, Default)
HYP_EXPECT_FIELD("Sky.SkyTint", 96, "Sky.SkyViewV1.SkyTint", View, Float, 4, 1, General, false, false, Default)
HYP_EXPECT_END()

HYP_EXPECT_UNIFORM_ALIAS("ViewInfo", "HyperionViewV1")
HYP_EXPECT_RESOURCE("ClusterLights", "Engine.View.ClusterLights", View, ReadBuffer, 64, false, false, General, false,
                    false, Default)
HYP_EXPECT_RESOURCE("ClusterHeaders", "Engine.View.ClusterHeaders", View, ReadBuffer, 8, false, false, General, false,
                    false, Default)
HYP_EXPECT_RESOURCE("ClusterIndices", "Engine.View.ClusterIndices", View, ReadBuffer, 4, false, false, General, false,
                    false, Default)
HYP_EXPECT_UNIFORM_ALIAS("ClusterViewInfo", "ClusterViewV1")
HYP_EXPECT_RESOURCE("EnvironmentSpecular", "Engine.Scene.EnvironmentSpecular", Scene, TextureCube, 0, false, false,
                    Environment, true, true, Default)
HYP_EXPECT_RESOURCE("EnvironmentBrdf", "Engine.Scene.EnvironmentBrdf", Scene, Texture2D, 0, false, false, Environment,
                    true, true, Default)
HYP_EXPECT_RESOURCE("EnvironmentSampler", "Engine.Scene.EnvironmentSampler", Scene, Sampler, 0, false, false,
                    Environment, true, true, Default)
HYP_EXPECT_UNIFORM_ALIAS("EnvironmentInfo", "EnvironmentV1")
HYP_EXPECT_RESOURCE("HyperionDirectionalLightsV1", "Engine.Scene.DirectionalLights", Scene, ReadBuffer, 32, false,
                    false, General, true, true, Default)
HYP_EXPECT_UNIFORM_ALIAS("SceneInfo", "HyperionSceneV1")
HYP_EXPECT_RESOURCE("ShadowDepth0", "Engine.View.ShadowDepth0", View, Texture2D, 0, false, false, Shadow, false, false,
                    Default)
HYP_EXPECT_RESOURCE("ShadowDepth1", "Engine.View.ShadowDepth1", View, Texture2D, 0, false, false, Shadow, false, false,
                    Default)
HYP_EXPECT_RESOURCE("ShadowDepth2", "Engine.View.ShadowDepth2", View, Texture2D, 0, false, false, Shadow, false, false,
                    Default)
HYP_EXPECT_RESOURCE("ShadowDepth3", "Engine.View.ShadowDepth3", View, Texture2D, 0, false, false, Shadow, false, false,
                    Default)
HYP_EXPECT_RESOURCE("ShadowSampler", "Engine.View.ShadowSampler", View, Sampler, 0, true, false, Shadow, false, false,
                    Default)
HYP_EXPECT_UNIFORM_ALIAS("ShadowViewInfo", "ShadowViewV1")
HYP_EXPECT_UNIFORM_ALIAS("ObjectInfo", "HyperionObjectV1")
HYP_EXPECT_RESOURCE("BaseColorTexture", "Pbr.BaseColorTexture", Material, Texture2D, 0, false, false, Pbr, false, false,
                    Default)
HYP_EXPECT_RESOURCE("BaseColorSampler", "Pbr.BaseColorSampler", Material, Sampler, 0, false, false, Pbr, false, false,
                    Default)
HYP_EXPECT_RESOURCE("MetallicRoughnessTexture", "Pbr.MetallicRoughnessTexture", Material, Texture2D, 0, false, false,
                    Pbr, false, false, Default)
HYP_EXPECT_RESOURCE("MetallicRoughnessSampler", "Pbr.MetallicRoughnessSampler", Material, Sampler, 0, false, false, Pbr,
                    false, false, Default)
HYP_EXPECT_RESOURCE("NormalTexture", "Pbr.NormalTexture", Material, Texture2D, 0, false, false, Pbr, false, false,
                    Default)
HYP_EXPECT_RESOURCE("NormalSampler", "Pbr.NormalSampler", Material, Sampler, 0, false, false, Pbr, false, false,
                    Default)
HYP_EXPECT_RESOURCE("OcclusionTexture", "Pbr.OcclusionTexture", Material, Texture2D, 0, false, false, Pbr, false, false,
                    Default)
HYP_EXPECT_RESOURCE("OcclusionSampler", "Pbr.OcclusionSampler", Material, Sampler, 0, false, false, Pbr, false, false,
                    Default)
HYP_EXPECT_RESOURCE("EmissiveTexture", "Pbr.EmissiveTexture", Material, Texture2D, 0, false, false, Pbr, false, false,
                    Default)
HYP_EXPECT_RESOURCE("EmissiveSampler", "Pbr.EmissiveSampler", Material, Sampler, 0, false, false, Pbr, false, false,
                    Default)
HYP_EXPECT_UNIFORM_ALIAS("MaterialInfo", "HyperionMaterialV1")
HYP_EXPECT_SEMANTIC_ALIAS("BaseColorTexture", "ALBEDO_TEXTURE")
HYP_EXPECT_SEMANTIC_ALIAS("MetallicRoughnessTexture", "METALLIC_ROUGHNESS_TEXTURE")
HYP_EXPECT_SEMANTIC_ALIAS("NormalTexture", "NORMAL_TEXTURE")
HYP_EXPECT_SEMANTIC_ALIAS("OcclusionTexture", "OCCLUSION_TEXTURE")
HYP_EXPECT_SEMANTIC_ALIAS("EmissiveTexture", "EMISSIVE_TEXTURE")
HYP_EXPECT_RESOURCE("HierarchicalDepth", "Engine.Pass.HierarchicalDepth", Pass, Texture2D, 0, false, false, General,
                    false, false, Default)
HYP_EXPECT_RESOURCE("SurfaceNormals", "Engine.Pass.SurfaceNormals", Pass, Texture2D, 0, false, false, General, false,
                    false, Default)
HYP_EXPECT_RESOURCE("SurfaceCoverage", "Engine.Pass.SurfaceCoverage", Pass, Texture2D, 0, false, false, General, false,
                    false, Default)
HYP_EXPECT_RESOURCE("GBuffer0", "Engine.Pass.GBuffer0", Pass, Texture2D, 0, false, false, General, false, false,
                    Default)
HYP_EXPECT_RESOURCE("GBuffer1", "Engine.Pass.GBuffer1", Pass, Texture2D, 0, false, false, General, false, false,
                    Default)
HYP_EXPECT_RESOURCE("GBuffer2", "Engine.Pass.GBuffer2", Pass, Texture2D, 0, false, false, General, false, false,
                    Default)
HYP_EXPECT_RESOURCE("GBuffer3", "Engine.Pass.GBuffer3", Pass, Texture2D, 0, false, false, General, false, false,
                    Default)
HYP_EXPECT_RESOURCE("SceneDepth", "Engine.Pass.SceneDepth", Pass, Texture2D, 0, false, false, General, false, false,
                    Default)
HYP_EXPECT_RESOURCE("ContactVisibility", "Engine.Pass.ContactVisibility", Pass, Texture2D, 0, false, false, General,
                    false, false, Default)
HYP_EXPECT_RESOURCE("ContactSampler", "Engine.Pass.ContactSampler", Pass, Sampler, 0, false, false, General, false,
                    false, Default)
HYP_EXPECT_RESOURCE("HyperionPreviewSource", "Engine.Pass.PreviewSource", Pass, Texture2D, 0, false, false, General,
                    false, false, Default)
HYP_EXPECT_RESOURCE("DepthMap", "Engine.Pass.DepthPreviewMap", Pass, Texture2D, 0, false, false, General, false, false,
                    Default)
HYP_EXPECT_RESOURCE("SourceDepth", "Engine.Pass.DepthSource", Pass, Texture2D, 0, false, false, General, false, false,
                    Default)
HYP_EXPECT_RESOURCE("OutputDepth", "Engine.Pass.DepthOutput", Pass, Texture2D, 0, false, true, General, false, false,
                    Default)
HYP_EXPECT_RESOURCE("ObjectMask", "Engine.Pass.OutlineObjectMask", Pass, Texture2D, 0, false, false, General, false,
                    false, Default)
HYP_EXPECT_RESOURCE("OutlineMask", "Engine.Pass.OutlineMask", Pass, Texture2D, 0, false, false, General, false, false,
                    Default)
HYP_EXPECT_RESOURCE("SceneColor", "Engine.Pass.SceneColor", Pass, Texture2D, 0, false, false, General, false, false,
                    Default)
HYP_EXPECT_RESOURCE("SkyRadiance", "Engine.Pass.SkyRadiance", Pass, TextureCube, 0, false, false, General, false, false,
                    Default)
HYP_EXPECT_RESOURCE("SkySampler", "Engine.Pass.SkySampler", Pass, Sampler, 0, false, false, General, false, false,
                    Default)
